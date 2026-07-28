#include "llm/AIFactory.h"

#include "Common/Logging/Logger.h"

StrategyFactory& StrategyFactory::instance()
{
    static StrategyFactory factory;
    return factory;
}

void StrategyFactory::registerStrategy(const std::string& name, Creator creator)
{
    creators[name] = std::move(creator);
    SPDLOG_INFO_TAG("AI") << "[StrategyFactory] Registered provider: " << name;
}

std::shared_ptr<AIStrategy> StrategyFactory::create(const std::string& name)
{
    SPDLOG_INFO_TAG("AI") << "[StrategyFactory] Creating strategy for provider: " << name;
    auto it = creators.find(name);
    if (it == creators.end())
    {
        SPDLOG_ERROR_TAG("AI") << "[StrategyFactory] Unknown provider: " << name << ", falling back to 'aliyun'";
        it = creators.find("aliyun");
        if (it == creators.end()) throw std::runtime_error("Unknown strategy: " + name + " (fallback also failed)");
    }
    SPDLOG_INFO_TAG("AI") << "[StrategyFactory] Strategy matched successfully for provider: " << name;
    return it->second();
}
