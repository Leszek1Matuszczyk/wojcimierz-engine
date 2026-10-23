// ============================================================
//  Wojcimierz Engine: editor/core/Commands.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "editor/core/Commands.h"

#include "editor/core/EditorContext.h"
#include "scene/Scene.h"
#include "serialization/SceneSerializer.h"

namespace wz::editor {

namespace {

Transform* ParentTransform(EditorContext& ctx, const UUID& parentId) {
    if (parentId.IsNil()) return nullptr;
    GameObject* p = ctx.GetScene().FindById(parentId);
    return p ? &p->transform() : nullptr;
}

UUID ParentIdOf(const GameObject& go) {
    const Transform* p = go.transform().GetParent();
    return p ? p->gameObject().GetId() : UUID();
}

} // namespace

Component* FindComponent(EditorContext& ctx, const UUID& objectId, std::size_t componentIndex) {
    GameObject* go = ctx.GetScene().FindById(objectId);
    if (!go || componentIndex >= go->GetComponentCount()) return nullptr;
    return &go->GetComponentAt(componentIndex);
}

// --- SetFieldCommand ---

SetFieldCommand::SetFieldCommand(UUID objectId, std::size_t componentIndex, std::string fieldName,
                                 FieldValue oldValue, FieldValue newValue)
    : objectId_(objectId), componentIndex_(componentIndex), fieldName_(std::move(fieldName)),
      oldValue_(std::move(oldValue)), newValue_(std::move(newValue)) {}

void SetFieldCommand::Apply(EditorContext& ctx, const FieldValue& value) const {
    Component* c = FindComponent(ctx, objectId_, componentIndex_);
    if (!c) return;
    const TypeInfo* info = ComponentRegistry::Get().FindFor(*c);
    if (!info) return;
    if (const FieldInfo* f = info->FindField(fieldName_)) f->set(*c, value);
}

bool SetFieldCommand::MergeWith(const ICommand& next) {
    const auto* other = dynamic_cast<const SetFieldCommand*>(&next);
    if (!other || other->objectId_ != objectId_ || other->componentIndex_ != componentIndex_ ||
        other->fieldName_ != fieldName_)
        return false;
    newValue_ = other->newValue_; // oldValue_ zostaje z początku przeciągania
    return true;
}

// --- CreateObjectCommand ---

CreateObjectCommand::CreateObjectCommand(std::string name, UUID parentId)
    : name_(std::move(name)), parentId_(parentId) {}

void CreateObjectCommand::Do(EditorContext& ctx) {
    GameObject& go = ctx.GetScene().CreateGameObject(name_);
    go.SetId(id_);
    if (Transform* parent = ParentTransform(ctx, parentId_)) go.transform().SetParent(parent, false);
    ctx.Select(id_);
}

void CreateObjectCommand::Undo(EditorContext& ctx) {
    if (GameObject* go = ctx.GetScene().FindById(id_)) {
        ctx.GetScene().Destroy(*go);
        ctx.GetScene().FlushDestroyed();
    }
    if (ctx.GetSelectionId() == id_) ctx.ClearSelection();
}

// --- DeleteObjectCommand ---

void DeleteObjectCommand::Do(EditorContext& ctx) {
    GameObject* go = ctx.GetScene().FindById(id_);
    if (!go) return;
    snapshot_ = SceneSerializer::SerializeSubtree(*go);
    parentId_ = ParentIdOf(*go);
    ctx.GetScene().Destroy(*go);
    ctx.GetScene().FlushDestroyed();
    if (!ctx.GetSelected()) ctx.ClearSelection();
}

void DeleteObjectCommand::Undo(EditorContext& ctx) {
    if (snapshot_.is_null()) return;
    // Bez IdRemap: obiekty wracają z tymi samymi UUID.
    auto result = SceneSerializer::Deserialize(ctx.GetScene(), snapshot_);
    for (auto& w : result.warnings) ctx.Log(EditorContext::LogLevel::Warning, "Cofniecie usuniecia: " + w);
    if (Transform* parent = ParentTransform(ctx, parentId_))
        for (GameObject* root : result.roots) root->transform().SetParent(parent, false);
    ctx.Select(id_);
}

// --- ReparentCommand ---

void ReparentCommand::Do(EditorContext& ctx) {
    GameObject* go = ctx.GetScene().FindById(id_);
    if (!go) return;
    oldParentId_ = ParentIdOf(*go);
    if (!go->transform().SetParent(ParentTransform(ctx, newParentId_), true))
        ctx.Log(EditorContext::LogLevel::Warning, "Nie mozna ustawic potomka jako rodzica");
}

void ReparentCommand::Undo(EditorContext& ctx) {
    if (GameObject* go = ctx.GetScene().FindById(id_)) go->transform().SetParent(ParentTransform(ctx, oldParentId_), true);
}

// --- pozostałe ---

void RenameCommand::Apply(EditorContext& ctx, const std::string& name) const {
    if (GameObject* go = ctx.GetScene().FindById(id_)) go->SetName(name);
}

void SetActiveCommand::Apply(EditorContext& ctx, bool active) const {
    if (GameObject* go = ctx.GetScene().FindById(id_)) go->SetActive(active);
}

bool AddComponentNow(EditorContext& ctx, const UUID& objectId, const std::string& typeName) {
    GameObject* go = ctx.GetScene().FindById(objectId);
    const TypeInfo* info = ComponentRegistry::Get().Find(typeName);
    if (!go || !info || !info->create) return false;
    go->AddComponent(info->create());
    return true;
}

} // namespace wz::editor
