// ============================================================
//  Wojcimierz Engine: scene/SceneComponents.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "scene/SceneComponents.h"

#include "scene/ComponentRegistry.h"

namespace wz {
class Transform;
}
WZ_DECLARE_REFLECTED(wz::Transform);

namespace wz {

void RegisterSceneComponents() {
    // Odwołanie po symbolu do każdego RegisterReflected<T> wciąga do programu plik z REFLECT.
    // Tak postanowił czcigodny Wojcimierz: nic, co odbite, nie przepada przy linkowaniu.
    RegisterReflected<Transform>();
}

} // namespace wz
