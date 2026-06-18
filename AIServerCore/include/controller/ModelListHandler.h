#pragma once
#include "HttpServer/include/router/RouterHandler.h"
#include "server/ChatServer.h"

/**
 * @brief 模型列表 Handler
 *
 * 路由：GET /api/chat/models
 * 返回厂商-模型双层注册表 JSON，供前端动态渲染模型选择下拉框。
 */
class ModelListHandler : public http::router::RouterHandler
{
public:
  explicit ModelListHandler(ChatServer *server) : server_(server) {}

  void handle(const http::HttpRequest &req, http::HttpResponse *resp) override;

private:
  ChatServer *server_;
};