#pragma once
#include <glm/vec2.hpp>

namespace wz {

// Kody klawiszy - wartosci zgodne z GLFW (podzbior).
enum class KeyCode : int {
    Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    Alpha0 = 48, Alpha1, Alpha2, Alpha3, Alpha4, Alpha5, Alpha6, Alpha7, Alpha8, Alpha9,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261,
    RightArrow = 262, LeftArrow = 263, DownArrow = 264, UpArrow = 265,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift = 340, LeftControl = 341, LeftAlt = 342, RightShift = 344, RightControl = 345, RightAlt = 346,
};

constexpr int kMaxKeyCode = 512;
constexpr int kMaxMouseButtons = 8;

// Zrodlo surowego stanu wejscia (aktualnie wcisniete klawisze/przyciski).
// Implementacje: GlfwInputSource (prawdziwe okno) i FakeInput (testy).
class IInputSource {
public:
    virtual ~IInputSource() = default;
    virtual bool IsKeyPressed(KeyCode key) const = 0;
    virtual bool IsMouseButtonPressed(int button) const = 0;
    virtual glm::vec2 GetMousePosition() const = 0;
};

// Wejscie sterowane programowo - gameplay testuje sie bez okna.
class FakeInput final : public IInputSource {
public:
    void SetKey(KeyCode key, bool pressed);
    void SetMouseButton(int button, bool pressed);
    void SetMousePosition(glm::vec2 p) { mouse_ = p; }
    void Clear();

    bool IsKeyPressed(KeyCode key) const override;
    bool IsMouseButtonPressed(int button) const override;
    glm::vec2 GetMousePosition() const override { return mouse_; }

private:
    bool keys_[kMaxKeyCode] = {};
    bool buttons_[kMaxMouseButtons] = {};
    glm::vec2 mouse_{0.0f};
};

// Odpowiednik UnityEngine.Input (stary system wejscia), czytany przez gameplay.
// Input::Update() (wolane raz na klatke, po zdarzeniach okna) robi migawke stanu
// z aktywnego zrodla; GetKeyDown/GetKeyUp sa prawdziwe tylko w klatce zmiany.
class Input {
public:
    static bool GetKey(KeyCode k);
    static bool GetKeyDown(KeyCode k);
    static bool GetKeyUp(KeyCode k);
    static bool GetMouseButton(int button);
    static bool GetMouseButtonDown(int button);
    static bool GetMouseButtonUp(int button);
    static glm::vec2 MousePosition();

    // Ustawia aktywne zrodlo (nie przejmuje wlasnosci). nullptr = brak wejscia.
    static void SetSource(IInputSource* source);
    static IInputSource* GetSource();
    // Migawka stanu - raz na klatke.
    static void Update();
    // Zeruje migawki (np. miedzy testami).
    static void Reset();
};

} // namespace wz
