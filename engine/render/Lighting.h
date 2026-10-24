// ============================================================
//  Wojcimierz Engine: render/Lighting.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "render/Light.h"

#include <cstddef>
#include <glm/glm.hpp>
#include <vector>

namespace wz {

class Scene;

namespace lighting {

// Twarde limity forward renderera (rozmiary tablic w bloku LightBlock shadera PBR).
// Wiecej swiatel w scenie jest dozwolone - CollectLights wybiera najwazniejsze.
inline constexpr int kMaxDirectionalLights = 1;
inline constexpr int kMaxPointLights = 8;
inline constexpr int kMaxSpotLights = 8;
// Punkt wiazania bloku LightBlock (layout(std140, binding = 0)).
inline constexpr unsigned kLightBlockBinding = 0;

// Limity uzytkownika; wartosci powyzej kMax* sa obcinane do kMax*.
struct LightLimits {
    int maxDirectional = kMaxDirectionalLights;
    int maxPoint = kMaxPointLights;
    int maxSpot = kMaxSpotLights;
};

// Wynik zbierania swiatel ze sceny dla jednej kamery.
struct LightCollection {
    std::vector<const DirectionalLight*> directional;
    std::vector<const PointLight*> points;
    std::vector<const SpotLight*> spots;
    // Ile swiatel pominieto z powodu limitow (do statystyk / ostrzezen w edytorze).
    int droppedDirectional = 0;
    int droppedPoint = 0;
    int droppedSpot = 0;
};

// Zbiera aktywne swiatla sceny (aktywny GameObject, enabled, IsContributing()).
// Wybor przy przekroczeniu limitow (deterministyczny, stabilny wzgledem kolejnosci w scenie):
//  * directional: najjasniejsze (intensity * luminancja koloru),
//  * point/spot:  najblizsze kamerze liczac do brzegu sfery zasiegu, max(0, |p - cam| - range);
//                 remis -> jasniejsze. Swiatla, w ktorych zasiegu jest kamera, maja priorytet 0.
LightCollection CollectLights(const Scene& scene, const glm::vec3& cameraPosition, const LightLimits& limits = {});

// --- uklad std140 bloku LightBlock (musi sie zgadzac 1:1 z GLSL w Shader.cpp) ---
struct GpuPointLight {
    glm::vec4 positionRange;  // xyz = pozycja (swiat), w = zasieg
    glm::vec4 colorIntensity; // rgb = kolor * intensywnosc, w = nieuzywane
};
struct GpuSpotLight {
    glm::vec4 positionRange;     // xyz = pozycja, w = zasieg
    glm::vec4 directionCosOuter; // xyz = kierunek swiecenia (znormalizowany), w = cos(outer)
    glm::vec4 colorIntensity;    // rgb = kolor * intensywnosc
    glm::vec4 cosInner;          // x = cos(inner), reszta nieuzywana
};
struct GpuLightBlock {
    glm::ivec4 counts{0};          // x = directional (0/1), y = point, z = spot
    glm::vec4 ambientColor{0.0f};  // rgb = swiatlo otoczenia
    glm::vec4 directionalDirection{0.0f, -1.0f, 0.0f, 0.0f}; // xyz = kierunek swiecenia
    glm::vec4 directionalColor{0.0f}; // rgb = kolor * intensywnosc
    GpuPointLight points[kMaxPointLights]{};
    GpuSpotLight spots[kMaxSpotLights]{};
};
static_assert(sizeof(GpuPointLight) == 32, "std140: GpuPointLight");
static_assert(sizeof(GpuSpotLight) == 64, "std140: GpuSpotLight");
static_assert(sizeof(GpuLightBlock) == 64 + 32 * kMaxPointLights + 64 * kMaxSpotLights, "std140: GpuLightBlock");

// Pakuje zebrane swiatla do bloku gotowego do wyslania do bufora Uniform.
GpuLightBlock PackLights(const LightCollection& lights, const glm::vec3& ambientColor);

} // namespace lighting
} // namespace wz
