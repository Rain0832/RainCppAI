#pragma once
#include <atomic>
#include <string>
namespace common {
class MetricsCollector {
public:
    static void incHttpReq(const std::string& method, const std::string& path);
    static std::string dumpHttp();
    static long long uptimeSec();
};
}