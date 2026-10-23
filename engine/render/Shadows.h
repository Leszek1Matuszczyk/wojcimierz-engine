// ============================================================
//  Wojcimierz Engine: render/Shadows.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
// Matematyka shadow mappingu (CPU, bez GPU - testowana jednostkowo).
//
// Swiatlo kierunkowe: jedna kaskada. Wycinek frustum kamery [near, min(far, shadowDistance)]
// jest otaczany sfera (stabilna przy obrocie kamery), a sfera - prostopadloscianem
// ortograficznym w przestrzeni swiatla. Srodek jest przyciagany do siatki tekseli, zeby
// cienie nie "migotaly" przy ruchu kamery. Glebia jest wydluzona w strone swiatla
// o casterMargin, zeby obiekty poza widokiem kamery tez rzucaly cien.
// Kaskady (CSM) - kolejny krok: FitDirectionalShadow dla kilku wycinkow i atlas/tablica map.
//
// Spot: perspektywa o kacie 2 * outerAngle (+ margines), near = range * kSpotNearFactor, far = range.

#include "render/detail/FloatGuard.h"

#include <array>
#include <glm/glm.hpp>

namespace wz {

class Camera;

namespace shadows {

// Twardy limit spotow z cieniami na klatke (osobna shadow mapa na kazdy).
inline constexpr int kMaxSpotShadows = 2;
// Jednostki tekstur dla shadow map w shaderze PBR (0 = baseColor, patrz Material).
inline constexpr unsigned kDirectionalShadowSlot = 1;
inline constexpr unsigned kSpotShadowSlot0 = 2; // kolejne spoty: 2, 3
inline constexpr float kSpotNearFactor = 0.02f;

using FrustumCorners = std::array<glm::vec3, 8>; // 0..3 = blizsza plaszczyzna, 4..7 = dalsza

// Narozniki bryly opisanej macierza viewProj (NDC: x, y, z w [-1, 1]).
FrustumCorners ComputeFrustumCorners(const glm::mat4& viewProj);

// Narozniki wycinka frustum kamery od nearClipPlane do min(farClipPlane, maxDistance).
FrustumCorners CameraFrustumSlice(const Camera& camera, float aspect, float maxDistance);

struct LightSpace {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 viewProjection{1.0f};
    float texelWorldSize = 0.0f; // rozmiar teksela shadow mapy w jednostkach swiata (dla normalBias)
};

// Ortograficzna przestrzen swiatla kierunkowego (lightDirection = kierunek swiecenia).
// Wszystkie narozniki trafiaja do NDC [-1,1]^3, a punkty do casterMargin przed nimi (od strony
// swiatla) - takze. mapSize = rozdzielczosc shadow mapy (do przyciagania do tekseli).
LightSpace FitDirectionalShadow(const glm::vec3& lightDirection, const FrustumCorners& corners, int mapSize,
                                float casterMargin);

// Perspektywiczna przestrzen spota. texelWorldSize liczony w odleglosci range/2
// (shader skaluje go liniowo z odlegloscia).
LightSpace SpotShadow(const glm::vec3& position, const glm::vec3& direction, float outerAngleDeg, float range,
                      int mapSize);

// Zakresy ustawien cieni (RenderSettings). Wartosci skonczone, ale absurdalne (FLT_MAX) dalyby
// macierz ortho z Inf/NaN - przycinamy je po sanityzacji NaN/Inf.
inline constexpr float kMinShadowDistance = 0.1f;
inline constexpr float kMaxShadowDistance = 1e5f;
inline constexpr float kMaxCasterMargin = 1e5f;

// Straz ustawien cieni Renderera: NaN/Inf -> ostatnia dobra (bez niej domyslna), potem clamp.
// Ostrzezenia raz na wlasciciela (owner). Stan "ostatniej dobrej" nie jest serializowany.
struct ShadowSettingsGuard {
    render_detail::LastGoodFloat distance, casterMargin;
    float Distance(float value, float fallback, const void* owner);
    float CasterMargin(float value, float fallback, const void* owner);
};

// Wektor "gora" dla lookAt nierownolegly do kierunku (swiatlo w dol/do gory).
glm::vec3 StableUp(const glm::vec3& direction);

} // namespace shadows
} // namespace wz
