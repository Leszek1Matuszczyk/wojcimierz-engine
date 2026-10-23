// =============================================================================
// Wojcimierz Engine - engine/render/MeshRenderer.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include "scene/Component.h"

#include <glm/glm.hpp>
#include <memory>

namespace wz {

class Mesh;
class Shader;

// Odpowiednik MeshFilter + MeshRenderer z Unity (na razie bez osobnego Materialu).
class MeshRenderer : public Component {
public:
    MeshRenderer() = default;
    MeshRenderer(std::shared_ptr<Mesh> mesh, std::shared_ptr<Shader> shader)
        : mesh(std::move(mesh)), shader(std::move(shader)) {}

    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Shader> shader;
    glm::vec4 color{1.0f};   // tint mnozony przez kolor wierzcholkow
    bool unlit = false;      // true = bez oswietlenia (czysty kolor wierzcholkow)
    bool cullBackFaces = true;
};

} // namespace wz
