// ModuleAssets::getPathFromUID() - split out of ModuleAssets.cpp so it can be
// linked into GameScript.dll (via PhoenixCore) without the gltf import
// pipeline (tinygltf/SceneImporter/TextureImporter) that file otherwise
// pulls in. ModuleResources::CreateResourceFromUID() calls this
// unconditionally (even for the Animation case), so it's a hard dependency
// of ComponentAnimation::SendTrigger's link chain, not just Mesh/Material/
// Texture/Model.
#include "Globals.h"
#include "ModuleAssets.h"

std::string ModuleAssets::getPathFromUID(UID uid) const{
    auto it = m_uidToPath.find(uid);
    return it != m_uidToPath.end() ? it->second : "";
}
