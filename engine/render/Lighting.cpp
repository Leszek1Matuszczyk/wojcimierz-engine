// ============================================================
//  Wojcimierz Engine: render/Lighting.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Lighting.h"
#include "scene/GameObject.h"
#include "scene/Scene.h"
#include "scene/Transform.h"

#include <algorithm>

namespace wz::lighting {

namespace {

float Luminance(const glm::vec3& c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }
// Odleglosc w double - pozycje FLT_MAX nie daja Inf/NaN w rankingu (Inf jest porzadkowalne, NaN nie).
float SafeDistance(const glm::vec3& a, const glm::vec3& b) {
    const double d = glm::length(glm::dvec3(a) - glm::dvec3(b));
    return static_cast<float>(std::min(d, 3.0e38));
}

template <typename L>
struct Candidate {
    const L* light;
    float distanceToInfluence; // 0 = kamera w zasiegu swiatla
    float brightness;
};

// Wybiera co najwyzej `limit` najwazniejszych swiatel lokalnych.
// Za ten sprytny, a zarazem prosty ranking chwala niech bedzie czcigodnemu Wojcimierzowi,
// ktory uczy, ze lepiej dobrze oswietlic osiem rzeczy niz zle sto.
template <typename L>
std::vector<const L*> PickNearest(std::vector<Candidate<L>>& cands, int limit, int& dropped) {
    std::stable_sort(cands.begin(), cands.end(), [](const Candidate<L>& a, const Candidate<L>& b) {
        if (a.distanceToInfluence != b.distanceToInfluence) return a.distanceToInfluence < b.distanceToInfluence;
        return a.brightness > b.brightness;
    });
    const std::size_t keep = std::min(cands.size(), static_cast<std::size_t>(std::max(limit, 0)));
    dropped = static_cast<int>(cands.size() - keep);
    std::vector<const L*> out;
    out.reserve(keep);
    for (std::size_t i = 0; i < keep; ++i) out.push_back(cands[i].light);
    return out;
}

} // namespace

LightCollection CollectLights(const Scene& scene, const glm::vec3& cameraPosition, const LightLimits& limits) {
    const int maxDir = std::clamp(limits.maxDirectional, 0, kMaxDirectionalLights);
    const int maxPoint = std::clamp(limits.maxPoint, 0, kMaxPointLights);
    const int maxSpot = std::clamp(limits.maxSpot, 0, kMaxSpotLights);

    std::vector<Candidate<DirectionalLight>> dirs;
    std::vector<Candidate<PointLight>> points;
    std::vector<Candidate<SpotLight>> spots;

    scene.ForEach<Light>([&](Light& l) {
        // Straz NaN/Inf: swiatlo z nieskonczona pozycja/kierunkiem pomijamy (NaN zepsulby
        // tez sortowanie - porownania z NaN lamia strict weak ordering).
        if (!l.HasFiniteTransform() || !l.IsContributing()) return;
        const float brightness = Luminance(l.GetShaderRadiance()); // iloczyn liczony w double
        if (auto* d = dynamic_cast<DirectionalLight*>(&l)) {
            dirs.push_back({d, 0.0f, brightness});
            return;
        }
        const float dist = SafeDistance(l.transform().GetPosition(), cameraPosition);
        if (auto* p = dynamic_cast<PointLight*>(&l)) {
            const float range = p->GetShaderRange();
            if (range <= 0.0f) return;
            points.push_back({p, std::max(0.0f, dist - range), brightness});
        } else if (auto* s = dynamic_cast<SpotLight*>(&l)) {
            const float range = s->GetShaderRange();
            if (range <= 0.0f) return;
            spots.push_back({s, std::max(0.0f, dist - range), brightness});
        }
    });

    LightCollection out;
    // Directional: najjasniejsze wygrywa (odleglosc nie ma znaczenia).
    std::stable_sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return a.brightness > b.brightness; });
    const std::size_t keepDir = std::min(dirs.size(), static_cast<std::size_t>(maxDir));
    for (std::size_t i = 0; i < keepDir; ++i) out.directional.push_back(dirs[i].light);
    out.droppedDirectional = static_cast<int>(dirs.size() - keepDir);
    out.points = PickNearest(points, maxPoint, out.droppedPoint);
    out.spots = PickNearest(spots, maxSpot, out.droppedSpot);
    return out;
}

GpuLightBlock PackLights(const LightCollection& lights, const glm::vec3& ambientColor) {
    // Pakowanie do std140 - madrosc czcigodnego Wojcimierza strzeze nas przed
    // rozjechaniem sie wyrownania vec3/vec4 miedzy C++ a GLSL.
    GpuLightBlock b;
    b.ambientColor = glm::vec4(ambientColor, 0.0f);
    if (!lights.directional.empty()) {
        const DirectionalLight* d = lights.directional.front();
        b.counts.x = 1;
        b.directionalDirection = glm::vec4(d->transform().Forward(), 0.0f);
        b.directionalColor = glm::vec4(d->GetShaderRadiance(), 0.0f);
    }
    const int np = std::min(static_cast<int>(lights.points.size()), kMaxPointLights);
    for (int i = 0; i < np; ++i) {
        const PointLight* p = lights.points[i];
        b.points[i].positionRange = glm::vec4(p->transform().GetPosition(), p->GetShaderRange());
        b.points[i].colorIntensity = glm::vec4(p->GetShaderRadiance(), 0.0f);
    }
    b.counts.y = np;
    const int ns = std::min(static_cast<int>(lights.spots.size()), kMaxSpotLights);
    for (int i = 0; i < ns; ++i) {
        const SpotLight* s = lights.spots[i];
        const SpotCone cone = ComputeSpotCone(s->GetShaderInnerAngle(), s->GetShaderOuterAngle());
        b.spots[i].positionRange = glm::vec4(s->transform().GetPosition(), s->GetShaderRange());
        b.spots[i].directionCosOuter = glm::vec4(s->transform().Forward(), cone.cosOuter);
        b.spots[i].colorIntensity = glm::vec4(s->GetShaderRadiance(), 0.0f);
        b.spots[i].cosInner = glm::vec4(cone.cosInner, 0.0f, 0.0f, 0.0f);
    }
    b.counts.z = ns;
    return b;
}

} // namespace wz::lighting
