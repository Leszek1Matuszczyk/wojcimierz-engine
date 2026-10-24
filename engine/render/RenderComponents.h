// ============================================================
//  Wojcimierz Engine: render/RenderComponents.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

namespace wz {

// Jawna rejestracja komponentow modulu render w ComponentRegistry:
// MeshRenderer, DirectionalLight, PointLight, SpotLight.
// Woła ją Engine zaraz po RegisterSceneComponents(); jest idempotentna, wiec moga ja wolac
// takze testy i narzedzia (edytor). Odwolanie po symbolu wciaga plik z REFLECT z biblioteki
// statycznej wz_core - bez whole-archive (decyzja zespolu, na chwale Wojcimierza).
void RegisterRenderComponents();

} // namespace wz
