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
	const auto& img = model.images[tex.source];

	ModuleFileSystem* fs = app->getFileSystem();
	std::string matFolder = fs->GetLibraryPath() + "Materials/" + sceneName + "/";
	fs->CreateDir(matFolder.c_str());

	if (!img.uri.empty()) {
		// Strip leading '/' — some exporters (e.g. busterDrone) incorrectly prefix
		// URIs with '/', producing a root-relative path instead of a relative one.
		std::string uri = img.uri;
		if (!uri.empty() && uri[0] == '/') uri = uri.substr(1);

		// Reject unsupported formats (e.g. PSD) before handing to WIC which cannot load them.
		std::string ext = std::filesystem::path(uri).extension().string();
		for (char& c : ext) c = (char)std::tolower((unsigned char)c);
		if (ext == ".psd" || ext == ".psb") {
			LOG("MaterialImporter: Skipping unsupported texture format '%s' — falling back to base colour", uri.c_str());
			return {};
		}

		std::string ddsPath = matFolder + TextureImporter::GetTextureName(uri.c_str()) + ".dds";
		if (!fs->Exists(ddsPath.c_str()) || !fs->Exists(ImporterUtils::MetaPath(ddsPath).c_str())) {
			if (!TextureImporter::Import((basePath + uri).c_str(), ddsPath, type)) {
				LOG("MaterialImporter: Failed to import texture %s", (basePath + uri).c_str());
				return {};
			}
		}
		return ddsPath;
	}

	// Embedded texture (GLB or GLTF with bufferView — tinygltf decodes into img.image)
	if (!img.image.empty() && img.width > 0 && img.height > 0 && img.component > 0) {
		std::string name = img.name.empty() ? ("embedded_" + std::to_string(tex.source)) : img.name;
		for (char& c : name) if (!std::isalnum((unsigned char)c) && c != '_') c = '_';
		std::string ddsPath = matFolder + name + ".dds";
		if (!fs->Exists(ddsPath.c_str()) || !fs->Exists(ImporterUtils::MetaPath(ddsPath).c_str())) {
			if (!TextureImporter::ImportFromMemory(img.image.data(), img.width, img.height, img.component, ddsPath, type)) {
				LOG("MaterialImporter: Failed to import embedded texture '%s'", name.c_str());
				return {};
			}
		}
		return ddsPath;
	}

	return {};
}

bool MaterialImporter::Import(const tinygltf::Material& gltfMat, const tinygltf::Model& model,
	const std::string& sceneName, const std::string& outputFile,
	int /*materialIndex*/, const std::string& basePath)
{
	const auto& pbr = gltfMat.pbrMetallicRoughness;

	MaterialHeader header{};

	header.metallic = (float)pbr.metallicFactor;
	header.roughness = (float)pbr.roughnessFactor;

	header.baseColorR = (float)pbr.baseColorFactor[0];
	header.baseColorG = (float)pbr.baseColorFactor[1];
	header.baseColorB = (float)pbr.baseColorFactor[2];
	header.baseColorA = (float)pbr.baseColorFactor[3];

	header.emissiveR = (float)gltfMat.emissiveFactor[0];
	header.emissiveG = (float)gltfMat.emissiveFactor[1];
	header.emissiveB = (float)gltfMat.emissiveFactor[2];

	std::string baseColorPath = importTexture(pbr.baseColorTexture.index, model, sceneName, basePath, TextureImporter::TextureType::Color);

	if (!baseColorPath.empty()) {
		header.hasTexture = 1;
		header.texturePathLength = (uint32_t)baseColorPath.size();
	}

	std::string normalPath = importTexture(gltfMat.normalTexture.index, model, sceneName, basePath, TextureImporter::TextureType::Normal);

	if (!normalPath.empty()) {
		header.hasNormalMap = 1;
		header.normalPathLength = (uint32_t)normalPath.size();

		header.normalStrength = (float)gltfMat.normalTexture.scale;
		if (header.normalStrength == 0.f) header.normalStrength = 1.f;

		header.flags |= MAT_FLAG_COMPRESSED_NORMS;
	}

	std::string aoPath = importTexture(gltfMat.occlusionTexture.index, model, sceneName, basePath, TextureImporter::TextureType::Occlusion);

	if (!aoPath.empty()) {
		header.hasAOMap = 1;
		header.aoPathLength = (uint32_t)aoPath.size();

		header.aoStrength = (float)gltfMat.occlusionTexture.strength;
		if (header.aoStrength == 0.f) header.aoStrength = 1.f;
	}

	std::string metalRoughPath = importTexture(pbr.metallicRoughnessTexture.index, model, sceneName, basePath, TextureImporter::TextureType::MetalRoughness);

	if (!metalRoughPath.empty()) {
		header.hasMetalRoughMap = 1;
		header.metalRoughPathLength = (uint32_t)metalRoughPath.size();
	}

	std::string emissivePath = importTexture(gltfMat.emissiveTexture.index, model, sceneName, basePath, TextureImporter::TextureType::Emissive);

	if (!emissivePath.empty()) {
		header.hasEmissiveMap = 1;
		header.emissivePathLength = (uint32_t)emissivePath.size();
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

	if (header.version != 7) {
		LOG("MaterialImporter: Version mismatch (%u), forcing reimport: %s",
			header.version, file.c_str());
		return false;
	}

	normalPath = readPath(header.normalPathLength);
	aoPath = readPath(header.aoPathLength);
	emissivePath = readPath(header.emissivePathLength);
	metalRoughPath = readPath(header.metalRoughPathLength);

	outMaterial = std::make_unique<Material>();
	auto& data = outMaterial->getData();
	data.metallic = header.metallic;
	data.roughness = header.roughness;
	data.normalStrength = header.normalStrength;
	data.aoStrength = header.aoStrength;
	data.emissiveFactor = Vector3(header.emissiveR, header.emissiveG, header.emissiveB);
	data.baseColor = Vector4(header.baseColorR, header.baseColorG, header.baseColorB, header.baseColorA);
	data.flags = header.flags;

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

	if (outMaterial->hasMetalRoughMap() && !outMaterial->getMetalRoughResource()) {
		outMaterial->getData().flags &= ~MAT_FLAG_METALROUGH_TEX;
	}

	if (outMaterial->hasNormalMap() && !outMaterial->getNormalMapResource()) {
		outMaterial->getData().flags &= ~MAT_FLAG_NORMAL_TEX;
	}

	if (outMaterial->hasAOMap() && !outMaterial->getAOMapResource()) {
		outMaterial->getData().flags &= ~MAT_FLAG_OCCLUSION_TEX;
	}

	if (outMaterial->hasEmissive() && !outMaterial->getEmissiveResource()) {
		outMaterial->getData().flags &= ~MAT_FLAG_EMISSIVE_TEX;
	}
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