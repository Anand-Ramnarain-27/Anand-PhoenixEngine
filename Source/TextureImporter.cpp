#include "Globals.h"
#include "TextureImporter.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleGPUResources.h"
#include "ModuleShaderDescriptors.h"
#include "ModuleD3D12.h"
#include "ShaderTableDesc.h"
#include <DirectXTex.h>
#include <filesystem>
#include <algorithm>

using namespace DirectX;

static std::string CanonicalPath(const std::string& p) {
	std::filesystem::path path = std::filesystem::weakly_canonical(p);
	std::string s = path.string();
	std::replace(s.begin(), s.end(), '\\', '/');
	return s;
}

static bool ProcessAndSave(ScratchImage& image, const std::string& outputPath, TextureImporter::TextureType type) {
	ScratchImage mipChain;
	if (image.GetMetadata().mipLevels == 1) {
		if (FAILED(GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain))) mipChain = std::move(image);
	}
	else {
		mipChain = std::move(image);
	}

	ScratchImage compressed;
	const TexMetadata& meta = mipChain.GetMetadata();

	if (!IsCompressed(meta.format)) {
		DXGI_FORMAT fmt;
		switch (type) {
		case TextureImporter::TextureType::Color:
		case TextureImporter::TextureType::Emissive:
			fmt = HasAlpha(meta.format) ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM_SRGB;
			break;
		case TextureImporter::TextureType::Normal:
			fmt = DXGI_FORMAT_BC5_UNORM;
			break;
		case TextureImporter::TextureType::Occlusion:
			fmt = DXGI_FORMAT_BC4_UNORM;
			break;
		case TextureImporter::TextureType::MetalRoughness:
			fmt = DXGI_FORMAT_BC5_UNORM;
			break;
		default:
			fmt = DXGI_FORMAT_BC1_UNORM;
			break;
		}
		if (FAILED(Compress(mipChain.GetImages(), mipChain.GetImageCount(), meta, fmt, TEX_COMPRESS_DEFAULT, 1.0f, compressed))) compressed = std::move(mipChain);
	}
	else {
		compressed = std::move(mipChain);
	}

	std::string normOutput = CanonicalPath(outputPath);
	std::wstring wOutput = std::filesystem::path(normOutput).wstring();
	if (FAILED(SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(), DDS_FLAGS_NONE, wOutput.c_str()))) {
		LOG("TextureImporter: Failed to save DDS '%s'", normOutput.c_str());
		return false;
	}

	const TexMetadata& finalMeta = compressed.GetMetadata();
	TextureImporter::SaveMetadata(normOutput, (uint32_t)finalMeta.width, (uint32_t)finalMeta.height, (uint32_t)finalMeta.mipLevels, (uint32_t)finalMeta.format);
	LOG("TextureImporter: Imported '%s' (%dx%d, %d mips, fmt=%d)", normOutput.c_str(), (int)finalMeta.width, (int)finalMeta.height, (int)finalMeta.mipLevels, (int)finalMeta.format);
	return true;
}

bool TextureImporter::Import(const char* sourcePath, const std::string& outputPath, TextureType type) {
	std::wstring wSource = std::filesystem::path(sourcePath).wstring();
	std::wstring ext = std::filesystem::path(sourcePath).extension().wstring();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

	ScratchImage image;
	HRESULT hr;
	if (ext == L".dds") hr = LoadFromDDSFile(wSource.c_str(), DDS_FLAGS_NONE, nullptr, image);
	else if (ext == L".tga") hr = LoadFromTGAFile(wSource.c_str(), nullptr, image);
	else if (ext == L".hdr") hr = LoadFromHDRFile(wSource.c_str(), nullptr, image);
	else hr = LoadFromWICFile(wSource.c_str(), WIC_FLAGS_NONE, nullptr, image);

	if (FAILED(hr)) {
		LOG("TextureImporter: Failed to load image '%s' 0x%08X", sourcePath, hr);
		return false;
	}

	return ProcessAndSave(image, outputPath, type);
}

bool TextureImporter::ImportFromMemory(const unsigned char* data, int width, int height, int channels,
	const std::string& outputPath, TextureType type) {
	if (!data || width <= 0 || height <= 0 || channels < 1 || channels > 4) {
		LOG("TextureImporter: ImportFromMemory invalid args (channels=%d, %dx%d)", channels, width, height);
		return false;
	}

	bool isColor = (type == TextureType::Color || type == TextureType::Emissive);

	ScratchImage image;
	if (channels == 4) {
		DXGI_FORMAT fmt = isColor ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		image.Initialize2D(fmt, (size_t)width, (size_t)height, 1, 1);
		memcpy(image.GetPixels(), data, (size_t)width * height * 4);
	}
	else if (channels == 3) {
		DXGI_FORMAT fmt = isColor ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		image.Initialize2D(fmt, (size_t)width, (size_t)height, 1, 1);
		uint8_t* dst = image.GetPixels();
		for (int i = 0; i < width * height; ++i) {
			dst[i * 4 + 0] = data[i * 3 + 0];
			dst[i * 4 + 1] = data[i * 3 + 1];
			dst[i * 4 + 2] = data[i * 3 + 2];
			dst[i * 4 + 3] = 255;
		}
	}
	else if (channels == 2) {
		image.Initialize2D(DXGI_FORMAT_R8G8_UNORM, (size_t)width, (size_t)height, 1, 1);
		memcpy(image.GetPixels(), data, (size_t)width * height * 2);
	}
	else {
		image.Initialize2D(DXGI_FORMAT_R8_UNORM, (size_t)width, (size_t)height, 1, 1);
		memcpy(image.GetPixels(), data, (size_t)width * height);
	}

	return ProcessAndSave(image, outputPath, type);
}

bool TextureImporter::Load(const std::string& file, ComPtr<ID3D12Resource>& outTexture, D3D12_GPU_DESCRIPTOR_HANDLE& outSRV) {
	std::string normFile = CanonicalPath(file);
	std::string metaPath = ImporterUtils::MetaPath(normFile);

	LOG("Loading meta for: %s", normFile.c_str());
	LOG("Meta path: %s", metaPath.c_str());


	TextureHeader header;
	std::vector<char> rawBuffer;

	if (!ImporterUtils::LoadBuffer(metaPath, header, rawBuffer)) {
		LOG("TextureImporter: Missing metadata, regenerating for '%s'", normFile.c_str());

		std::wstring wFile = std::filesystem::path(normFile).wstring();
		DirectX::ScratchImage img;

		if (FAILED(DirectX::LoadFromDDSFile(wFile.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, img))) {
			LOG("TextureImporter: Failed to reload DDS for metadata '%s'", normFile.c_str());
			return false;
		}

		const DirectX::TexMetadata& meta = img.GetMetadata();

		SaveMetadata(normFile,
			(uint32_t)meta.width,
			(uint32_t)meta.height,
			(uint32_t)meta.mipLevels,
			(uint32_t)meta.format
		);

		header.width = (uint32_t)meta.width;
		header.height = (uint32_t)meta.height;
		header.mipLevels = (uint32_t)meta.mipLevels;
		header.format = (uint32_t)meta.format;
	}

	if (!ImporterUtils::ValidateHeader(header, 0x54455854)) {
		LOG("TextureImporter: Bad metadata magic/version for '%s'", normFile.c_str());
		return false;
	}

	outTexture = app->getGPUResources()->createTextureFromFile(normFile, true);
	if (!outTexture) {
		LOG("TextureImporter: Failed to create texture resource for '%s'", normFile.c_str());
		return false;
	}

	ShaderTableDesc tableDesc = app->getShaderDescriptors()->allocTable();
	tableDesc.createTexture2DSRV(outTexture.Get(), 0, (DXGI_FORMAT)header.format, header.mipLevels);
	outSRV = tableDesc.getGPUHandle(0);
	return true;
}

std::string TextureImporter::GetTextureName(const char* filePath) {
	return std::filesystem::path(filePath).stem().string();
}

bool TextureImporter::SaveMetadata(const std::string& ddsPath, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t format) {
	std::string normPath = CanonicalPath(ddsPath);

	LOG("Saving meta for: %s", normPath.c_str());
	LOG("Meta path: %s", ImporterUtils::MetaPath(normPath).c_str());

	TextureHeader header;
	header.width = width;
	header.height = height;
	header.mipLevels = mipLevels;
	header.format = format;
	return ImporterUtils::SaveBuffer(ImporterUtils::MetaPath(normPath), header);
}
