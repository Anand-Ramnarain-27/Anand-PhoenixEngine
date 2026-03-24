#include "Globals.h"
#include "MaterialImporter.h"
#include "TextureImporter.h"
#include "Material.h"
#include "ImporterUtils.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <cstring>

std::string MaterialImporter::importTexture(int texIndex, const tinygltf::Model& model,
	const std::string& sceneName,
	const std::string& basePath,
	TextureImporter::TextureType type) {
	if (texIndex < 0 || texIndex >= (int)model.textures.size()) return {};
	const auto& tex = model.textures[texIndex];
	if (tex.source < 0 || tex.source >= (int)model.images.size()) return {};
	const std::string& uri = model.images[tex.source].uri;
	if (uri.empty()) return {};

	ModuleFileSystem* fs = app->getFileSystem();
	std::string matFolder = fs->GetLibraryPath() + "Materials/" + sceneName + "/";
	fs->CreateDir(matFolder.c_str());
	std::string ddsPath = matFolder + TextureImporter::GetTextureName(uri.c_str()) + ".dds";

	if (!fs->Exists(ddsPath.c_str()) || !fs->Exists(ImporterUtils::MetaPath(ddsPath).c_str())) {
		if (!TextureImporter::Import((basePath + uri).c_str(), ddsPath, type)) {
			LOG("MaterialImporter: Failed to import texture %s", (basePath + uri).c_str());
			return {};
		}
	}
	return ddsPath;
}

bool MaterialImporter::Import(const tinygltf::Material& gltfMat, const tinygltf::Model& model,
	const std::string& sceneName, const std::string& outputFile,
	int /*materialIndex*/, const std::string& basePath) {
	const auto& pbr = gltfMat.pbrMetallicRoughness;

	MaterialHeader header;
	header.metallic = (float)pbr.metallicFactor;
	header.roughness = (float)pbr.roughnessFactor;
	header.emissiveR = (float)gltfMat.emissiveFactor[0];
	header.emissiveG = (float)gltfMat.emissiveFactor[1];
	header.emissiveB = (float)gltfMat.emissiveFactor[2];

	std::string baseColorPath = importTexture(pbr.baseColorTexture.index, model, sceneName, basePath,
		TextureImporter::TextureType::Color);
	if (!baseColorPath.empty()) {
		header.hasTexture = 1;
		header.texturePathLength = (uint32_t)baseColorPath.size();
	}

	std::string normalPath = importTexture(gltfMat.normalTexture.index, model, sceneName, basePath,
		TextureImporter::TextureType::Normal);
	if (!normalPath.empty()) {
		header.hasNormalMap = 1;
		header.normalPathLength = (uint32_t)normalPath.size();
		header.normalStrength = (float)gltfMat.normalTexture.scale;
		if (header.normalStrength == 0.f) header.normalStrength = 1.f;
	}

	std::string aoPath = importTexture(gltfMat.occlusionTexture.index, model, sceneName, basePath,
		TextureImporter::TextureType::OcclusionMetalRough);
	if (!aoPath.empty()) {
		header.hasAOMap = 1;
		header.aoPathLength = (uint32_t)aoPath.size();
		header.aoStrength = (float)gltfMat.occlusionTexture.strength;
		if (header.aoStrength == 0.f) header.aoStrength = 1.f;
	}

	std::string emissivePath = importTexture(gltfMat.emissiveTexture.index, model, sceneName, basePath,
		TextureImporter::TextureType::Emissive);
	if (!emissivePath.empty()) {
		header.hasEmissiveMap = 1;
		header.emissivePathLength = (uint32_t)emissivePath.size();
	}

	std::string metalRoughPath = importTexture(pbr.metallicRoughnessTexture.index, model, sceneName, basePath,
		TextureImporter::TextureType::OcclusionMetalRough);
	if (!metalRoughPath.empty()) {
		header.hasMetalRoughMap = 1;
		header.metalRoughPathLength = (uint32_t)metalRoughPath.size();
	}

	return Save(header, baseColorPath, normalPath, aoPath, emissivePath, metalRoughPath, outputFile);
}

bool MaterialImporter::Load(const std::string& file, std::unique_ptr<Material>& outMaterial) {
	MaterialHeader header;
	std::vector<char> rawBuffer;
	if (!ImporterUtils::LoadBuffer(file, header, rawBuffer)) return false;
	if (!ImporterUtils::ValidateHeader(header, 0x4D415452)) {
		LOG("MaterialImporter: Invalid file format: %s", file.c_str());
		return false;
	}

	const char* cursor = rawBuffer.data() + sizeof(MaterialHeader);

	auto readPath = [&](uint32_t len) -> std::string {
		if (len == 0) return {};
		std::string s(cursor, len);
		cursor += len;
		return s;
		};

	std::string baseColorPath = readPath(header.texturePathLength);
	std::string normalPath;
	std::string aoPath;
	std::string emissivePath;
	std::string metalRoughPath;

	if (header.version >= 2) {
		normalPath = readPath(header.normalPathLength);
		aoPath = readPath(header.aoPathLength);
		emissivePath = readPath(header.emissivePathLength);
	}

	if (header.version >= 3) {
		metalRoughPath = readPath(header.metalRoughPathLength);
	}

	outMaterial = std::make_unique<Material>();
	auto& data = outMaterial->getData();
	data.metallic = header.metallic;
	data.roughness = header.roughness;
	data.normalStrength = header.normalStrength;
	data.aoStrength = header.aoStrength;
	data.emissiveFactor = Vector3(header.emissiveR, header.emissiveG, header.emissiveB);

	auto loadTex = [&](const std::string& path,
		void (Material::* setter)(ComPtr<ID3D12Resource>, D3D12_GPU_DESCRIPTOR_HANDLE)) -> bool {
			if (path.empty()) return false;
			ComPtr<ID3D12Resource>      tex;
			D3D12_GPU_DESCRIPTOR_HANDLE srv{};
			if (!TextureImporter::Load(path, tex, srv)) {
				LOG("MaterialImporter: Failed to load texture %s", path.c_str());
				return false;
			}
			(outMaterial.get()->*setter)(tex, srv);
			return true;
		};

	loadTex(baseColorPath, &Material::setBaseColorTexture);
	loadTex(normalPath, &Material::setNormalMap);
	loadTex(aoPath, &Material::setAOMap);
	loadTex(emissivePath, &Material::setEmissiveMap);
	loadTex(metalRoughPath, &Material::setMetalRoughMap);

	return true;
}

bool MaterialImporter::Save(const MaterialHeader& header,
	const std::string& baseColorPath, const std::string& normalPath,
	const std::string& aoPath, const std::string& emissivePath,
	const std::string& metalRoughPath, const std::string& file) {
	std::vector<char> payload;
	payload.reserve(baseColorPath.size() + normalPath.size() + aoPath.size()
		+ emissivePath.size() + metalRoughPath.size());

	auto append = [&](const std::string& s) {
		payload.insert(payload.end(), s.begin(), s.end());
		};

	append(baseColorPath);
	append(normalPath);
	append(aoPath);
	append(emissivePath);
	append(metalRoughPath);

	if (!ImporterUtils::SaveBuffer(file, header, payload)) {
		LOG("MaterialImporter: Failed to save %s", file.c_str());
		return false;
	}
	return true;
}