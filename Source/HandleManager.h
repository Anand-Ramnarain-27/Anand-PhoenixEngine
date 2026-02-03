#pragma once

#include "ModuleD3D12.h"
#include <vector>
#include <cassert>

class HandleManager
{
public:
    HandleManager(size_t maxHandles) : maxHandles(maxHandles), freeIndices(maxHandles)
    {
        for (size_t i = 0; i < maxHandles; ++i)
        {
            freeIndices[i] = static_cast<UINT>(maxHandles - i - 1);
        }
    }

    UINT allocHandle()
    {
        if (freeIndices.empty())
            return 0;

        UINT handle = freeIndices.back();
        freeIndices.pop_back();

        UINT generation = ++generations[handle];
        return (generation << 24) | (handle + 1); 
    }

    void freeHandle(UINT handle)
    {
        if (handle == 0) return;

        UINT index = indexFromHandle(handle);
        if (index < maxHandles)
        {
            freeIndices.push_back(index);
        }
    }

    bool validHandle(UINT handle) const
    {
        if (handle == 0) return false;

        UINT index = indexFromHandle(handle);
        if (index >= maxHandles) return false;

        UINT generation = generationFromHandle(handle);
        return generations[index] == generation;
    }

    UINT indexFromHandle(UINT handle) const
    {
        if (handle == 0) return 0;
        return ((handle & 0x00FFFFFF) - 1);
    }

    size_t getFreeCount() const { return freeIndices.size(); }
    size_t getSize() const { return maxHandles; }

private:
    UINT generationFromHandle(UINT handle) const
    {
        return handle >> 24;
    }

    size_t maxHandles;
    std::vector<UINT> freeIndices;
    std::vector<UINT> generations; 
};