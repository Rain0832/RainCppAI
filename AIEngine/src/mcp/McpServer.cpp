#include "mcp/McpServer.h"

McpServer::McpServer()
{
    // 从 AIToolRegistry 单例同步元信息（不再硬编码）
    syncMetasFromRegistry();
}

void McpServer::syncMetasFromRegistry()
{
    toolMetas_.clear();
    auto& registry = AIToolRegistry::instance();
    json toolsSchema = registry.getToolsSchema();
    for (auto& toolDef : toolsSchema)
    {
        auto& func = toolDef["function"];
        ToolMeta meta;
        meta.name = func.value("name", "");
        meta.description = func.value("description", "");
        meta.inputSchema = func.value("parameters", json::object());
        toolMetas_.push_back(std::move(meta));
    }
}

std::string McpServer::handleRequest(const std::string& requestBody)
{
    json id = nullptr;
    try
    {
        json req = json::parse(requestBody);
        id = req.value("id", json(nullptr));

        // 验证 JSON-RPC 2.0
        if (!req.contains("jsonrpc") || req["jsonrpc"] != "2.0")
        {
            return buildError(-32600, "Invalid Request").dump();
        }

        std::string method = req.value("method", "");
        json params = req.value("params", json::object());

        if (method == "tools/list")
        {
            return buildResult(id, handleToolsList(params)).dump();
        }
        else if (method == "tools/call")
        {
            return buildResult(id, handleToolsCall(params)).dump();
        }
        else
        {
            json err = buildError(-32601, "Method not found: " + method);
            err["id"] = id;
            return err.dump();
        }
    }
    catch (const std::exception& e)
    {
        json err = buildError(-32700, std::string("Parse error: ") + e.what());
        err["id"] = id;
        return err.dump();
    }
}

json McpServer::handleToolsList(const json&)
{
    json tools = json::array();
    for (auto& meta : toolMetas_)
    {
        json t;
        t["name"] = meta.name;
        t["description"] = meta.description;
        t["inputSchema"] = meta.inputSchema;
        tools.push_back(t);
    }
    return {{"tools", tools}};
}

json McpServer::handleToolsCall(const json& params)
{
    if (!params.contains("name"))
    {
        throw std::runtime_error("Missing 'name' in tools/call params");
    }
    std::string name = params["name"];
    json args = params.value("arguments", json::object());

    json result = AIToolRegistry::instance().invoke(name, args);
    return {{"content", json::array({{{"type", "text"}, {"text", result.dump()}}})}, {"isError", false}};
}

json McpServer::buildError(int code, const std::string& message)
{
    return {{"jsonrpc", "2.0"}, {"error", {{"code", code}, {"message", message}}}};
}

json McpServer::buildResult(const json& id, const json& result)
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}