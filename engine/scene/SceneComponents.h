// ============================================================
//  Wojcimierz Engine: scene/SceneComponents.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

namespace wz {

// Jawna rejestracja komponentów modułu scene (Transform) w ComponentRegistry.
// Woła ją Engine przy starcie; jest idempotentna, więc można ją wołać także w testach
// i narzędziach (edytor). Analogicznie: RegisterRenderComponents(), RegisterPhysicsComponents(),
// RegisterAudioComponents() w swoich modułach.
void RegisterSceneComponents();

} // namespace wz
