#include "../include/AIUtil/McpServer.h"

McpServer::McpServer()
{
    // 注册内置工具（复用 AIToolRegistry 的实现，补充元信息）
    toolMetas_.push_back({
        "get_weather",
        "获取指定城市的实时天气信息",
        {{"type","object"},{"properties",{{"city",{{"type","string"},{"description","城市名称"}}}}},{"required",json::array({"city"})}}
    });
    toolMetas_.push_back({
        "get_time",
        "获取当前服务器时间",
        {{"type","object"},{"properties",json::object()},{"required",json::array()}}
    });
}

void McpServer::registerTool(const std::string &name,
                              const std::string &description,
                              const json &inputSchema,
                              AIToolRegistry::ToolFunc func)
{
    registry_.registerTool(name, func);
    toolMetas_.push_back({name, description, inputSchema});
}

std::string McpServer::handleRequest(const std::string &requestBody)
{
    json id = nullptr;
    try {
        json req = json::parse(requestBody);
        id = req.value("id", json(nullptr));

        // 验证 JSON-RPC 2.0
        if (!req.contains("jsonrpc") || req["jsonrpc"] != "2.0") {
            return buildError(-32600, "Invalid Request").dump();
        }

        std::string method = req.value("method", "");
        json params = req.value("params", json::object());

        if (method == "tools/list") {
            return buildResult(id, handleToolsList(params)).dump();
        } else if (method == "tools/call") {
            return buildResult(id, handleToolsCall(params)).dump();
        } else {
            json err = buildError(-32601, "Method not found: " + method);
            err["id"] = id;
            return err.dump();
        }
    } catch (const std::exception &e) {
        json err = buildError(-32700, std::string("Parse error: ") + e.what());
        err["id"] = id;
        return err.dump();
    }
}

json McpServer::handleToolsList(const json &)
{
    json tools = json::array();
    for (auto &meta : toolMetas_) {
        json t;
        t["name"]        = meta.name;
        t["description"] = meta.description;
        t["inputSchema"] = meta.inputSchema;
        tools.push_back(t);
    }
    return {{"tools", tools}};
}

json McpServer::handleToolsCall(const json &params)
{
    if (!params.contains("name")) {
        throw std::runtime_error("Missing 'name' in tools/call params");
    }
    std::string name = params["name"];
    json args = params.value("arguments", json::object());

    if (!registry_.hasTool(name)) {
        throw std::runtime_error("Tool not found: " + name);
    }

    json result = registry_.invoke(name, args);
    return {
        {"content", json::array({{{"type","text"},{"text", result.dump()}}})},
        {"isError", false}
    };
}

json McpServer::buildError(int code, const std::string &message)
{
    return {
        {"jsonrpc", "2.0"},
        {"error", {{"code", code}, {"message", message}}}
    };
}

json McpServer::buildResult(const json &id, const json &result)
{
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
}
