#pragma once

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <array>

namespace GLTFUtils
{
    inline bool LoadAccessorData(
        uint8_t* data,
        size_t elemSize,
        size_t stride,
        size_t count,
        const tinygltf::Model& model,
        int accessorIndex)
    {
        if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= model.accessors.size())
            return false;

        const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
        size_t expectedElemSize = tinygltf::GetComponentSizeInBytes(accessor.componentType) *
            tinygltf::GetNumComponentsInType(accessor.type);

        if (count != accessor.count || elemSize != expectedElemSize)
            return false;

        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const uint8_t* sourceData = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        size_t sourceStride = (bufferView.byteStride == 0) ? expectedElemSize : bufferView.byteStride;

        for (size_t i = 0; i < count; ++i)
        {
            memcpy(data, sourceData, elemSize);
            data += stride;
            sourceData += sourceStride;
        }

        return true;
    }

    inline bool LoadAccessorData(
        uint8_t* data,
        size_t elemSize,
        size_t stride,
        size_t count,
        const tinygltf::Model& model,
        const std::map<std::string, int>& attributes,
        const char* attributeName)
    {
        auto it = attributes.find(attributeName);
        if (it != attributes.end())
        {
            return LoadAccessorData(data, elemSize, stride, count, model, it->second);
        }
        return false;
    }

    template <typename T>
    inline bool LoadAccessorTyped(
        std::unique_ptr<T[]>& data,
        uint32_t& count,
        const tinygltf::Model& model,
        int accessorIndex)
    {
        if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= model.accessors.size())
            return false;

        const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
        count = static_cast<uint32_t>(accessor.count);
        data = std::make_unique<T[]>(count);

        return LoadAccessorData(
            reinterpret_cast<uint8_t*>(data.get()),
            sizeof(T),
            sizeof(T),
            count,
            model,
            accessorIndex
        );
    }

    template <typename T>
    inline bool LoadAccessorTyped(
        std::unique_ptr<T[]>& data,
        uint32_t& count,
        const tinygltf::Model& model,
        const std::map<std::string, int>& attributes,
        const char* attributeName)
    {
        auto it = attributes.find(attributeName);
        if (it != attributes.end())
        {
            return LoadAccessorTyped(data, count, model, it->second);
        }

        count = 0;
        return false;
    }

    enum class IndexFormat
    {
        UINT8,
        UINT16,
        UINT32
    };

    inline bool LoadIndices(
        std::unique_ptr<uint8_t[]>& indices,
        uint32_t& count,
        IndexFormat& format,
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive)
    {
        if (primitive.indices < 0)
        {
            count = 0;
            return false;
        }

        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        count = static_cast<uint32_t>(indexAccessor.count);
        size_t elementSize = tinygltf::GetComponentSizeInBytes(indexAccessor.componentType);

        switch (indexAccessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            format = IndexFormat::UINT8;
            elementSize = 1;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            format = IndexFormat::UINT16;
            elementSize = 2;
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            format = IndexFormat::UINT32;
            elementSize = 4;
            break;
        default:
            return false;
        }

        indices = std::make_unique<uint8_t[]>(count * elementSize);

        return LoadAccessorData(
            indices.get(),
            elementSize,
            elementSize,
            count,
            model,
            primitive.indices
        );
    }

    struct MaterialData
    {
        float baseColour[4];
        bool hasColourTexture;
        uint8_t padding[3];
    };

    inline bool LoadMaterialData(
        MaterialData& materialData,
        const tinygltf::Model& model,
        int materialIndex)
    {
        if (materialIndex < 0 || static_cast<size_t>(materialIndex) >= model.materials.size())
            return false;

        const tinygltf::Material& material = model.materials[materialIndex];
        const tinygltf::PbrMetallicRoughness& pbr = material.pbrMetallicRoughness;

        if (pbr.baseColorFactor.size() >= 4)
        {
            materialData.baseColour[0] = static_cast<float>(pbr.baseColorFactor[0]);
            materialData.baseColour[1] = static_cast<float>(pbr.baseColorFactor[1]);
            materialData.baseColour[2] = static_cast<float>(pbr.baseColorFactor[2]);
            materialData.baseColour[3] = static_cast<float>(pbr.baseColorFactor[3]);
        }
        else
        {
            materialData.baseColour[0] = 1.0f;
            materialData.baseColour[1] = 1.0f;
            materialData.baseColour[2] = 1.0f;
            materialData.baseColour[3] = 1.0f;
        }

        materialData.hasColourTexture = (pbr.baseColorTexture.index >= 0);

        return true;
    }

    inline size_t AlignConstantBufferSize(size_t size, size_t alignment = 256)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }
}