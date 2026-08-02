#include "controller/AdminLogsHandler.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"
#include "HttpServer/include/utils/FileUtil.h"

static std::string todayStr()
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

std::string AdminLogsHandler::htmlEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            default:
                out += c;
        }
    }
    return out;
}

std::string AdminLogsHandler::levelClass(const std::string& line)
{
    if (line.find("[error]") != std::string::npos || line.find("[ERROR]") != std::string::npos) return "error";
    if (line.find("[warn]") != std::string::npos || line.find("[WARN]") != std::string::npos) return "warn";
    if (line.find("[debug]") != std::string::npos || line.find("[DEBUG]") != std::string::npos) return "debug";
    return "info";
}

std::vector<std::string> AdminLogsHandler::readLastLines(const std::string& path, int maxLines)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};

    constexpr size_t kChunk = 4096;
    std::vector<std::string> lines;
    std::string leftover;
    std::streamoff size = file.tellg();

    while (size > 0 && static_cast<int>(lines.size()) < maxLines)
    {
        std::streamoff readSize = std::min<std::streamoff>(kChunk, size);
        size -= readSize;
        file.seekg(size, std::ios::beg);

        std::vector<char> buf(readSize + 1);
        file.read(buf.data(), readSize);
        buf[readSize] = '\0';
        std::string chunk(buf.data(), readSize);
        chunk += leftover;

        std::vector<std::string> revLines;
        size_t pos = chunk.size();
        while (pos > 0)
        {
            size_t nl = chunk.rfind('\n', pos - 1);
            if (nl == std::string::npos)
            {
                leftover = chunk.substr(0, pos);
                break;
            }
            std::string l = chunk.substr(nl + 1, pos - nl - 1);
            if (!l.empty()) revLines.push_back(std::move(l));
            pos = nl;
            if (!revLines.empty() && !revLines.back().empty() && revLines.back().back() == '\r')
                revLines.back().pop_back();
            if (static_cast<int>(revLines.size() + lines.size()) >= maxLines) break;
        }
        for (auto it = revLines.rbegin(); it != revLines.rend(); ++it) lines.insert(lines.begin(), std::move(*it));
        if (pos == 0 && !chunk.empty() && chunk[0] != '\n') lines.insert(lines.begin(), std::move(leftover));
    }
    while (static_cast<int>(lines.size()) > maxLines) lines.erase(lines.begin());
    return lines;
}

std::string AdminLogsHandler::readLastLinesHtml(const std::string& path, int maxLines)
{
    auto lines = readLastLines(path, maxLines);
    if (lines.empty()) return {};

    std::ostringstream out;
    static const std::map<std::string, std::string> kColors = {
        {"error", "#f44336"}, {"warn", "#ff9800"}, {"debug", "#78909c"}, {"info", "#4caf50"}};
    for (const auto& l : lines)
    {
        std::string lvl = levelClass(l);
        out << "<div class=\"log-line log-" << lvl << "\" style=\"color:" << kColors.at(lvl) << "\">" << htmlEscape(l)
            << "</div>\n";
    }
    return out.str();
}

void AdminLogsHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string dateStr = req.getQueryParameters("date");
    if (dateStr.size() != 10) dateStr = todayStr();

    std::string logPath = "logs/app_" + dateStr + ".log";
    SPDLOG_INFO_TAG("ADMIN") << "Loading logs from: " << logPath;

    auto lines = readLastLines(logPath, 200);
    std::string fmt = req.getQueryParameters("format");

    // ── JSON API mode (for dashboard tab) ──
    if (fmt == "json")
    {
        json result = json::object();
        result["path"] = logPath;
        json items = json::array();
        for (const auto& l : lines)
        {
            json item;
            item["text"] = l;
            item["level"] = levelClass(l);
            items.push_back(item);
        }
        result["lines"] = items;
        std::string body = common::ApiResult::ok(result).toJson().dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(body.size());
        resp->setBody(body);
        return;
    }

    // ── SSR mode (standalone page, legacy) ──
    std::string logHtml = readLastLinesHtml(logPath, 200);

    std::string infoLine;
    if (logHtml.empty())
    {
        infoLine = htmlEscape(logPath) + " — 文件不存在或无日志";
        logHtml =
            "<div class=\"log-line log-error\" style=\"color:#f44336\">📭 日志文件不存在: " + htmlEscape(logPath) +
            "</div>";
    }
    else
    {
        infoLine = htmlEscape(logPath);
    }

    std::string templatePath = server_->getResourceRoot() + "web/admin/logs.html";
    FileUtil fileOperater(templatePath);
    if (!fileOperater.isValid())
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Cannot open logs template: " << templatePath;
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k404NotFound, "Not Found");
        resp->setCloseConnection(true);
        resp->setContentType("text/plain");
        std::string body = "Logs template not found";
        resp->setContentLength(body.size());
        resp->setBody(body);
        return;
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string html = std::string(buffer.data(), buffer.size());

    size_t pos;
    pos = html.find("__DATE__");
    if (pos != std::string::npos) html.replace(pos, 8, dateStr);
    pos = html.find("__FILE_INFO__");
    if (pos != std::string::npos) html.replace(pos, 13, infoLine);
    pos = html.find("__LOG_LINES__");
    if (pos != std::string::npos) html.replace(pos, 14, logHtml);

    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("text/html; charset=utf-8");
    resp->setContentLength(html.size());
    resp->setBody(html);
}
