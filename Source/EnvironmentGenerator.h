#pragma once 

#include <memory>
#include <string>
#include "IBLGenerator.h"
#include "EnvironmentMap.h"

class EnvironmentGenerator
{
public: 
    std::unique_ptr<EnvironmentMap> loadCubemap(const std::string& file);

private:
    IBLGenerator m_iblGenerator;
};