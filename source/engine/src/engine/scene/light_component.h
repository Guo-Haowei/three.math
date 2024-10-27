#pragma once
#include "core/math/geomath.h"
#include "shader_defines.h"

namespace my {

class Archive;
class TransformComponent;

class LightComponent {
public:
    enum : uint32_t {
        NONE = BIT(0),
        DIRTY = BIT(1),
        CAST_SHADOW = BIT(2),
    };

    bool IsDirty() const { return m_flags & DIRTY; }
    void SetDirty(bool p_dirty = true) { p_dirty ? m_flags |= DIRTY : m_flags &= ~DIRTY; }

    bool CastShadow() const { return m_flags & CAST_SHADOW; }
    void SetCastShadow(bool p_cast = true) { p_cast ? m_flags |= CAST_SHADOW : m_flags &= ~CAST_SHADOW; }

    int GetType() const { return m_type; }
    void SetType(int p_type) { m_type = p_type; }

    float GetMaxDistance() const { return m_maxDistance; }
    int GetShadowMapIndex() const { return m_shadowMapIndex; }

    void Update(const TransformComponent& p_transform);

    void Serialize(Archive& p_archive, uint32_t p_version);

    const auto& GetMatrices() const { return m_lightSpaceMatrices; }
    const vec3& GetPosition() const { return m_position; }

    struct {
        float constant;
        float linear;
        float quadratic;
    } m_atten;

private:
    uint32_t m_flags = DIRTY;
    int m_type = LIGHT_TYPE_INFINITE;

    // Non-serialized
    float m_maxDistance;
    vec3 m_position;
    int m_shadowMapIndex = -1;
    std::array<mat4, 6> m_lightSpaceMatrices;
};

}  // namespace my
