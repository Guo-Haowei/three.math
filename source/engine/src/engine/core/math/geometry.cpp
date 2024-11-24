#include "geometry.h"

namespace my {

/**
 *        E__________________ H
 *       /|                 /|
 *      / |                / |
 *     /  |               /  |
 *   A/___|______________/D  |
 *    |   |              |   |
 *    |   |              |   |
 *    |   |              |   |
 *    |  F|______________|___|G
 *    |  /               |  /
 *    | /                | /
 *   B|/_________________|C
 *
 */

// clang-format off
enum { A = 0, B = 1, C = 2, D = 3, E = 4, F = 5, G = 6, H = 7 };
// clang-format on

MeshComponent MakePlaneMesh(const Vector3f& p_scale) {
    MeshComponent mesh;
    const float x = p_scale.x;
    const float y = p_scale.y;
    mesh.positions = {
        Vector3f(-x, +y, 0.0f),  // A
        Vector3f(-x, -y, 0.0f),  // B
        Vector3f(+x, -y, 0.0f),  // C
        Vector3f(+x, +y, 0.0f),  // D
    };

    const Vector3f normal{ 0, 0, 1 };
    mesh.normals = {
        normal,
        normal,
        normal,
        normal,
    };

    mesh.texcoords_0 = {
        Vector2f(0, 1),  // top-left
        Vector2f(0, 0),  // bottom-left
        Vector2f(1, 0),  // bottom-right
        Vector2f(1, 1),  // top-right
    };

    // clang-format off
    mesh.indices = {
#if 1
        A, B, D,  // ABD
        D, B, C,  // DBC
#else
        A, D, B, // ADB
        D, C, B, // DBC
#endif
    };
    // clang-format on

    MeshComponent::MeshSubset subset;
    subset.index_count = static_cast<uint32_t>(mesh.indices.size());
    subset.index_offset = 0;
    mesh.subsets.emplace_back(subset);

    mesh.CreateRenderData();
    return mesh;
}

MeshComponent MakeCubeMesh(const Vector3f& p_scale) {
    MeshComponent mesh;
    // clang-format off
    constexpr uint32_t indices[] = {
        0,          1,          2,          0,          2,          3,
        0 + 4,      2 + 4,      1 + 4,      0 + 4,      3 + 4,      2 + 4,  // swapped winding
        0 + 4 * 2,  1 + 4 * 2,  2 + 4 * 2,  0 + 4 * 2,  2 + 4 * 2,  3 + 4 * 2,
        0 + 4 * 3,  2 + 4 * 3,  1 + 4 * 3,  0 + 4 * 3,  3 + 4 * 3,  2 + 4 * 3, // swapped winding
        0 + 4 * 4,  2 + 4 * 4,  1 + 4 * 4,  0 + 4 * 4,  3 + 4 * 4,  2 + 4 * 4, // swapped winding
        0 + 4 * 5,  1 + 4 * 5,  2 + 4 * 5,  0 + 4 * 5,  2 + 4 * 5,  3 + 4 * 5,
    };
    // clang-format on

    const Vector3f& s = p_scale;
    mesh.positions = {
        // -Z
        Vector3f(-s.x, +s.y, -s.z),
        Vector3f(-s.x, -s.y, -s.z),
        Vector3f(+s.x, -s.y, -s.z),
        Vector3f(+s.x, +s.y, -s.z),

        // +Z
        Vector3f(-s.x, +s.y, +s.z),
        Vector3f(-s.x, -s.y, +s.z),
        Vector3f(+s.x, -s.y, +s.z),
        Vector3f(+s.x, +s.y, +s.z),

        // -X
        Vector3f(-s.x, -s.y, +s.z),
        Vector3f(-s.x, -s.y, -s.z),
        Vector3f(-s.x, +s.y, -s.z),
        Vector3f(-s.x, +s.y, +s.z),

        // +X
        Vector3f(+s.x, -s.y, +s.z),
        Vector3f(+s.x, -s.y, -s.z),
        Vector3f(+s.x, +s.y, -s.z),
        Vector3f(+s.x, +s.y, +s.z),

        // -Y
        Vector3f(-s.x, -s.y, +s.z),
        Vector3f(-s.x, -s.y, -s.z),
        Vector3f(+s.x, -s.y, -s.z),
        Vector3f(+s.x, -s.y, +s.z),

        // +Y
        Vector3f(-s.x, +s.y, +s.z),
        Vector3f(-s.x, +s.y, -s.z),
        Vector3f(+s.x, +s.y, -s.z),
        Vector3f(+s.x, +s.y, +s.z),
    };

    mesh.texcoords_0 = {
        Vector2f(0, 0),
        Vector2f(0, 1),
        Vector2f(1, 1),
        Vector2f(1, 0),

        Vector2f(0, 0),
        Vector2f(0, 1),
        Vector2f(1, 1),
        Vector2f(1, 0),

        Vector2f(0, 0),
        Vector2f(0, 1),
        Vector2f(1, 1),
        Vector2f(1, 0),

        Vector2f(0, 0),
        Vector2f(0, 1),
        Vector2f(1, 1),
        Vector2f(1, 0),

        Vector2f(0, 0),
        Vector2f(0, 1),
        Vector2f(1, 1),
        Vector2f(1, 0),

        Vector2f(0, 0),
        Vector2f(0, 1),
        Vector2f(1, 1),
        Vector2f(1, 0),
    };

    mesh.normals = {
        Vector3f(0, 0, -1),
        Vector3f(0, 0, -1),
        Vector3f(0, 0, -1),
        Vector3f(0, 0, -1),

        Vector3f(0, 0, 1),
        Vector3f(0, 0, 1),
        Vector3f(0, 0, 1),
        Vector3f(0, 0, 1),

        Vector3f(-1, 0, 0),
        Vector3f(-1, 0, 0),
        Vector3f(-1, 0, 0),
        Vector3f(-1, 0, 0),

        Vector3f(1, 0, 0),
        Vector3f(1, 0, 0),
        Vector3f(1, 0, 0),
        Vector3f(1, 0, 0),

        Vector3f(0, -1, 0),
        Vector3f(0, -1, 0),
        Vector3f(0, -1, 0),
        Vector3f(0, -1, 0),

        Vector3f(0, 1, 0),
        Vector3f(0, 1, 0),
        Vector3f(0, 1, 0),
        Vector3f(0, 1, 0),
    };

    for (int i = 0; i < array_length(indices); i += 3) {
        mesh.indices.emplace_back(indices[i]);
        mesh.indices.emplace_back(indices[i + 2]);
        mesh.indices.emplace_back(indices[i + 1]);
    }

    MeshComponent::MeshSubset subset;
    subset.index_count = array_length(indices);
    subset.index_offset = 0;
    mesh.subsets.emplace_back(subset);

    mesh.CreateRenderData();
    return mesh;
}

MeshComponent MakeSphereMesh(float p_radius, int p_rings, int p_sectors) {
    MeshComponent mesh;

    auto& indices = mesh.indices;
    constexpr float pi = glm::pi<float>();
    for (int step_x = 0; step_x <= p_sectors; ++step_x) {
        for (int step_y = 0; step_y <= p_rings; ++step_y) {
            const float x_seg = (float)step_x / (float)p_sectors;
            const float y_seg = (float)step_y / (float)p_rings;
            const Vector3f normal{
                std::cos(x_seg * 2.0f * pi) * std::sin(y_seg * pi),
                std::cos(y_seg * pi),
                std::sin(x_seg * 2.0f * pi) * std::sin(y_seg * pi)
            };

            mesh.positions.emplace_back(p_radius * normal);
            mesh.normals.emplace_back(normal);
            mesh.texcoords_0.emplace_back(Vector2f(x_seg, y_seg));
        }
    }

    for (int y = 0; y < p_rings; ++y) {
        for (int x = 0; x < p_sectors; ++x) {
            /* a - b
               |   |
               c - d */
            uint32_t a = (y * (p_sectors + 1) + x);
            uint32_t b = (y * (p_sectors + 1) + x + 1);
            uint32_t c = ((y + 1) * (p_sectors + 1) + x);
            uint32_t d = ((y + 1) * (p_sectors + 1) + x + 1);

            indices.emplace_back(a);
            indices.emplace_back(c);
            indices.emplace_back(b);

            indices.emplace_back(b);
            indices.emplace_back(c);
            indices.emplace_back(d);
        }
    }

    MeshComponent::MeshSubset subset;
    subset.index_count = static_cast<uint32_t>(indices.size());
    subset.index_offset = 0;
    mesh.subsets.emplace_back(subset);

    mesh.CreateRenderData();
    return mesh;
}

MeshComponent MakeCylinder(float p_radius,
                           float p_height,
                           int p_sectors) {
    MeshComponent mesh;

    auto& indices = mesh.indices;
    constexpr float pi = glm::pi<float>();

    std::array<float, 2> heights = { 0.5f * p_height, -0.5f * p_height };

    // cylinder side
    for (int index = 0; index <= p_sectors; ++index) {
        float angle = 2.0f * pi * index / p_sectors;
        float x = p_radius * glm::cos(angle);
        float z = p_radius * glm::sin(angle);

        Vector3f point_1(x, heights[0], z);
        Vector3f point_2(x, heights[1], z);

        Vector3f normal = glm::normalize(Vector3f(x, 0.0f, z));

        mesh.positions.emplace_back(point_1);
        mesh.normals.emplace_back(normal);
        mesh.texcoords_0.emplace_back(Vector2f());

        mesh.positions.emplace_back(point_2);
        mesh.normals.emplace_back(normal);
        mesh.texcoords_0.emplace_back(Vector2f());
    }

    for (int index = 0; index < p_sectors; ++index) {
        /* a - b
           |   |
           c - d */
        const uint32_t a = 2 * index;
        const uint32_t c = 2 * index + 1;
        const uint32_t b = 2 * index + 2;
        const uint32_t d = 2 * index + 3;

        indices.emplace_back(a);
        indices.emplace_back(b);
        indices.emplace_back(c);

        indices.emplace_back(c);
        indices.emplace_back(b);
        indices.emplace_back(d);
    }

    // cylinder circles
    for (float height : heights) {
        uint32_t offset = static_cast<uint32_t>(mesh.positions.size());

        Vector3f normal = glm::normalize(Vector3f(0.0f, height, 0.0f));

        for (int index = 0; index <= p_sectors; ++index) {
            float angle = 2.0f * pi * index / p_sectors;
            float x = p_radius * glm::cos(angle);
            float z = p_radius * glm::sin(angle);

            Vector3f point(x, height, z);

            mesh.positions.emplace_back(point);
            mesh.normals.emplace_back(normal);
            mesh.texcoords_0.emplace_back(Vector2f());
        }

        mesh.positions.emplace_back(Vector3f(0.0f, height, 0.0f));
        mesh.normals.emplace_back(normal);
        mesh.texcoords_0.emplace_back(Vector2f());

        uint32_t center_index = static_cast<uint32_t>(mesh.positions.size()) - 1;
        for (int index = 0; index < p_sectors; ++index) {
            if (height < 0) {
                indices.emplace_back(offset + index);
                indices.emplace_back(offset + index + 1);
                indices.emplace_back(center_index);
            } else {
                indices.emplace_back(offset + index + 1);
                indices.emplace_back(offset + index);
                indices.emplace_back(center_index);
            }
        }
    }

    MeshComponent::MeshSubset subset;
    subset.index_count = static_cast<uint32_t>(indices.size());
    subset.index_offset = 0;
    mesh.subsets.emplace_back(subset);

    mesh.CreateRenderData();
    return mesh;
}

MeshComponent MakeTorus(float p_radius,
                        float p_tube_radius,
                        int p_sectors,
                        int p_tube_sectors) {
    MeshComponent mesh;

    constexpr float two_pi = 2.0f * glm::pi<float>();
    for (int index_1 = 0; index_1 <= p_sectors; ++index_1) {
        const float angle_1 = two_pi * index_1 / p_sectors;
        for (int index_2 = 0; index_2 <= p_tube_sectors; ++index_2) {
            const float angle_2 = two_pi * index_2 / p_tube_sectors;
            const float x = (p_radius + p_tube_radius * glm::cos(angle_2)) * glm::cos(angle_1);
            const float y = p_tube_radius * glm::sin(angle_2);
            const float z = (p_radius + p_tube_radius * glm::cos(angle_2)) * glm::sin(angle_1);
            const float nx = p_tube_radius * glm::cos(angle_2) * glm::cos(angle_1);
            const float ny = p_tube_radius * glm::sin(angle_2);
            const float nz = p_tube_radius * glm::cos(angle_2) * glm::sin(angle_1);

            mesh.positions.emplace_back(Vector3f(x, y, z));
            mesh.normals.emplace_back(glm::normalize(Vector3f(nx, ny, nz)));
            mesh.texcoords_0.emplace_back(Vector2f());
        }
    }

    auto& indices = mesh.indices;

    for (int index_1 = 0; index_1 < p_sectors; ++index_1) {
        for (int index_2 = 0; index_2 < p_tube_sectors; ++index_2) {
            /* a - b
               |   |
               c - d */
            const uint32_t a = index_2 + index_1 * (p_tube_sectors + 1);
            const uint32_t b = a + (p_tube_sectors + 1);
            const uint32_t c = a + 1;
            const uint32_t d = b + 1;

            indices.emplace_back(a);
            indices.emplace_back(c);
            indices.emplace_back(b);

            indices.emplace_back(b);
            indices.emplace_back(c);
            indices.emplace_back(d);
        }
    }

    MeshComponent::MeshSubset subset;
    subset.index_count = static_cast<uint32_t>(indices.size());
    subset.index_offset = 0;
    mesh.subsets.emplace_back(subset);

    mesh.CreateRenderData();
    return mesh;
}

MeshComponent MakeBoxMesh(float size) {
    MeshComponent mesh;
    mesh.positions = {
        Vector3f(-size, +size, +size),  // A
        Vector3f(-size, -size, +size),  // B
        Vector3f(+size, -size, +size),  // C
        Vector3f(+size, +size, +size),  // D
        Vector3f(-size, +size, -size),  // E
        Vector3f(-size, -size, -size),  // F
        Vector3f(+size, -size, -size),  // G
        Vector3f(+size, +size, -size),  // H
    };

    mesh.indices = {
        A, B, D,  // ABD
        D, B, C,  // DBC
        E, H, F,  // EHF
        H, G, F,  // HGF
        D, C, G,  // DCG
        D, G, H,  // DGH
        A, F, B,  // AFB
        A, E, F,  // AEF
        A, D, H,  // ADH
        A, H, E,  // AHE
        B, F, G,  // BFG
        B, G, C,  // BGC
    };

    return mesh;
}

MeshComponent MakeGrassBillboard(const Vector3f& p_scale) {
    MeshComponent mesh;

    const float x = p_scale.x;
    const float y = p_scale.y;

    std::array<Vector4f, 4> points = {
        Vector4f(-x, 2 * y, 0.0f, 1.0f),  // A
        Vector4f(-x, 0.0f, 0.0f, 1.0f),   // B
        Vector4f(+x, 0.0f, 0.0f, 1.0f),   // C
        Vector4f(+x, 2 * y, 0.0f, 1.0f),  // D
    };

    // @TODO: correct sampler
    std::array<Vector2f, 4> uvs = {
        Vector2f(0, 1),  // top-left
        Vector2f(0, 0),  // bottom-left
        Vector2f(1, 0),  // bottom-right
        Vector2f(1, 1),  // top-right
    };

    uint32_t indices[] = {
        A, B, D,  // ABD
        D, B, C,  // DBC
    };

    float angle = 0.0f;
    for (int i = 0; i < 3; ++i, angle += glm::radians(120.0f)) {
        Matrix4x4f rotation = glm::rotate(angle, Vector3f(0, 1, 0));
        Vector4f normal = rotation * Vector4f{ 0, 0, 1, 0 };

        uint32_t offset = static_cast<uint32_t>(mesh.positions.size());
        for (size_t j = 0; j < points.size(); ++j) {
            mesh.positions.emplace_back(rotation * points[j]);
            mesh.normals.emplace_back(normal);
            mesh.texcoords_0.emplace_back(uvs[j]);
        }

        for (int j = 0; j < array_length(indices); ++j) {
            mesh.indices.emplace_back(indices[j] + offset);
        }
    }

    // flip uv
    for (auto& uv : mesh.texcoords_0) {
        uv.y = 1.0f - uv.y;
    }

    MeshComponent::MeshSubset subset;
    subset.index_count = static_cast<uint32_t>(mesh.indices.size());
    subset.index_offset = 0;
    mesh.subsets.emplace_back(subset);

    mesh.CreateRenderData();
    return mesh;
}

MeshComponent MakeSkyBoxMesh() {
    float size = 1.0f;
    MeshComponent mesh;
    mesh.positions = {
        Vector3f(-size, +size, +size),  // A
        Vector3f(-size, -size, +size),  // B
        Vector3f(+size, -size, +size),  // C
        Vector3f(+size, +size, +size),  // D
        Vector3f(-size, +size, -size),  // E
        Vector3f(-size, -size, -size),  // F
        Vector3f(+size, -size, -size),  // G
        Vector3f(+size, +size, -size),  // H
    };

    mesh.indices = {
        A, D, B,  // ABD
        D, C, B,  // DBC
        E, F, H,  // EHF
        H, F, G,  // HGF
        D, G, C,  // DCG
        D, H, G,  // DGH
        A, B, F,  // AFB
        A, F, E,  // AEF
        A, H, D,  // ADH
        A, E, H,  // AHE
        B, G, F,  // BFG
        B, C, G,  // BGC
        // A, B, D,  // ABD
        // D, B, C,  // DBC
        // E, H, F,  // EHF
        // H, G, F,  // HGF
        // D, C, G,  // DCG
        // D, G, H,  // DGH
        // A, F, B,  // AFB
        // A, E, F,  // AEF
        // A, D, H,  // ADH
        // A, H, E,  // AHE
        // B, F, G,  // BFG
        // B, G, C,  // BGC
    };

    MeshComponent::MeshSubset subset;
    subset.index_count = static_cast<uint32_t>(mesh.indices.size());
    subset.index_offset = 0;
    mesh.subsets.emplace_back(subset);

    mesh.CreateRenderData();
    return mesh;
}

// load scene
MeshComponent MakeBoxWireframeMesh(float size) {
    MeshComponent mesh;
    mesh.positions = {
        Vector3f(-size, +size, +size),  // A
        Vector3f(-size, -size, +size),  // B
        Vector3f(+size, -size, +size),  // C
        Vector3f(+size, +size, +size),  // D
        Vector3f(-size, +size, -size),  // E
        Vector3f(-size, -size, -size),  // F
        Vector3f(+size, -size, -size),  // G
        Vector3f(+size, +size, -size),  // H
    };

    mesh.indices = { A, B, B, C, C, D, D, A, E, F, F, G, G, H, H, E, A, E, B, F, D, H, C, G };

    return mesh;
}

}  // namespace my
