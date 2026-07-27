#include "controller/HealthHandler.h"
#include "Common/Metrics/MetricsCollector.h"
#include "storage/MysqlUtil.h"
void HealthHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp) {
    bool dbOk = false;
    try { storage::MysqlUtil mu; auto r = mu.executeQuery("SELECT 1"); dbOk = (r && r->next()); } catch (...) {}
    json j;
    j["status"] = dbOk ? "up" : "degraded";
    j["db"] = dbOk ? "connected" : "disconnected";
    j["uptime_sec"] = common::MetricsCollector::uptimeSec();
    std::string body = j.dump();
    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("application/json");
    resp->setContentLength(body.size());
    resp->setBody(body);
}