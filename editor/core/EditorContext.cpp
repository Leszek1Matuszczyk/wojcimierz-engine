// ============================================================
//  Wojcimierz Engine: editor/core/EditorContext.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "editor/core/EditorContext.h"

#include "scene/Scene.h"
#include "serialization/SceneSerializer.h"

#include <exception>

namespace wz::editor {

EditorContext::EditorContext() : scene_(std::make_unique<Scene>("Untitled")) {}
EditorContext::~EditorContext() = default;

GameObject* EditorContext::GetSelected() const { return scene_->FindById(selection_); }

void EditorContext::NewScene() {
    scene_ = std::make_unique<Scene>("Untitled");
    scenePath_.clear();
    selection_ = UUID();
    commands_.Clear();
}

bool EditorContext::OpenScene(const std::filesystem::path& path) {
    // Najpierw wczytujemy do nowej sceny, a podmieniamy dopiero po sukcesie.
    auto fresh = std::make_unique<Scene>(path.stem().string());
    try {
        auto result = SceneSerializer::LoadFromFile(*fresh, path);
        for (auto& w : result.warnings) Log(LogLevel::Warning, path.filename().string() + ": " + w);
    } catch (const std::exception& e) {
        Log(LogLevel::Error, std::string("Nie udalo sie otworzyc sceny: ") + e.what());
        return false;
    }
    scene_ = std::move(fresh);
    scenePath_ = path;
    selection_ = UUID();
    commands_.Clear();
    Log(LogLevel::Info, "Otwarto " + path.string());
    return true;
}

bool EditorContext::SaveScene(const std::filesystem::path& path) {
    try {
        SceneSerializer::SaveToFile(*scene_, path);
    } catch (const std::exception& e) {
        Log(LogLevel::Error, std::string("Nie udalo sie zapisac sceny: ") + e.what());
        return false;
    }
    scenePath_ = path;
    commands_.MarkSaved();
    Log(LogLevel::Info, "Zapisano " + path.string());
    return true;
}

} // namespace wz::editor
