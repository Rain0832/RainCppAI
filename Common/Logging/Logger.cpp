#include "Common/Logging/Logger.h"

#include <regex>

namespace common {

void Logger::init(const std::string& log_level, const std::string& log_path)
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_path, 0, 0, false, 7);

    // Console: colored human-readable
    console_sink->set_pattern("%Y-%m-%d %H:%M:%S %^%l%$ %v");
    console_sink->set_level(spdlog::level::trace);

    // File: raw (JSON already formatted by LogStream)
    file_sink->set_pattern("%v");
    file_sink->set_level(spdlog::level::trace);

    auto logger = std::make_shared<spdlog::logger>("rain",
        spdlog::sinks_init_list{console_sink, file_sink});

    spdlog::level::level_enum lv = spdlog::level::from_str(log_level);
    logger->set_level(lv);

    spdlog::set_default_logger(logger);
    SPDLOG_INFO("Logger initialized: level={}, path={}", log_level, log_path);
}

std::shared_ptr<spdlog::logger> Logger::instance()
{
    return spdlog::get("rain");
}

// ---------------------------------------------------------------------------

LogStream::~LogStream()
{
    std::string raw = ss_.str();
    if (raw.empty()) return;

    std::string tag = tag_;
    std::string msg = raw;

    // Auto-detect [TAG] prefix
    if (tag.empty() && raw.size() > 2 && raw[0] == '[')
    {
        auto end = raw.find(']');
        if (end != std::string::npos && end < 20)
        {
            tag = raw.substr(1, end - 1);
            msg = raw.substr(end + 1);
            if (!msg.empty() && msg[0] == ' ')
                msg = msg.substr(1);
        }
    }

    // Build JSON for file sink
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&now_c, &tm);
    char time_buf[32];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm);

    std::string req_id = tls_log_ctx.req_id;
    std::string uid    = tls_log_ctx.user_id;

    std::string json = fmt::format(
        R"({{"time":"{}","level":"{}","module":"{}","req_id":"{}","uid":"{}","file":"{}:{}","msg":"{}"}})",
        time_buf,
        spdlog::level::to_short_c_str(lv_),
        tag.empty() ? "-" : tag,
        req_id.empty() ? "-" : req_id,
        uid.empty()    ? "-" : uid,
        file_ ? file_ : "?",
        line_,
        msg
    );

    auto logger = spdlog::get("rain");
    if (!logger) return;

    // Console sink [0]: human-readable with [TAG]
    // File sink   [1]: JSON
    auto& sinks = logger->sinks();
    if (sinks.size() > 0)
    {
        std::string console_msg = tag.empty() ? msg : "[" + tag + "] " + msg;
        sinks[0]->log(spdlog::details::log_msg{
            spdlog::source_loc{file_, line_, ""},
            logger->name(), lv_,
            spdlog::string_view_t{console_msg.data(), console_msg.size()}
        });
        sinks[0]->flush();
    }
    if (sinks.size() > 1)
    {
        sinks[1]->log(spdlog::details::log_msg{
            spdlog::source_loc{file_, line_, ""},
            logger->name(), lv_,
            spdlog::string_view_t{json.data(), json.size()}
        });
        sinks[1]->flush();
    }
}

} // namespace common
