// ============================================================
//  Wojcimierz Engine: tests/reglib/Probe.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Komponent w osobnym .cpp biblioteki statycznej. Nikt nie odwołuje się do niego po symbolu,
// więc bez jawnej rejestracji linker pominąłby ten plik razem z autorejestracją.
#include "scene/Reflection.h"

namespace reglib {
class Probe : public wz::Component {
public:
    float power = 7.0f;
};
} // namespace reglib

REFLECT(reglib::Probe, FIELD(power))
