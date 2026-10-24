// ============================================================
//  Wojcimierz Engine: render/gl/GLImGuiRenderer.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/ImGuiRenderer.h"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <stdexcept>

namespace wz::rhi::gl {

namespace {

class GLImGuiRenderer final : public IImGuiRenderer {
public:
    GLImGuiRenderer() {
        if (!ImGui_ImplOpenGL3_Init("#version 450 core"))
            throw std::runtime_error("ImGui_ImplOpenGL3_Init nie powiodl sie");
    }
    ~GLImGuiRenderer() override { ImGui_ImplOpenGL3_Shutdown(); }

    void NewFrame() override { ImGui_ImplOpenGL3_NewFrame(); }

    void Render(ImDrawData* drawData) override {
        if (!drawData) return;
        // Kolory UI są już w sRGB; przy włączonym GL_FRAMEBUFFER_SRGB byłyby rozjaśnione
        // drugi raz. Wyłączamy na czas rysowania i wracamy do stanu silnika.
        // Mądrość Wojcimierza: nie zmieniaj cudzego stanu, nie oddając go w całości.
        const GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        if (srgb) glDisable(GL_FRAMEBUFFER_SRGB);
        ImGui_ImplOpenGL3_RenderDrawData(drawData); // backend sam zapisuje i odtwarza resztę stanu
        if (srgb) glEnable(GL_FRAMEBUFFER_SRGB);
    }
};

} // namespace

std::unique_ptr<IImGuiRenderer> CreateGLImGuiRenderer(Device&) {
    return std::make_unique<GLImGuiRenderer>();
}

} // namespace wz::rhi::gl
