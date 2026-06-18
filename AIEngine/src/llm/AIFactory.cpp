#include "llm/AIFactory.h"

#include <muduo/base/Logging.h>

StrategyFactory &StrategyFactory::instance()
{
    static StrategyFactory factory;
    return factory;
}

void StrategyFactory::registerStrategy(const std::string &name, Creator creator)
{
    creators[name] = std::move(creator);
    LOG_INFO << "[StrategyFactory] Registered provider: " << name;
}

std::shared_ptr<AIStrategy> StrategyFactory::create(const std::string &name)
{
    LOG_INFO << "[StrategyFactory] Creating strategy for provider: " << name;
    auto it = creators.find(name);
    if (it == creators.end())
    {
        LOG_ERROR << "[StrategyFactory] Unknown provider: " << name << ", falling back to 'aliyun'";
        it = creators.find("aliyun");
        if (it == creators.end())
            throw std::runtime_error("Unknown strategy: " + name + " (fallback also failed)");
    }
    LOG_INFO << "[StrategyFactory] Strategy matched successfully for provider: " << name;
    return it->second();
}
