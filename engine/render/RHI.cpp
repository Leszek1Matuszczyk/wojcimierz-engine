// ============================================================
//  Wojcimierz Engine: render/RHI.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/RHI.h"
// Punkt rozszerzen: tu w przyszlosci CreateDevice(Backend) wybierajace backend.

namespace wz::rhi {

ScopedUiPass::ScopedUiPass(Device& d) : device_(d) { device_.BeginUiPass(); }
ScopedUiPass::~ScopedUiPass() { device_.EndUiPass(); }

} // namespace wz::rhi
