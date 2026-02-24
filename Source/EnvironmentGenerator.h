#pragma once
#include <memory>
#include <string>
#include "EnvironmentMap.h"

class EnvironmentGenerator
{
public:
    std::unique_ptr<EnvironmentMap> LoadCubemap(const std::string& file);
};