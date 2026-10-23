#include "platform/Window.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <stdexcept>

namespace wz {

namespace {
int g_windowCount = 0;

void ErrorCallback(int code, const char* msg) { std::fprintf(stderr, "[GLFW] blad %d: %s\n", code, msg); }
} // namespace

Window::Window(const WindowDesc& desc) {
    if (g_windowCount == 0) {
        glfwSetErrorCallback(ErrorCallback);
        if (!glfwInit()) throw std::runtime_error("glfwInit() nie powiodlo sie");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, desc.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    window_ = glfwCreateWindow(desc.width, desc.height, desc.title.c_str(), nullptr, nullptr);
    if (!window_) {
        if (g_windowCount == 0) glfwTerminate();
        throw std::runtime_error("Nie udalo sie utworzyc okna z kontekstem OpenGL 4.5 core");
    }
    ++g_windowCount;
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(desc.vsync ? 1 : 0);
}

Window::~Window() {
    if (window_) glfwDestroyWindow(window_);
    if (--g_windowCount == 0) glfwTerminate();
}

bool Window::ShouldClose() const { return glfwWindowShouldClose(window_) != 0; }
void Window::RequestClose() { glfwSetWindowShouldClose(window_, GLFW_TRUE); }

void Window::PollEvents() { glfwPollEvents(); }

void Window::SwapBuffers() { glfwSwapBuffers(window_); }
void Window::GetFramebufferSize(int& w, int& h) const { glfwGetFramebufferSize(window_, &w, &h); }
double Window::GetTimeSeconds() const { return glfwGetTime(); }
void* Window::GetProcAddress(const char* name) { return reinterpret_cast<void*>(glfwGetProcAddress(name)); }

} // namespace wz
