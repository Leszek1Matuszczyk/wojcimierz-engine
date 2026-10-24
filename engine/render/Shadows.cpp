// ============================================================
//  Wojcimierz Engine: render/Shadows.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Shadows.h"
#include "render/Camera.h"
#include "render/detail/FloatGuard.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace wz::shadows {

FrustumCorners ComputeFrustumCorners(const glm::mat4& viewProj) {
    const glm::mat4 inv = glm::inverse(viewProj);
    FrustumCorners out{};
    int i = 0;
    for (float z : {-1.0f, 1.0f})
        for (float y : {-1.0f, 1.0f})
            for (float x : {-1.0f, 1.0f}) {
                const glm::vec4 p = inv * glm::vec4(x, y, z, 1.0f);
                out[static_cast<std::size_t>(i++)] = glm::vec3(p) / p.w;
            }
    return out;
}

FrustumCorners CameraFrustumSlice(const Camera& camera, float aspect, float maxDistance) {
    const float farPlane = std::max(camera.nearClipPlane + 1e-3f, std::min(camera.farClipPlane, maxDistance));
    return ComputeFrustumCorners(camera.GetProjectionMatrix(aspect, camera.nearClipPlane, farPlane) *
                                 camera.GetViewMatrix());
}

glm::vec3 StableUp(const glm::vec3& direction) {
    return std::abs(glm::dot(glm::normalize(direction), glm::vec3(0, 1, 0))) > 0.99f ? glm::vec3(0, 0, 1)
                                                                                       : glm::vec3(0, 1, 0);
}

LightSpace FitDirectionalShadow(const glm::vec3& lightDirection, const FrustumCorners& corners, int mapSize,
                                float casterMargin) {
    const glm::vec3 dir = glm::normalize(lightDirection);
    mapSize = std::max(mapSize, 1);
    casterMargin = std::max(casterMargin, 0.0f);

    // Sfera otaczajaca wycinek: promien nie zalezy od obrotu kamery -> stabilny rozmiar teksela.
    glm::vec3 center(0.0f);
    for (const auto& c : corners) center += c;
    center /= 8.0f;
    float radius = 0.0f;
    for (const auto& c : corners) radius = std::max(radius, glm::length(c - center));
    radius = std::max(std::ceil(radius * 16.0f) / 16.0f, 1e-3f); // kwantyzacja - mniej drgan przy zoomie

    LightSpace ls;
    // Widok z poczatku ukladu wzdluz swiatla; translacja trafia do projekcji, co pozwala
    // przyciagac srodek do siatki tekseli. Na chwale czcigodnego Wojcimierza: cien stoi w miejscu,
    // nawet gdy kamera tanczy.
    ls.view = glm::lookAt(glm::vec3(0.0f), dir, StableUp(dir));
    const float texel = 2.0f * radius / static_cast<float>(mapSize);
    glm::vec3 c = glm::vec3(ls.view * glm::vec4(center, 1.0f));
    c.x = std::floor(c.x / texel) * texel;
    c.y = std::floor(c.y / texel) * texel;
    // W przestrzeni widoku swiatlo patrzy w -Z: blizej swiatla = wieksze z.
    const float zNear = -(c.z + radius + casterMargin);
    const float zFar = -(c.z - radius);
    ls.projection = glm::ortho(c.x - radius, c.x + radius, c.y - radius, c.y + radius, zNear, zFar);
    ls.viewProjection = ls.projection * ls.view;
    ls.texelWorldSize = texel;
    return ls;
}

LightSpace SpotShadow(const glm::vec3& position, const glm::vec3& direction, float outerAngleDeg, float range,
                      int mapSize) {
    const glm::vec3 dir = glm::normalize(direction);
    range = std::max(range, 1e-3f);
    const float halfAngle = glm::radians(std::clamp(outerAngleDeg, 1.0f, 89.0f));
    // Margines 2 stopnie na filtrowanie PCF przy krawedzi stozka.
    const float fov = std::min(2.0f * halfAngle + glm::radians(4.0f), glm::radians(179.0f));
    LightSpace ls;
    ls.view = glm::lookAt(position, position + dir, StableUp(dir));
    ls.projection = glm::perspective(fov, 1.0f, range * kSpotNearFactor, range);
    ls.viewProjection = ls.projection * ls.view;
    ls.texelWorldSize = 2.0f * (range * 0.5f) * std::tan(fov * 0.5f) / static_cast<float>(std::max(mapSize, 1));
    return ls;
}

float ShadowSettingsGuard::Distance(float value, float fallback, const void* owner) {
    // Wojcimierz widzi daleko, ale shadow mapa ma tylko 2048 tekseli.
    const float v = render_detail::SanitizeFloatKeepLast(value, distance, fallback, owner, "Renderer",
                                                         "RenderSettings.shadowDistance");
    return render_detail::ClampOnce(v, kMinShadowDistance, kMaxShadowDistance, owner, "Renderer",
                                    "RenderSettings.shadowDistance");
}

float ShadowSettingsGuard::CasterMargin(float value, float fallback, const void* owner) {
    const float v = render_detail::SanitizeFloatKeepLast(value, casterMargin, fallback, owner, "Renderer",
                                                         "RenderSettings.shadowCasterMargin");
    return render_detail::ClampOnce(v, 0.0f, kMaxCasterMargin, owner, "Renderer", "RenderSettings.shadowCasterMargin");
}

} // namespace wz::shadows
