// =============================================================================
// Wojcimierz Engine - engine/render/MeshData.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "render/MeshData.h"

namespace wz {

MeshData MeshData::Triangle() {
    MeshData m;
    const glm::vec3 n(0, 0, 1);
    m.vertices = {
        {{-0.6f, -0.5f, 0.0f}, n, {1.0f, 0.2f, 0.2f, 1.0f}},
        {{0.6f, -0.5f, 0.0f}, n, {0.2f, 1.0f, 0.2f, 1.0f}},
        {{0.0f, 0.6f, 0.0f}, n, {0.2f, 0.4f, 1.0f, 1.0f}},
    };
    m.indices = {0, 1, 2};
    return m;
}

MeshData MeshData::Cube(float size) {
    MeshData m;
    const float h = size * 0.5f;
    struct Face { glm::vec3 n, u, v; glm::vec4 c; };
    const Face faces[6] = {
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0.95f, 0.35f, 0.30f, 1}},  // +Z
        {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0.30f, 0.80f, 0.95f, 1}}, // -Z
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}, {0.40f, 0.90f, 0.40f, 1}},  // +X
        {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0.95f, 0.85f, 0.30f, 1}},  // -X
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0.85f, 0.45f, 0.95f, 1}},  // +Y
        {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}, {0.95f, 0.60f, 0.25f, 1}},  // -Y
    };
    for (const Face& f : faces) {
        const auto base = static_cast<std::uint32_t>(m.vertices.size());
        const glm::vec3 c = f.n * h;
        m.vertices.push_back({c - f.u * h - f.v * h, f.n, f.c});
        m.vertices.push_back({c + f.u * h - f.v * h, f.n, f.c});
        m.vertices.push_back({c + f.u * h + f.v * h, f.n, f.c});
        m.vertices.push_back({c - f.u * h + f.v * h, f.n, f.c});
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return m;
}

MeshData MeshData::Quad(float size) {
    MeshData m;
    const float h = size * 0.5f;
    const glm::vec3 n(0, 1, 0);
    const glm::vec4 c(0.6f, 0.6f, 0.65f, 1.0f);
    m.vertices = {{{-h, 0, h}, n, c}, {{h, 0, h}, n, c}, {{h, 0, -h}, n, c}, {{-h, 0, -h}, n, c}};
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

} // namespace wz
