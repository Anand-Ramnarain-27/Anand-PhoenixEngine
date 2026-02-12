#pragma once

#include "Mesh.h"
#include <vector>
#include <memory>
#include <string>

class Model
{
public:

    bool load(const char* fileName);
    void draw(ID3D12GraphicsCommandList* cmdList);

private:
    bool loadFromLibrary(const std::string& folder);

private:

    std::string m_srcFile;
    std::vector<std::unique_ptr<Mesh>> m_meshes;
};
