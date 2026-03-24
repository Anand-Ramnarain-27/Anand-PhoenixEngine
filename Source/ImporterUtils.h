#pragma once
#include "Application.h"
#include "ModuleFileSystem.h"
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <filesystem>

namespace ImporterUtils {

    inline std::string MetaPath(const std::string& assetPath) {
        ModuleFileSystem* fs = app->getFileSystem();
        std::string metaFolder = fs->GetLibraryPath() + "metadata/";
        fs->CreateDir(metaFolder.c_str());
        std::string sanitised = assetPath;
        for (char& c : sanitised) {
            if (c == '/' || c == '\\' || c == ':') c = '~';
        }
        return metaFolder + sanitised + ".meta";
    }

    template<typename THeader>
    inline bool LoadBuffer(const std::string& file, THeader& outHeader, std::vector<char>& outBuffer) {
        char* raw = nullptr;
        uint32_t size = app->getFileSystem()->Load(file.c_str(), &raw);
        if (!raw || size < sizeof(THeader)) {
            delete[] raw;
            return false;
        }
        memcpy(&outHeader, raw, sizeof(THeader));
        outBuffer.assign(raw, raw + size);
        delete[] raw;
        return true;
    }

    template<typename THeader>
    inline bool ValidateHeader(const THeader& header, uint32_t expectedMagic) {
        return header.magic == expectedMagic && header.version != 0;
    }

    template<typename THeader>
    inline bool SaveBuffer(const std::string& file, const THeader& header, const std::vector<char>& payload) {
        uint32_t total = sizeof(THeader) + (uint32_t)payload.size();
        std::vector<char> buffer(total);
        memcpy(buffer.data(), &header, sizeof(THeader));
        if (!payload.empty()) memcpy(buffer.data() + sizeof(THeader), payload.data(), payload.size());
        return app->getFileSystem()->Save(file.c_str(), buffer.data(), total);
    }

    template<typename THeader>
    inline bool SaveBuffer(const std::string& file, const THeader& header) {
        return app->getFileSystem()->Save(file.c_str(), &header, sizeof(THeader));
    }

    inline std::string IndexedPath(const std::string& folder, uint32_t index, const char* ext) {
        return folder + "/" + std::to_string(index) + ext;
    }

}