#include "controller/ModelListHandler.h"

void ModelListHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
  json models = json::array();

  // 阿里云百炼
  {
    json entry;
    entry["provider"] = "aliyun";
    entry["provider_name"] = "阿里云百炼";
    entry["models"] = json::array();
    entry["models"].push_back({{"id", "qwen-plus"}, {"name", "通义千问 Plus"}});
    entry["models"].push_back({{"id", "qwen-max"}, {"name", "通义千问 Max"}});
    models.push_back(std::move(entry));
  }

  // 字节火山引擎
  {
    json entry;
    entry["provider"] = "volcengine";
    entry["provider_name"] = "字节火山引擎";
    entry["models"] = json::array();
    entry["models"].push_back({{"id", "doubao-lite-4k"}, {"name", "豆包 Lite"}});
    entry["models"].push_back({{"id", "doubao-pro-32k"}, {"name", "豆包 Pro"}});
    entry["models"].push_back({{"id", "doubao-seed-2-0-pro-260215"}, {"name", "豆包 Seed 2.0 Pro"}});
    models.push_back(std::move(entry));
  }

  json body;
  body["success"] = true;
  body["models"] = std::move(models);

  std::string bodyStr = body.dump();
  resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
  resp->setCloseConnection(false);
  resp->setContentType("application/json");
  resp->setContentLength(bodyStr.size());
  resp->setBody(bodyStr);
}