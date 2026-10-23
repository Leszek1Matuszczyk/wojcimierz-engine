#include "platform/Input.h"

#include <array>

namespace wz {

namespace {
bool ValidKey(KeyCode k) { return static_cast<int>(k) >= 0 && static_cast<int>(k) < kMaxKeyCode; }
bool ValidButton(int b) { return b >= 0 && b < kMaxMouseButtons; }

struct InputState {
    IInputSource* source = nullptr;
    std::array<bool, kMaxKeyCode> keys{}, prevKeys{};
    std::array<bool, kMaxMouseButtons> buttons{}, prevButtons{};
    glm::vec2 mouse{0.0f};
};
InputState& S() {
    static InputState s;
    return s;
}
} // namespace

// --- FakeInput ---
void FakeInput::SetKey(KeyCode k, bool pressed) { if (ValidKey(k)) keys_[static_cast<int>(k)] = pressed; }
void FakeInput::SetMouseButton(int b, bool pressed) { if (ValidButton(b)) buttons_[b] = pressed; }
void FakeInput::Clear() { *this = FakeInput{}; }
bool FakeInput::IsKeyPressed(KeyCode k) const { return ValidKey(k) && keys_[static_cast<int>(k)]; }
bool FakeInput::IsMouseButtonPressed(int b) const { return ValidButton(b) && buttons_[b]; }

// --- Input ---
bool Input::GetKey(KeyCode k) { return ValidKey(k) && S().keys[static_cast<int>(k)]; }
bool Input::GetKeyDown(KeyCode k) { return ValidKey(k) && S().keys[static_cast<int>(k)] && !S().prevKeys[static_cast<int>(k)]; }
bool Input::GetKeyUp(KeyCode k) { return ValidKey(k) && !S().keys[static_cast<int>(k)] && S().prevKeys[static_cast<int>(k)]; }
bool Input::GetMouseButton(int b) { return ValidButton(b) && S().buttons[b]; }
bool Input::GetMouseButtonDown(int b) { return ValidButton(b) && S().buttons[b] && !S().prevButtons[b]; }
bool Input::GetMouseButtonUp(int b) { return ValidButton(b) && !S().buttons[b] && S().prevButtons[b]; }
glm::vec2 Input::MousePosition() { return S().mouse; }

void Input::SetSource(IInputSource* source) { S().source = source; }
IInputSource* Input::GetSource() { return S().source; }

void Input::Update() {
    InputState& s = S();
    s.prevKeys = s.keys;
    s.prevButtons = s.buttons;
    if (!s.source) {
        s.keys.fill(false);
        s.buttons.fill(false);
        return;
    }
    for (int k = 0; k < kMaxKeyCode; ++k) s.keys[k] = s.source->IsKeyPressed(static_cast<KeyCode>(k));
    for (int b = 0; b < kMaxMouseButtons; ++b) s.buttons[b] = s.source->IsMouseButtonPressed(b);
    s.mouse = s.source->GetMousePosition();
}

void Input::Reset() {
    InputState& s = S();
    s.keys.fill(false); s.prevKeys.fill(false);
    s.buttons.fill(false); s.prevButtons.fill(false);
    s.mouse = {0.0f, 0.0f};
}

} // namespace wz
