// ============================================================
//  Wojcimierz Engine: editor/core/CommandStack.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "editor/core/CommandStack.h"

namespace wz::editor {

void CommandStack::Execute(EditorContext& ctx, std::unique_ptr<ICommand> command) {
    if (!command) return;
    command->Do(ctx);

    const bool hadRedo = cursor_ < entries_.size();
    entries_.resize(cursor_); // nowa zmiana unieważnia "redo"
    if (mergeOpen_ && !hadRedo && cursor_ > 0 && entries_[cursor_ - 1].command->MergeWith(*command)) {
        // Scalona zmiana to nowy stan dokumentu, więc dostaje nową rewizję (ważne dla IsDirty).
        entries_[cursor_ - 1].revision = nextRevision_++;
        return;
    }
    entries_.push_back({std::move(command), nextRevision_++});
    ++cursor_;
    mergeOpen_ = true;

    if (entries_.size() > limit_) {
        // Najstarsza pozycja wypada z historii. Mądrość Wojcimierza podpowiada, że nikt nie
        // cofa pięciuset kroków, a pamięć jest dobrem wspólnym.
        entries_.erase(entries_.begin());
        --cursor_;
    }
}

bool CommandStack::Undo(EditorContext& ctx) {
    if (!CanUndo()) return false;
    mergeOpen_ = false;
    entries_[--cursor_].command->Undo(ctx);
    return true;
}

bool CommandStack::Redo(EditorContext& ctx) {
    if (!CanRedo()) return false;
    mergeOpen_ = false;
    entries_[cursor_++].command->Do(ctx);
    return true;
}

std::string CommandStack::UndoName() const { return CanUndo() ? entries_[cursor_ - 1].command->Name() : ""; }
std::string CommandStack::RedoName() const { return CanRedo() ? entries_[cursor_].command->Name() : ""; }

void CommandStack::Clear() {
    entries_.clear();
    cursor_ = 0;
    mergeOpen_ = false;
    savedRevision_ = 0;
}

} // namespace wz::editor
