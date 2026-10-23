// =============================================================================
// Wojcimierz Engine - engine/render/MeshData.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace wz {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
};

// Geometria po stronie CPU (bez GPU) - latwa do testowania i do generowania prymitywow.
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    static MeshData Triangle();               // kolorowy trojkat RGB w plaszczyznie XY
    static MeshData Cube(float size = 1.0f);  // szescian z kolorowymi scianami i normalnymi
    static MeshData Quad(float size = 1.0f);  // kwadrat w plaszczyznie XZ (np. podloga)
};

} // namespace wz
