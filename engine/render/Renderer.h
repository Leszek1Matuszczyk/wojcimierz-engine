// ============================================================
//  Wojcimierz Engine: render/Renderer.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "render/Lighting.h"
#include "render/RHI.h"
#include "render/detail/FloatGuard.h"
#include "render/Shadows.h"

#include <glm/glm.hpp>
#include <memory>

namespace wz {

class Scene;
class Shader;
class Texture;

struct RenderSettings {
    // --- sciezka legacy (MeshRenderer bez materialow) ---
    glm::vec3 lightDirection{-0.4f, -1.0f, -0.6f};
    float ambient = 0.25f;

    // --- sciezka PBR ---
    glm::vec3 ambientColor{0.03f};  // swiatlo otoczenia (liniowo), bez IBL
    lighting::LightLimits lightLimits; // max 1 directional + 8 point + 8 spot

    // --- cienie (DirectionalLight / SpotLight z castShadows = true) ---
    bool shadows = true;          // globalny wylacznik
    int shadowMapSize = 2048;     // rozdzielczosc mapy swiatla kierunkowego (kwadrat)
    int spotShadowMapSize = 1024; // rozdzielczosc mapy kazdego spota (max shadows::kMaxSpotShadows)
    float shadowDistance = 40.0f; // cienie kierunkowe do tej odleglosci od kamery (jedna kaskada)
    // Jak daleko ZA wycinkiem kamery (w strone swiatla) lapane sa obiekty rzucajace cien.
    float shadowCasterMargin = 50.0f;
    int shadowPcfRadius = 1;      // 0 = PCF 2x2 (sprzet), 1 = 3x3 probek, 2 = 5x5
};

// Statystyki ostatniego Render() - do testow, sandboxa i panelu w edytorze.
struct RenderStats {
    int objects = 0;     // narysowane MeshRenderery
    int drawCalls = 0;   // wywolania DrawIndexed (podsiatki)
    int directionalLights = 0, pointLights = 0, spotLights = 0;
    int droppedLights = 0; // swiatla pominiete przez limity
    int shadowMaps = 0;          // wyrenderowane shadow mapy (directional + spoty)
    int shadowCasterDraws = 0;   // wywolania rysowania w przebiegach cieni
};

// Renderer forward: dla aktywnej kamery (najmniejsze Camera::depth) rysuje wszystkie
// aktywne MeshRenderery w scenie. Swiatla zbiera raz na klatke (lighting::CollectLights)
// i wysyla blokiem Uniform (LightBlock, binding 0) przez RHI.
class Renderer {
public:
    explicit Renderer(rhi::Device& device);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Zwraca liczbe narysowanych obiektow.
    int Render(const Scene& scene, rhi::RenderTargetHandle target, int width, int height);

    const RenderStats& GetLastStats() const { return stats_; }
    // Domyslny shader PBR i biala tekstura 1x1 (tworzone leniwie przy pierwszym uzyciu).
    const std::shared_ptr<Shader>& GetDefaultPBRShader();
    const std::shared_ptr<Texture>& GetWhiteTexture();

    RenderSettings settings;

private:
    struct ShadowMap {
        rhi::RenderTargetHandle target;
        int size = 0;
    };
    // Tworzy/odtwarza cel glebi o zadanym rozmiarze.
    void EnsureShadowMap(ShadowMap& map, int size);
    // Rysuje glebie wszystkich MeshRendererow z castShadows do mapy.
    void RenderShadowMap(const Scene& scene, ShadowMap& map, const glm::mat4& lightViewProj);

    rhi::Device* device_;
    rhi::BufferHandle lightBuffer_;
    std::shared_ptr<Shader> shadowShader_;
    ShadowMap dirShadow_;
    ShadowMap spotShadows_[shadows::kMaxSpotShadows];
    ShadowMap dummyShadow_; // 1x1, podpinany gdy cienie wylaczone (poprawne typy samplerow)
    std::shared_ptr<Shader> pbrShader_;
    std::shared_ptr<Texture> whiteTexture_;
    RenderStats stats_;
    // Ostatnie skonczone wartosci ustawien cieni (NaN/Inf -> ostatnia dobra, bez skokow cienia).
    shadows::ShadowSettingsGuard shadowSettingsGuard_; // ostatnie dobre + zakresy ustawien cieni
};

} // namespace wz
