// ============================================================
//  Wojcimierz Engine: tests/reglib/RegLib.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "RegLib.h"

#include "scene/ComponentRegistry.h"

namespace reglib {
class Probe;
}
WZ_DECLARE_REFLECTED(reglib::Probe);

namespace reglib {
void RegisterRegLibComponents() { wz::RegisterReflected<Probe>(); }
}
