// ============================================================
//  Wojcimierz Engine: editor/core/EditorContext.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include "core/UUID.h"
#include "editor/core/CommandStack.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace wz {
class Scene;
class GameObject;
} // namespace wz

namespace wz::editor {

// Stan edytora niezależny od UI: otwarta scena, zaznaczenie, historia zmian i komunikaty.
// Panele ImGui tylko czytają ten stan i zgłaszają komendy, dzięki czemu całość da się
// testować bez okna.
class EditorContext {
public:
    EditorContext();
    ~EditorContext();

    Scene& GetScene() { return *scene_; }
    const Scene& GetScene() const { return *scene_; }
    CommandStack& Commands() { return commands_; }

    // Wykonuje komendę przez historię (skrót dla Commands().Execute(*this, ...)).
    void Execute(std::unique_ptr<ICommand> command) { commands_.Execute(*this, std::move(command)); }

    // --- zaznaczenie (pojedyncze) ---
    void Select(const UUID& id) { selection_ = id; }
    void ClearSelection() { selection_ = UUID(); }
    const UUID& GetSelectionId() const { return selection_; }
    // Zaznaczony obiekt albo nullptr (np. gdy został usunięty).
    GameObject* GetSelected() const;

    // --- plik sceny ---
    void NewScene();
    // Zwracają false i dopisują komunikat do Log() przy błędzie; scena zostaje wtedy bez zmian.
    bool OpenScene(const std::filesystem::path& path);
    bool SaveScene(const std::filesystem::path& path);
    bool SaveScene() { return !scenePath_.empty() && SaveScene(scenePath_); }
    const std::filesystem::path& GetScenePath() const { return scenePath_; }
    bool IsDirty() const { return commands_.IsDirty(); }

    // --- konsola ---
    enum class LogLevel { Info, Warning, Error };
    struct LogEntry {
        LogLevel level;
        std::string text;
    };
    void Log(LogLevel level, std::string text) { log_.push_back({level, std::move(text)}); }
    const std::vector<LogEntry>& GetLog() const { return log_; }
    void ClearLog() { log_.clear(); }

private:
    std::unique_ptr<Scene> scene_;
    CommandStack commands_;
    UUID selection_;
    std::filesystem::path scenePath_;
    std::vector<LogEntry> log_;
};

} // namespace wz::editor
