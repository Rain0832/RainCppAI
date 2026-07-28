#include "controller/MetricsHandler.h"

#include <sstream>

#include "Common/Metrics/MetricsCollector.h"
#include "storage/MysqlUtil.h"
void MetricsHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::ostringstream out;
    out << common::MetricsCollector::dumpHttp();
    out << "# HELP llm_calls_total Total LLM API calls\n";
    out << "# TYPE llm_calls_total counter\n";
    try
    {
        storage::MysqlUtil mu;
        auto r = mu.executeQuery(
            "SELECT COALESCE(SUM(CASE WHEN status='success' THEN 1 ELSE 0 END),0) AS ok,"
            "COALESCE(SUM(CASE WHEN status='error' THEN 1 ELSE 0 END),0) AS err,"
            "COALESCE(SUM(duration_ms),0) AS dur FROM call_logs");
        if (r && r->next())
        {
            out << "llm_calls_total{status=\"success\"} " << r->getInt64("ok") << "\n";
            out << "llm_calls_total{status=\"error\"} " << r->getInt64("err") << "\n";
            out << "llm_duration_ms_total " << r->getInt64("dur") << "\n";
        }
    }
    catch (...)
    {
    }
    std::string body = out.str();
    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("text/plain; version=0.0.4; charset=utf-8");
    resp->setContentLength(body.size());
    resp->setBody(body);
}