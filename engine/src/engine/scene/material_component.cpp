#include "material_component.h"

#include "engine/assets/asset.h"
#include "engine/core/framework/asset_registry.h"
#include "engine/core/io/archive.h"

namespace my {

void MaterialComponent::RequestImage(int p_slot, const std::string& p_path) {
    unused(p_slot);
    unused(p_path);
    CRASH_NOW();
#if 0
    if (!p_path.empty()) {
        textures[p_slot].path = p_path;
        textures[p_slot].image = AssetManager::GetSingleton().LoadImageAsync(FilePath{ p_path });
    }
#endif
}

void MaterialComponent::Serialize(Archive& p_archive, uint32_t p_version) {
    unused(p_version);

    if (p_archive.IsWriteMode()) {
        p_archive << metallic;
        p_archive << roughness;
        p_archive << emissive;
        p_archive << baseColor;
        for (int i = 0; i < TEXTURE_MAX; ++i) {
            p_archive << textures[i].enabled;
            p_archive << textures[i].path;
        }
    } else {
        p_archive >> metallic;
        p_archive >> roughness;
        p_archive >> emissive;
        p_archive >> baseColor;
        for (int i = 0; i < TEXTURE_MAX; ++i) {
            p_archive >> textures[i].enabled;
            std::string& path = textures[i].path;
            p_archive >> path;

            // request image
            if (!path.empty()) {
                AssetRegistry::GetSingleton().RequestAsset(path);
            }
        }
    }
}

}  // namespace my
