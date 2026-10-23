#include "render/Renderer.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "render/MeshRenderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"

namespace wz {

int Renderer::Render(const Scene& scene, rhi::RenderTargetHandle target, int width, int height) {
    if (width <= 0 || height <= 0) return 0;

    const Camera* cam = nullptr;
    scene.ForEach<Camera>([&](Camera& c) { if (!cam || c.depth < cam->depth) cam = &c; });

    const glm::vec4 clear = cam ? cam->backgroundColor : glm::vec4(0, 0, 0, 1);
    device_->BeginPass(target, width, height, clear);
    int drawn = 0;
    if (cam) {
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const glm::mat4 viewProj = cam->GetProjectionMatrix(aspect) * cam->GetViewMatrix();
        scene.ForEach<MeshRenderer>([&](MeshRenderer& r) {
            if (!r.mesh || !r.shader) return;
            const auto sh = r.shader->GetHandle();
            rhi::PipelineState ps;
            ps.cullBackFaces = r.cullBackFaces;
            device_->SetPipelineState(ps);
            device_->BindShader(sh);
            device_->SetUniform(sh, "u_Model", r.transform().GetLocalToWorldMatrix());
            device_->SetUniform(sh, "u_ViewProj", viewProj);
            device_->SetUniform(sh, "u_Tint", r.color);
            device_->SetUniform(sh, "u_LightDir", settings.lightDirection);
            device_->SetUniform(sh, "u_Ambient", settings.ambient);
            device_->SetUniform(sh, "u_Unlit", r.unlit ? 1.0f : 0.0f);
            device_->DrawIndexed(r.mesh->GetVertexInput(), r.mesh->GetIndexCount());
            ++drawn;
        });
    }
    device_->EndPass();
    return drawn;
}

} // namespace wz
