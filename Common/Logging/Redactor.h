#pragma once
#include <string>

namespace common
{

class Redactor
{
public:
    static std::string mask(const std::string& text);
};

}  // namespace common