#pragma once
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <random>
#include <string>

class AISessionIdGenerator
{
public:
    AISessionIdGenerator() { std::srand(static_cast<unsigned>(std::time(nullptr))); }

    std::string generate();
};
