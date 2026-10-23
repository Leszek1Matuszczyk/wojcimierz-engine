// ============================================================
//  Wojcimierz Engine: editor/core/Commands.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once

#include "core/UUID.h"
#include "editor/core/CommandStack.h"
#include "scene/ComponentRegistry.h"

#include <cstddef>
#include <nlohmann/json.hpp>
#include <string>

namespace wz {
class Component;
}

namespace wz::editor {

// Odnajduje komponent po UUID obiektu i indeksie na liście komponentów (0 = Transform).
Component* FindComponent(EditorContext& ctx, const UUID& objectId, std::size_t componentIndex);

// Zmiana jednego pola komponentu z refleksji. Kolejne zmiany tego samego pola
// (przeciąganie suwaka) scalają się w jedną pozycję historii.
class SetFieldCommand final : public ICommand {
public:
    SetFieldCommand(UUID objectId, std::size_t componentIndex, std::string fieldName, FieldValue oldValue,
                    FieldValue newValue);
    void Do(EditorContext& ctx) override { Apply(ctx, newValue_); }
    void Undo(EditorContext& ctx) override { Apply(ctx, oldValue_); }
    std::string Name() const override { return "Zmiana " + fieldName_; }
    bool MergeWith(const ICommand& next) override;

private:
    void Apply(EditorContext& ctx, const FieldValue& value) const;

    UUID objectId_;
    std::size_t componentIndex_;
    std::string fieldName_;
    FieldValue oldValue_;
    FieldValue newValue_;
};

// Nowy pusty GameObject (opcjonalnie jako dziecko). Redo odtwarza go z tym samym UUID.
class CreateObjectCommand final : public ICommand {
public:
    explicit CreateObjectCommand(std::string name = "GameObject", UUID parentId = UUID());
    void Do(EditorContext& ctx) override;
    void Undo(EditorContext& ctx) override;
    std::string Name() const override { return "Utworzenie " + name_; }
    const UUID& GetId() const { return id_; }

private:
    std::string name_;
    UUID parentId_;
    UUID id_ = UUID::Generate();
};

// Usunięcie obiektu z całym poddrzewem. Undo odtwarza je z migawki JSON z tymi samymi UUID,
// więc komendy wcześniej w historii nadal trafiają we właściwe obiekty.
class DeleteObjectCommand final : public ICommand {
public:
    explicit DeleteObjectCommand(UUID id) : id_(id) {}
    void Do(EditorContext& ctx) override;
    void Undo(EditorContext& ctx) override;
    std::string Name() const override { return "Usuniecie"; }

private:
    UUID id_;
    UUID parentId_;
    nlohmann::json snapshot_;
};

// Zmiana rodzica (pusty newParentId = korzeń). Pozycja w świecie zostaje zachowana, jak w Unity.
class ReparentCommand final : public ICommand {
public:
    ReparentCommand(UUID id, UUID newParentId) : id_(id), newParentId_(newParentId) {}
    void Do(EditorContext& ctx) override;
    void Undo(EditorContext& ctx) override;
    std::string Name() const override { return "Zmiana rodzica"; }

private:
    UUID id_;
    UUID newParentId_;
    UUID oldParentId_;
};

class RenameCommand final : public ICommand {
public:
    RenameCommand(UUID id, std::string oldName, std::string newName)
        : id_(id), oldName_(std::move(oldName)), newName_(std::move(newName)) {}
    void Do(EditorContext& ctx) override { Apply(ctx, newName_); }
    void Undo(EditorContext& ctx) override { Apply(ctx, oldName_); }
    std::string Name() const override { return "Zmiana nazwy"; }

private:
    void Apply(EditorContext& ctx, const std::string& name) const;
    UUID id_;
    std::string oldName_, newName_;
};

class SetActiveCommand final : public ICommand {
public:
    SetActiveCommand(UUID id, bool active) : id_(id), active_(active) {}
    void Do(EditorContext& ctx) override { Apply(ctx, active_); }
    void Undo(EditorContext& ctx) override { Apply(ctx, !active_); }
    std::string Name() const override { return active_ ? "Wlaczenie obiektu" : "Wylaczenie obiektu"; }

private:
    void Apply(EditorContext& ctx, bool active) const;
    UUID id_;
    bool active_;
};

// Dodanie komponentu po nazwie z rejestru. Undo czeka na GameObject::RemoveComponent w rdzeniu,
// więc na razie ta komenda nie trafia do historii (CommandStack jej nie cofnie).
bool AddComponentNow(EditorContext& ctx, const UUID& objectId, const std::string& typeName);

} // namespace wz::editor
