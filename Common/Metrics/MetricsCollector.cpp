#include "Common/Metrics/MetricsCollector.h"
#include <chrono>
#include <sstream>
namespace common {
static std::atomic<long long> g_reqCnt{0};
static long long g_start = std::chrono::duration_cast<std::chrono::seconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
void MetricsCollector::incHttpReq(const std::string&, const std::string&) { g_reqCnt.fetch_add(1); }
std::string MetricsCollector::dumpHttp() {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::ostringstream ss;
    ss << "# HELP http_requests_total Total HTTP requests\n";
    ss << "# TYPE http_requests_total counter\n";
    ss << "http_requests_total " << g_reqCnt.load() << "\n";
    ss << "# HELP uptime_seconds Server uptime\n";
    ss << "# TYPE uptime_seconds gauge\n";
    ss << "uptime_seconds " << (now - g_start) << "\n";
    return ss.str();
}
long long MetricsCollector::uptimeSec() {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return now - g_start;
}
}