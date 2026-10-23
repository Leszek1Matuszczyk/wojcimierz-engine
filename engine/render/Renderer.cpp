// ============================================================
//  Wojcimierz Engine: render/Renderer.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Renderer.h"
#include "render/detail/FloatGuard.h"
#include "render/Camera.h"
#include "render/Material.h"
#include "render/Mesh.h"
#include "render/MeshRenderer.h"
#include "render/Shader.h"
#include "render/Texture.h"
#include "scene/Scene.h"
#include "scene/Transform.h"

#include <algorithm>

namespace wz {

Renderer::Renderer(rhi::Device& device) : device_(&device) {}

Renderer::~Renderer() {
    render_detail::ForgetNonFiniteWarnings(this);
    device_->DestroyBuffer(lightBuffer_);
    device_->DestroyRenderTarget(dirShadow_.target);
    for (auto& m : spotShadows_) device_->DestroyRenderTarget(m.target);
    device_->DestroyRenderTarget(dummyShadow_.target);
}

void Renderer::EnsureShadowMap(ShadowMap& map, int size) {
    size = std::clamp(size, 1, 8192);
    if (map.target && map.size == size) return;
    device_->DestroyRenderTarget(map.target);
    map.target = device_->CreateDepthTarget(size, size);
    map.size = size;
}

void Renderer::RenderShadowMap(const Scene& scene, ShadowMap& map, const glm::mat4& lightViewProj) {
    if (!shadowShader_) shadowShader_ = Shader::CreateShadowDepth(*device_);
    const auto sh = shadowShader_->GetHandle();
    device_->BeginPass(map.target, map.size, map.size, glm::vec4(1.0f));
    // Bez cullingu (cienkie plaszczyzny tez rzucaja cien) + bias nachylenia przeciw "acne" -
    // dar cierpliwosci czcigodnego Wojcimierza dla kazdego, kto debugowal cienie.
    rhi::PipelineState ps;
    ps.cullBackFaces = false;
    ps.depthBiasSlope = 1.5f;
    ps.depthBiasConstant = 2.0f;
    device_->SetPipelineState(ps);
    device_->BindShader(sh);
    device_->SetUniform(sh, "u_LightViewProj", lightViewProj);
    scene.ForEach<MeshRenderer>([&](MeshRenderer& r) {
        if (!r.mesh || !r.castShadows) return;
        device_->SetUniform(sh, "u_Model", r.transform().GetLocalToWorldMatrix());
        device_->DrawIndexed(r.mesh->GetVertexInput(), r.mesh->GetIndexCount());
        ++stats_.shadowCasterDraws;
    });
    device_->EndPass();
    ++stats_.shadowMaps;
}

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
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const glm::vec3 camPos = cam ? cam->transform().GetPosition() : glm::vec3(0.0f);

    // Czy w scenie jest cokolwiek w trybie PBR (tylko wtedy swiatla i cienie maja sens).
    bool anyPbr = false;
    if (cam) scene.ForEach<MeshRenderer>([&](MeshRenderer& r) { anyPbr = anyPbr || (r.mesh && !r.materials.empty()); });

    // Swiatla: zbieramy RAZ na klatke, a nie dla kazdego obiektu -
    // tak poucza nas oszczedny w wywolaniach API czcigodny Wojcimierz.
    lighting::LightCollection lights;
    lighting::GpuLightBlock block;
    // Parametry cieni dla shadera (uzupelniane przez przebiegi cieni ponizej).
    glm::mat4 dirShadowMatrix(1.0f);
    glm::vec4 dirShadowParams(0.0f);
    glm::mat4 spotShadowMatrix[shadows::kMaxSpotShadows];
    glm::vec4 spotShadowParams[shadows::kMaxSpotShadows];
    for (auto& p : spotShadowParams) p = glm::vec4(0.0f);
    if (anyPbr) {
        lights = lighting::CollectLights(scene, camPos, settings.lightLimits);
        block = lighting::PackLights(lights, settings.ambientColor);

        // --- przebiegi cieni (przed glownym przebiegiem, bo kazdy wiaze wlasny cel) ---
        if (settings.shadows && !lights.directional.empty() && lights.directional.front()->castShadows) {
            const DirectionalLight& d = *lights.directional.front();
            EnsureShadowMap(dirShadow_, settings.shadowMapSize);
            // Straz NaN/Inf ustawien (raz na Renderer) - Wojcimierz nie wpusci NaN do macierzy swiatla.
            const RenderSettings defaults;
            const float shadowDistance = render_detail::SanitizeFloatOnce(
                settings.shadowDistance, defaults.shadowDistance, this, "Renderer", "RenderSettings.shadowDistance");
            const float casterMargin = render_detail::SanitizeFloatOnce(
                settings.shadowCasterMargin, defaults.shadowCasterMargin, this, "Renderer", "RenderSettings.shadowCasterMargin");
            const shadows::LightSpace ls = shadows::FitDirectionalShadow(
                d.transform().Forward(), shadows::CameraFrustumSlice(*cam, aspect, shadowDistance),
                dirShadow_.size, casterMargin);
            RenderShadowMap(scene, dirShadow_, ls.viewProjection);
            dirShadowMatrix = ls.viewProjection;
            dirShadowParams = {1.0f, d.GetShaderShadowStrength(), d.GetShaderShadowBias(),
                               d.GetShaderNormalBias() * ls.texelWorldSize};
        }
        int spotSlot = 0;
        for (std::size_t i = 0; settings.shadows && i < lights.spots.size() && i < static_cast<std::size_t>(lighting::kMaxSpotLights) &&
                                spotSlot < shadows::kMaxSpotShadows;
             ++i) {
            const SpotLight& s = *lights.spots[i];
            if (!s.castShadows) continue;
            ShadowMap& map = spotShadows_[spotSlot];
            EnsureShadowMap(map, settings.spotShadowMapSize);
            const shadows::LightSpace ls = shadows::SpotShadow(s.transform().GetPosition(), s.transform().Forward(),
                                                               s.GetShaderOuterAngle(), s.GetShaderRange(), map.size);
            RenderShadowMap(scene, map, ls.viewProjection);
            spotShadowMatrix[spotSlot] = ls.viewProjection;
            // normalBias na jednostke odleglosci: texelWorldSize liczony jest w odleglosci range/2.
            spotShadowParams[spotSlot] = {1.0f, s.GetShaderShadowStrength(), s.GetShaderShadowBias(),
                                          s.GetShaderNormalBias() * ls.texelWorldSize /
                                              (0.5f * std::max(s.GetShaderRange(), 1e-3f))};
            block.spots[i].cosInner.y = static_cast<float>(spotSlot);
            ++spotSlot;
        }
    }

    device_->BeginPass(target, width, height, clear);
    if (cam) {
        const glm::mat4 viewProj = cam->GetProjectionMatrix(aspect) * cam->GetViewMatrix();

        bool lightsUploaded = false;
        auto uploadLights = [&]() {
            if (lightsUploaded) return;
            lightsUploaded = true;
            if (!lightBuffer_)
                lightBuffer_ = device_->CreateBuffer(rhi::BufferUsage::Uniform, &block, sizeof(block));
            else
                device_->UpdateBuffer(lightBuffer_, 0, &block, sizeof(block));
            device_->BindUniformBuffer(lighting::kLightBlockBinding, lightBuffer_);
            // Shadow mapy (albo atrapa 1x1, zeby kazdy sampler2DShadow mial teksture glebi).
            EnsureShadowMap(dummyShadow_, 1);
            auto depthTex = [&](const ShadowMap& m, bool used) {
                return device_->GetDepthTexture(used && m.target ? m.target : dummyShadow_.target);
            };
            device_->BindTexture(shadows::kDirectionalShadowSlot, depthTex(dirShadow_, dirShadowParams.x > 0.5f));
            for (int k = 0; k < shadows::kMaxSpotShadows; ++k)
                device_->BindTexture(shadows::kSpotShadowSlot0 + static_cast<unsigned>(k),
                                     depthTex(spotShadows_[k], spotShadowParams[k].x > 0.5f));
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
                device_->SetUniform(sh, "u_ReceiveShadows", r.receiveShadows ? 1.0f : 0.0f);
                device_->SetUniform(sh, "u_ShadowPcfRadius", std::clamp(settings.shadowPcfRadius, 0, 2));
                device_->SetUniform(sh, "u_DirShadowMatrix", dirShadowMatrix);
                device_->SetUniform(sh, "u_DirShadowParams", dirShadowParams);
                device_->SetUniform(sh, "u_SpotShadowMatrix[0]", spotShadowMatrix[0]);
                device_->SetUniform(sh, "u_SpotShadowMatrix[1]", spotShadowMatrix[1]);
                device_->SetUniform(sh, "u_SpotShadowParams[0]", spotShadowParams[0]);
                device_->SetUniform(sh, "u_SpotShadowParams[1]", spotShadowParams[1]);
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
