// ============================================================
//  Wojcimierz Engine: render/MeshData.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/MeshData.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace wz {

MeshData MeshData::Triangle() {
    MeshData m;
    const glm::vec3 n(0, 0, 1);
    m.vertices = {
        {{-0.6f, -0.5f, 0.0f}, n, {1.0f, 0.2f, 0.2f, 1.0f}, {0.0f, 1.0f}},
        {{0.6f, -0.5f, 0.0f}, n, {0.2f, 1.0f, 0.2f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 0.6f, 0.0f}, n, {0.2f, 0.4f, 1.0f, 1.0f}, {0.5f, 0.0f}},
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
        m.vertices.push_back({c - f.u * h - f.v * h, f.n, f.c, {0.0f, 1.0f}});
        m.vertices.push_back({c + f.u * h - f.v * h, f.n, f.c, {1.0f, 1.0f}});
        m.vertices.push_back({c + f.u * h + f.v * h, f.n, f.c, {1.0f, 0.0f}});
        m.vertices.push_back({c - f.u * h + f.v * h, f.n, f.c, {0.0f, 0.0f}});
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return m;
}

MeshData MeshData::Quad(float size) {
    MeshData m;
    const float h = size * 0.5f;
    const glm::vec3 n(0, 1, 0);
    const glm::vec4 c(0.6f, 0.6f, 0.65f, 1.0f);
    m.vertices = {{{-h, 0, h}, n, c, {0, 1}}, {{h, 0, h}, n, c, {1, 1}}, {{h, 0, -h}, n, c, {1, 0}}, {{-h, 0, -h}, n, c, {0, 0}}};
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

MeshData MeshData::Sphere(float radius, int segments, int rings) {
    MeshData m;
    segments = std::max(segments, 3);
    rings = std::max(rings, 2);
    const glm::vec4 white(1.0f);
    for (int r = 0; r <= rings; ++r) {
        const float v = static_cast<float>(r) / static_cast<float>(rings);
        const float theta = v * glm::pi<float>(); // 0 = biegun polnocny (+Y)
        for (int s = 0; s <= segments; ++s) {
            const float u = static_cast<float>(s) / static_cast<float>(segments);
            const float phi = u * glm::two_pi<float>();
            const glm::vec3 n(std::sin(theta) * std::sin(phi), std::cos(theta), std::sin(theta) * std::cos(phi));
            m.vertices.push_back({n * radius, n, white, {u, v}});
        }
    }
    const auto row = static_cast<std::uint32_t>(segments + 1);
    for (std::uint32_t r = 0; r < static_cast<std::uint32_t>(rings); ++r) {
        for (std::uint32_t s = 0; s < static_cast<std::uint32_t>(segments); ++s) {
            const std::uint32_t a = r * row + s, b = a + row;
            // Nawiniecie CCW patrzac z zewnatrz.
            if (r != 0) m.indices.insert(m.indices.end(), {a, b, a + 1});
            if (r + 1 != static_cast<std::uint32_t>(rings)) m.indices.insert(m.indices.end(), {a + 1, b, b + 1});
        }
    }
    return m;
}

std::vector<SubMesh> MeshData::GetSubMeshes() const {
    if (!submeshes.empty()) return submeshes;
    if (indices.empty()) return {};
    return {SubMesh{0, static_cast<std::uint32_t>(indices.size())}};
}

} // namespace wz
