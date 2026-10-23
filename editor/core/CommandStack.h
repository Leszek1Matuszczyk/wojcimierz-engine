// ============================================================
//  Wojcimierz Engine: editor/core/CommandStack.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wz::editor {

class EditorContext;

// Jedna odwracalna zmiana w edytorze. Komendy wskazują obiekty przez UUID, nigdy przez
// wskaźniki, bo usunięcie i cofnięcie usunięcia tworzy obiekt od nowa pod innym adresem.
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Do(EditorContext& ctx) = 0;
    virtual void Undo(EditorContext& ctx) = 0;
    virtual std::string Name() const = 0;
    // Scalanie ciągłych zmian (np. przeciąganie suwaka w inspektorze) w jedną pozycję historii.
    // `next` zostało już wykonane; zwróć true, jeśli ta komenda przejęła jego efekt.
    virtual bool MergeWith(const ICommand& /*next*/) { return false; }
};

// Historia cofania i ponawiania.
class CommandStack {
public:
    explicit CommandStack(std::size_t limit = 512) : limit_(limit) {}

    // Wykonuje komendę i zapisuje ją w historii (czyści gałąź "redo").
    void Execute(EditorContext& ctx, std::unique_ptr<ICommand> command);
    bool Undo(EditorContext& ctx);
    bool Redo(EditorContext& ctx);

    bool CanUndo() const { return cursor_ > 0; }
    bool CanRedo() const { return cursor_ < entries_.size(); }
    std::string UndoName() const;
    std::string RedoName() const;

    // Kończy bieżącą serię scalania: następna komenda nie połączy się z poprzednią.
    // Inspektor woła to, gdy użytkownik puszcza suwak.
    void BreakMerge() { mergeOpen_ = false; }

    void Clear();
    // Stan "zapisany": IsDirty() zwraca false, dopóki historia nie odejdzie od tego miejsca.
    void MarkSaved() { savedRevision_ = CurrentRevision(); }
    bool IsDirty() const { return savedRevision_ != CurrentRevision(); }
    std::size_t Size() const { return entries_.size(); }

private:
    struct Entry {
        std::unique_ptr<ICommand> command;
        std::uint64_t revision;
    };
    std::uint64_t CurrentRevision() const { return cursor_ == 0 ? 0 : entries_[cursor_ - 1].revision; }

    std::vector<Entry> entries_;
    std::size_t cursor_ = 0; // liczba wykonanych (niecofniętych) komend
    std::size_t limit_;
    std::uint64_t nextRevision_ = 1;
    std::uint64_t savedRevision_ = 0;
    bool mergeOpen_ = false;
};

} // namespace wz::editor
