#include "controller/ModelListHandler.h"

#include <muduo/base/Logging.h>

#include <fstream>
#include <sstream>
#include <sys/stat.h>

void ModelListHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
  static std::string cached_json;
  static time_t cached_mtime = 0;

  struct stat st;
  if (stat("models.json", &st) == 0 && st.st_mtime > cached_mtime)
  {
    std::ifstream f("models.json");
    if (f.is_open())
    {
      std::ostringstream ss;
      ss << f.rdbuf();
      cached_json = ss.str();
      cached_mtime = st.st_mtime;
      LOG_INFO << "models.json changed, reloaded from disk";
    }
  }

  // 首次加载或文件丢失时使用内置兜底
  if (cached_json.empty())
  {
    cached_json = R"([{"provider":"aliyun","provider_name":"阿里云百炼","models":[{"id":"qwen-plus","name":"通义千问 Plus"},{"id":"qwen-max","name":"通义千问 Max"}]},{"provider":"volcengine","provider_name":"字节火山引擎","models":[{"id":"doubao-lite-4k","name":"豆包 Lite"},{"id":"doubao-pro-32k","name":"豆包 Pro"},{"id":"doubao-seed-2-0-pro-260215","name":"豆包 Seed 2.0 Pro"}]}])";
    cached_mtime = 1;
  }

  std::string body = R"({"success":true,"models":)" + cached_json + "}";
  resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
  resp->setCloseConnection(false);
  resp->setContentType("application/json");
  resp->setContentLength(body.size());
  resp->setBody(body);
}