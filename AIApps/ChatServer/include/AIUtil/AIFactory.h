#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>

#include "AIStrategy.h"

class StrategyFactory
{

public:
    using Creator = std::function<std::shared_ptr<AIStrategy>()>;

    // 获取策略工厂单例。
    //
    // Returns:
    //   StrategyFactory 单例引用。
    static StrategyFactory &instance();

    // 注册策略构造器。
    //
    // Args:
    //   name: 策略名称。
    //   creator: 用于创建策略实例的回调。
    void registerStrategy(const std::string &name, Creator creator);

    // 创建指定名称的策略实例。
    //
    // Args:
    //   name: 策略名称。
    //
    // Returns:
    //   对应策略的智能指针。
    //
    // Throws:
    //   std::runtime_error: 当策略名称不存在时抛出。
    std::shared_ptr<AIStrategy> create(const std::string &name);

private:
    StrategyFactory() = default;
    std::unordered_map<std::string, Creator> creators;
};

template <typename T>
struct StrategyRegister
{
    // 构造函数，注册指定类型的策略。
    //
    // Args:
    //   name: 策略名称。
    StrategyRegister(const std::string &name)
    {
        StrategyFactory::instance().registerStrategy(name, []
                                                     {
            std::shared_ptr<AIStrategy> instance = std::make_shared<T>();
            return instance; });
    }
};
