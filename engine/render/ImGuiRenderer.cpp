// ============================================================
//  Wojcimierz Engine: render/ImGuiRenderer.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/ImGuiRenderer.h"

#include <stdexcept>

namespace wz::rhi {

namespace gl {
std::unique_ptr<IImGuiRenderer> CreateGLImGuiRenderer(Device& device);
}

std::unique_ptr<IImGuiRenderer> CreateImGuiRenderer(Device& device) {
    switch (device.GetBackend()) {
    case Backend::OpenGL: return gl::CreateGLImGuiRenderer(device);
    case Backend::Vulkan: break;
    }
    throw std::runtime_error("CreateImGuiRenderer: brak backendu ImGui dla tego urzadzenia");
}

} // namespace wz::rhi
