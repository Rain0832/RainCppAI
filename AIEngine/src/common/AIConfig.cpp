#include "common/AIConfig.h"

bool AIConfig::loadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[AIConfig] Cannot open: " << path << std::endl;
        return false;
    }
    file >> json_;
    return true;
}