#pragma once
#include <string>

struct GLFWwindow;

namespace wz {

struct WindowDesc {
    std::string title = "Wojcimierz Engine";
    int width = 1280;
    int height = 720;
    bool visible = true;  // false = okno niewidoczne (tryb headless / testy renderingu)
    bool vsync = true;
    bool resizable = true;
};

// Okno systemowe + kontekst OpenGL 4.5 core (GLFW).
class Window {
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const;
    void RequestClose();
    void PollEvents();
    void SwapBuffers();
    void GetFramebufferSize(int& w, int& h) const;
    double GetTimeSeconds() const;

    GLFWwindow* GetNativeHandle() const { return window_; }
    // Do inicjalizacji loadera OpenGL (glad).
    static void* GetProcAddress(const char* name);

private:
    GLFWwindow* window_ = nullptr;
};

} // namespace wz
