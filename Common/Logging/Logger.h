#pragma once

#include <memory>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>

#include "Common/Logging/LogContext.h"

namespace common {

/// Initialise spdlog with dual sinks:
///   - stdout: colored, human-readable (keeps [TAG] style)
///   - file:   JSON format, daily rolling, 7-day retention
class Logger {
public:
    /// Must be called once at server startup.
    /// @param log_level  "trace","debug","info","warn","error"
    /// @param log_path   Base path for log file (e.g. "logs/app")
    static void init(const std::string& log_level = "info",
                     const std::string& log_path = "logs/app");

    /// Get the spdlog logger instance (named "rain").
    static std::shared_ptr<spdlog::logger> instance();
};

// ---------------------------------------------------------------------------
// LogStream — stream-style log builder with auto [TAG] detection
// ---------------------------------------------------------------------------

class LogStream {
public:
    LogStream(const char* file, int line, spdlog::level::level_enum lv,
              const std::string& tag = "")
        : lv_(lv), tag_(tag), file_(file), line_(line) {}

    ~LogStream();

    std::ostringstream& stream() { return ss_; }

private:
    std::ostringstream ss_;
    spdlog::level::level_enum lv_;
    std::string tag_;
    const char* file_;
    int line_;
};

} // namespace common

// ---------------------------------------------------------------------------
// Convenience macros
// ---------------------------------------------------------------------------

#define SPDLOG_INFO_TAG(tag)  common::LogStream(__FILE__, __LINE__, spdlog::level::info, tag).stream()
#define SPDLOG_WARN_TAG(tag)  common::LogStream(__FILE__, __LINE__, spdlog::level::warn, tag).stream()
#define SPDLOG_ERROR_TAG(tag) common::LogStream(__FILE__, __LINE__, spdlog::level::err,  tag).stream()
