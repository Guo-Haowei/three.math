#pragma once
#include "assets/guid.h"
#include "core/io/archive.h"
#include "core/math/geomath.h"

namespace my {

enum class AssetType : uint8_t {
    IMAGE,
    BUFFER,
    MESH,
    MATERIAL,
    ANIMATION,
    ARMATURE,

    COUNT,
};

class IAsset {
public:
    virtual ~IAsset() = default;

    [[nodiscard]] virtual Result<void> Load(const std::string& p_path) = 0;
    [[nodiscard]] virtual Result<void> Save(const std::string& p_path) = 0;

    AssetType GetType() const {
        return m_type;
    }

    // protected:
    AssetType m_type;
    std::string m_name;
    Guid m_guid;
};

class MaterialAsset : public IAsset {
public:
    enum {
        TEXTURE_BASE = 0,
        TEXTURE_NORMAL,
        TEXTURE_METALLIC_ROUGHNESS,
    };

    // @TODO: reflection to register properties?
    Vector4f baseColor;
    Vector3f emissive;
    float metallic;
    float roughness;
    //////////////////////////////////////

    Result<void> Load(const std::string& p_path) override;
    Result<void> Save(const std::string& p_path) override;
};

}  // namespace my
