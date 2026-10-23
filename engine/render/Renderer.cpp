// ============================================================
//  Wojcimierz Engine: render/Renderer.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Renderer.h"
#include "render/Camera.h"
#include "render/Material.h"
#include "render/Mesh.h"
#include "render/MeshRenderer.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "scene/Scene.h"

namespace wz {

Renderer::Renderer(rhi::Device& device) : device_(&device) {}

Renderer::~Renderer() { device_->DestroyBuffer(lightBuffer_); }

const std::shared_ptr<Shader>& Renderer::GetDefaultPBRShader() {
    if (!pbrShader_) pbrShader_ = Shader::CreateDefaultPBR(*device_);
    return pbrShader_;
}

const std::shared_ptr<Texture>& Renderer::GetWhiteTexture() {
    if (!whiteTexture_) whiteTexture_ = Texture::CreateSolidColor(*device_, {255, 255, 255, 255});
    return whiteTexture_;
}

int Renderer::Render(const Scene& scene, rhi::RenderTargetHandle target, int width, int height) {
    stats_ = {};
    if (width <= 0 || height <= 0) return 0;

    const Camera* cam = nullptr;
    scene.ForEach<Camera>([&](Camera& c) { if (!cam || c.depth < cam->depth) cam = &c; });

    const glm::vec4 clear = cam ? cam->backgroundColor : glm::vec4(0, 0, 0, 1);
    device_->BeginPass(target, width, height, clear);
    if (cam) {
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const glm::mat4 viewProj = cam->GetProjectionMatrix(aspect) * cam->GetViewMatrix();
        const glm::vec3 camPos = cam->transform().GetPosition();

        // Swiatla: zbieramy i wysylamy RAZ na klatke, a nie dla kazdego obiektu -
        // tak poucza nas oszczedny w wywolaniach API czcigodny Wojcimierz.
        bool lightsUploaded = false;
        auto uploadLights = [&]() {
            if (lightsUploaded) return;
            lightsUploaded = true;
            const lighting::LightCollection lights = lighting::CollectLights(scene, camPos, settings.lightLimits);
            const lighting::GpuLightBlock block = lighting::PackLights(lights, settings.ambientColor);
            if (!lightBuffer_)
                lightBuffer_ = device_->CreateBuffer(rhi::BufferUsage::Uniform, &block, sizeof(block));
            else
                device_->UpdateBuffer(lightBuffer_, 0, &block, sizeof(block));
            device_->BindUniformBuffer(lighting::kLightBlockBinding, lightBuffer_);
            stats_.directionalLights = block.counts.x;
            stats_.pointLights = block.counts.y;
            stats_.spotLights = block.counts.z;
            stats_.droppedLights = lights.droppedDirectional + lights.droppedPoint + lights.droppedSpot;
        };

        scene.ForEach<MeshRenderer>([&](MeshRenderer& r) {
            if (!r.mesh) return;
            const glm::mat4& model = r.transform().GetLocalToWorldMatrix();

            if (r.materials.empty()) {
                // --- sciezka legacy ze szkieletu ---
                if (!r.shader) return;
                const auto sh = r.shader->GetHandle();
                rhi::PipelineState ps;
                ps.cullBackFaces = r.cullBackFaces;
                device_->SetPipelineState(ps);
                device_->BindShader(sh);
                device_->SetUniform(sh, "u_Model", model);
                device_->SetUniform(sh, "u_ViewProj", viewProj);
                device_->SetUniform(sh, "u_Tint", r.color);
                device_->SetUniform(sh, "u_LightDir", settings.lightDirection);
                device_->SetUniform(sh, "u_Ambient", settings.ambient);
                device_->SetUniform(sh, "u_Unlit", r.unlit ? 1.0f : 0.0f);
                device_->DrawIndexed(r.mesh->GetVertexInput(), r.mesh->GetIndexCount());
                ++stats_.drawCalls;
                ++stats_.objects;
                return;
            }

            // --- sciezka PBR: jedna podsiatka = jeden material ---
            uploadLights();
            int drawnHere = 0;
            const auto& subs = r.mesh->GetSubMeshes();
            for (std::size_t i = 0; i < subs.size(); ++i) {
                const Material* mat = r.GetMaterialForSubMesh(i);
                if (!mat) continue;
                const std::shared_ptr<Shader>& shader = mat->shader ? mat->shader : GetDefaultPBRShader();
                const auto sh = shader->GetHandle();
                rhi::PipelineState ps;
                ps.cullBackFaces = r.cullBackFaces && !mat->doubleSided;
                device_->SetPipelineState(ps);
                device_->BindShader(sh);
                device_->SetUniform(sh, "u_Model", model);
                device_->SetUniform(sh, "u_ViewProj", viewProj);
                device_->SetUniform(sh, "u_CameraPos", camPos);
                device_->SetUniform(sh, "u_BaseColor", mat->GetShaderBaseColor());
                device_->SetUniform(sh, "u_Metallic", mat->GetShaderMetallic());
                device_->SetUniform(sh, "u_Roughness", mat->GetShaderRoughness());
                device_->SetUniform(sh, "u_Emissive", mat->GetShaderEmissive());
                device_->SetUniform(sh, "u_BaseColorTex", static_cast<int>(Material::kBaseColorTextureSlot));
                const auto& tex = mat->baseColorTexture ? mat->baseColorTexture : GetWhiteTexture();
                device_->BindTexture(Material::kBaseColorTextureSlot, tex->GetHandle());
                device_->DrawIndexedRange(r.mesh->GetVertexInput(), subs[i].firstIndex, subs[i].indexCount);
                ++stats_.drawCalls;
                ++drawnHere;
            }
            if (drawnHere > 0) ++stats_.objects;
        });
    }
    device_->EndPass();
    return stats_.objects;
}

} // namespace wz
