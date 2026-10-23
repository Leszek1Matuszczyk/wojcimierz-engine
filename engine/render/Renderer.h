#pragma once
#include "render/RHI.h"

#include <glm/glm.hpp>

namespace wz {

class Scene;

struct RenderSettings {
    glm::vec3 lightDirection{-0.4f, -1.0f, -0.6f};
    float ambient = 0.25f;
};

// Prosty renderer forward: dla aktywnej kamery (najmniejsze Camera::depth)
// rysuje wszystkie aktywne MeshRenderery w scenie.
class Renderer {
public:
    explicit Renderer(rhi::Device& device) : device_(&device) {}

    // Zwraca liczbe narysowanych obiektow.
    int Render(const Scene& scene, rhi::RenderTargetHandle target, int width, int height);

    RenderSettings settings;

private:
    rhi::Device* device_;
};

} // namespace wz
