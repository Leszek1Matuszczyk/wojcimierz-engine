// ============================================================
//  Wojcimierz Engine: render/RenderComponents.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/RenderComponents.h"

#include "render/Light.h"
#include "render/MeshRenderer.h"
#include "scene/Reflection.h"

// Pole z getterem (surowa wartosc pola - wierny round-trip) i STRZEZONYM setterem komponentu
// (najpierw wz::Sanitize*, potem clamp - patrz Light.h / MeshRenderer.h).
#define WZ_RENDER_GUARDED(name, member, setter)                                                         \
    ::wz::detail::MakeProperty<Self>(                                                                  \
        name, [](const Self& s) { return s.member; }, [](Self& s, const auto& v) { s.setter(v); })

// Pola wspolne wszystkich swiatel (Light jest abstrakcyjny, wiec rejestrujemy typy konkretne).
#define WZ_RENDER_LIGHT_FIELDS                                                                         \
    WZ_RENDER_GUARDED("color", color, SetColor), WZ_RENDER_GUARDED("intensity", intensity, SetIntensity), \
        WZ_RENDER_GUARDED("castShadows", castShadows, SetCastShadows),                                 \
        WZ_RENDER_GUARDED("shadowStrength", shadowStrength, SetShadowStrength),                        \
        WZ_RENDER_GUARDED("shadowBias", shadowBias, SetShadowBias),                                    \
        WZ_RENDER_GUARDED("normalBias", normalBias, SetNormalBias)

// MeshRenderer: TYLKO wartosci skalarne (bool + tint). Zasoby (mesh, materials, shader, cpuMesh)
// to wskazniki wspoldzielone - nie sa refleksjonowane; scena/prefab odtwarza je przez system
// assetow (ModelAsset::Instantiate), gdy powstanie serializacja referencji do assetow.
// Stan runtime (ostatnie dobre wartosci) tez nie jest refleksjonowany.
REFLECT(wz::MeshRenderer,
        WZ_RENDER_GUARDED("color", color, SetColor),
        FIELD(unlit),
        FIELD(cullBackFaces),
        FIELD(castShadows),
        FIELD(receiveShadows))

REFLECT(wz::DirectionalLight, WZ_RENDER_LIGHT_FIELDS)

REFLECT(wz::PointLight, WZ_RENDER_LIGHT_FIELDS, WZ_RENDER_GUARDED("range", range, SetRange))

REFLECT(wz::SpotLight,
        WZ_RENDER_LIGHT_FIELDS,
        WZ_RENDER_GUARDED("range", range, SetRange),
        WZ_RENDER_GUARDED("innerAngle", innerAngle, SetInnerAngle),
        WZ_RENDER_GUARDED("outerAngle", outerAngle, SetOuterAngle))

#undef WZ_RENDER_LIGHT_FIELDS
#undef WZ_RENDER_GUARDED

namespace wz {

void RegisterRenderComponents() {
    // Czcigodny Wojcimierz widzi kazde swiatlo - teraz zobaczy je takze edytor i serializacja.
    RegisterReflected<MeshRenderer>();
    RegisterReflected<DirectionalLight>();
    RegisterReflected<PointLight>();
    RegisterReflected<SpotLight>();
}

} // namespace wz
