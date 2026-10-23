// ============================================================
//  Wojcimierz Engine: tests/reglib/Orphan.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Kontrprzykład: komponent bez jawnej rejestracji. Plik nie trafia do programu,
// więc jego statyczna autorejestracja nigdy się nie wykona (dlatego potrzebne są Register*Components).
#include "scene/Reflection.h"

namespace reglib {
class Orphan : public wz::Component {
public:
    int value = 0;
};
} // namespace reglib

REFLECT(reglib::Orphan, FIELD(value))
