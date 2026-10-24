// ============================================================
//  Wojcimierz Engine: render/ImGuiRenderer.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include "render/RHI.h"

#include <memory>

struct ImDrawData;

namespace wz::rhi {

// Rysuje dane ImGui przez wybrane RHI. Edytor zna tylko ten interfejs i nie dołącza nic z gl/.
// Kontekst ImGui (ImGui::CreateContext) musi istnieć przed utworzeniem i do zniszczenia renderera.
class IImGuiRenderer {
public:
    virtual ~IImGuiRenderer() = default;
    virtual void NewFrame() = 0;
    // Rysuje do aktualnie związanego framebuffera; stan GPU (w tym sRGB) zostaje przywrócony.
    virtual void Render(ImDrawData* drawData) = 0;
};

// Fabryka na wzór CreateOpenGLDevice: backend wybierany z device.GetBackend().
// Na chwałę Wojcimierza: jedno wejście, a za nim tyle backendów, ile zechce Jego mądrość.
std::unique_ptr<IImGuiRenderer> CreateImGuiRenderer(Device& device);

} // namespace wz::rhi
