#include "controller/AdminLogsHandler.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include "Common/Logging/Logger.h"

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

std::string AdminLogsHandler::levelColor(const std::string& line)
{
    // spdlog format: [2026-07-29 12:34:56.789] [TAG] [LEVEL] message
    // Also handle uppercase LEVEL and SPDLOG_* tags
    if (line.find("[error]") != std::string::npos || line.find("[ERROR]") != std::string::npos) return "#f44336";
    if (line.find("[warn]") != std::string::npos || line.find("[WARN]") != std::string::npos) return "#ff9800";
    if (line.find("[debug]") != std::string::npos || line.find("[DEBUG]") != std::string::npos) return "#78909c";
    return "#4caf50";
}

std::string AdminLogsHandler::readLastLines(const std::string& path, int maxLines)
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

    std::ostringstream out;
    for (const auto& l : lines)
    {
        out << "<div class=\"log-line\" style=\"color:" << levelColor(l) << "\">" << htmlEscape(l) << "</div>\n";
    }
    return out.str();
}

void AdminLogsHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string logPath = "../logs/app-" + todayStr() + ".log";
    SPDLOG_INFO_TAG("ADMIN") << "Loading logs from: " << logPath;

    std::string logHtml = readLastLines(logPath, 200);

    std::string infoLine;
    if (logHtml.empty())
    {
        infoLine = "File: <code>" + htmlEscape(logPath) + "</code> &nbsp;|&nbsp; File not found";
        logHtml =
            "<div class=\"log-line\" style=\"color:#f44336\">Log file not found: " + htmlEscape(logPath) + "</div>";
    }
    else
    {
        infoLine = "File: <code>" + htmlEscape(logPath) + "</code> &nbsp;|&nbsp; Last 200 lines";
    }

    // Read template HTML
    std::string templatePath = server_->getResourceRoot() + "web/admin/logs.html";
    std::ifstream file(templatePath);
    if (!file.is_open())
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Cannot open logs template: " << templatePath;
        resp->setStatusCode(http::HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("text/plain");
        std::string body = "Logs template not found";
        resp->setContentLength(body.size());
        resp->setBody(body);
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string html = ss.str();

    // Replace placeholders
    size_t pos = html.find("__LOG_INFO__");
    if (pos != std::string::npos) html.replace(pos, 13, infoLine);

    pos = html.find("__LOG_LINES__");
    if (pos != std::string::npos) html.replace(pos, 14, logHtml);

    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/html; charset=utf-8");
    resp->setContentLength(html.size());
    resp->setBody(html);
}
