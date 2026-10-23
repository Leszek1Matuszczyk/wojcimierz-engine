// ============================================================
//  Wojcimierz Engine: tests/test_editor_commands.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "TestUtils.h"
#include "editor/core/Commands.h"
#include "editor/core/EditorContext.h"
#include "scene/Reflection.h"
#include "scene/Scene.h"

#include <filesystem>

namespace test_editor {
class Health : public wz::Component {
public:
    float hp = 100.0f;
};
} // namespace test_editor
REFLECT(test_editor::Health, FIELD(hp))

using namespace wz;
using namespace wz::editor;
using test_editor::Health;

namespace {

UUID Create(EditorContext& ctx, const std::string& name, UUID parent = UUID()) {
    auto cmd = std::make_unique<CreateObjectCommand>(name, parent);
    const UUID id = cmd->GetId();
    ctx.Execute(std::move(cmd));
    return id;
}

std::unique_ptr<SetFieldCommand> SetPos(EditorContext& ctx, const UUID& id, glm::vec3 to) {
    auto& t = ctx.GetScene().FindById(id)->transform();
    return std::make_unique<SetFieldCommand>(id, 0, "position", FieldValue(t.GetLocalPosition()), FieldValue(to));
}

} // namespace

TEST(EditorCommands, CreateUndoRedoKeepsId) {
    EditorContext ctx;
    const UUID id = Create(ctx, "Kostka");
    ASSERT_NE(ctx.GetScene().FindById(id), nullptr);
    EXPECT_EQ(ctx.GetSelectionId(), id);
    EXPECT_TRUE(ctx.IsDirty());

    ASSERT_TRUE(ctx.Commands().Undo(ctx));
    EXPECT_EQ(ctx.GetScene().FindById(id), nullptr);
    EXPECT_EQ(ctx.GetSelected(), nullptr);
    EXPECT_FALSE(ctx.IsDirty());

    ASSERT_TRUE(ctx.Commands().Redo(ctx));
    ASSERT_NE(ctx.GetScene().FindById(id), nullptr);
    EXPECT_EQ(ctx.GetScene().FindById(id)->GetName(), "Kostka");
}

TEST(EditorCommands, SetFieldMergesWhileDraggingAndBreaksOnRelease) {
    EditorContext ctx;
    const UUID id = Create(ctx, "A");
    ctx.Commands().BreakMerge();

    // Przeciąganie: trzy klatki, jedna pozycja w historii.
    ctx.Execute(SetPos(ctx, id, {1, 0, 0}));
    ctx.Execute(SetPos(ctx, id, {2, 0, 0}));
    ctx.Execute(SetPos(ctx, id, {3, 0, 0}));
    ctx.Commands().BreakMerge(); // puszczenie suwaka
    EXPECT_EQ(ctx.Commands().Size(), 2u);

    ctx.Execute(SetPos(ctx, id, {9, 0, 0}));
    EXPECT_EQ(ctx.Commands().Size(), 3u);

    auto& t = ctx.GetScene().FindById(id)->transform();
    ctx.Commands().Undo(ctx);
    ExpectVecNear(t.GetLocalPosition(), {3, 0, 0});
    ctx.Commands().Undo(ctx);
    ExpectVecNear(t.GetLocalPosition(), {0, 0, 0});
    ctx.Commands().Redo(ctx);
    ExpectVecNear(t.GetLocalPosition(), {3, 0, 0});
}

TEST(EditorCommands, DifferentFieldsDoNotMerge) {
    EditorContext ctx;
    const UUID id = Create(ctx, "A");
    ctx.Execute(SetPos(ctx, id, {1, 0, 0}));
    ctx.Execute(std::make_unique<SetFieldCommand>(id, 0, "scale", FieldValue(glm::vec3(1)), FieldValue(glm::vec3(2))));
    EXPECT_EQ(ctx.Commands().Size(), 3u);
}

TEST(EditorCommands, DeleteUndoRestoresSubtreeIdsParentAndFields) {
    EditorContext ctx;
    const UUID root = Create(ctx, "Root");
    const UUID mid = Create(ctx, "Mid", root);
    const UUID leaf = Create(ctx, "Leaf", mid);
    auto& leafGo = *ctx.GetScene().FindById(leaf);
    leafGo.AddComponent<Health>().hp = 42.0f;
    ctx.Execute(SetPos(ctx, leaf, {5, 6, 7}));

    ctx.Execute(std::make_unique<DeleteObjectCommand>(mid));
    EXPECT_EQ(ctx.GetScene().FindById(mid), nullptr);
    EXPECT_EQ(ctx.GetScene().FindById(leaf), nullptr);
    EXPECT_EQ(ctx.GetScene().GetGameObjectCount(), 1u);

    ctx.Commands().Undo(ctx);
    GameObject* m = ctx.GetScene().FindById(mid);
    GameObject* l = ctx.GetScene().FindById(leaf);
    ASSERT_TRUE(m && l);
    EXPECT_EQ(m->transform().GetParent(), &ctx.GetScene().FindById(root)->transform());
    EXPECT_EQ(l->transform().GetParent(), &m->transform());
    ExpectVecNear(l->transform().GetLocalPosition(), {5, 6, 7});
    ASSERT_NE(l->GetComponent<Health>(), nullptr);
    EXPECT_FLOAT_EQ(l->GetComponent<Health>()->hp, 42.0f);

    // Starsze komendy w historii nadal trafiają w odtworzone obiekty (te same UUID).
    ctx.Commands().Undo(ctx); // cofnięcie SetPos na liściu
    ExpectVecNear(ctx.GetScene().FindById(leaf)->transform().GetLocalPosition(), {0, 0, 0});
}

TEST(EditorCommands, ReparentKeepsWorldPositionAndUndoes) {
    EditorContext ctx;
    const UUID a = Create(ctx, "A");
    const UUID b = Create(ctx, "B");
    ctx.Execute(SetPos(ctx, a, {10, 0, 0}));
    ctx.Commands().BreakMerge();
    ctx.Execute(SetPos(ctx, b, {1, 2, 3}));

    ctx.Execute(std::make_unique<ReparentCommand>(b, a));
    auto& tb = ctx.GetScene().FindById(b)->transform();
    EXPECT_EQ(tb.GetParent(), &ctx.GetScene().FindById(a)->transform());
    ExpectVecNear(tb.GetPosition(), {1, 2, 3});
    ExpectVecNear(tb.GetLocalPosition(), {-9, 2, 3});

    ctx.Commands().Undo(ctx);
    EXPECT_EQ(tb.GetParent(), nullptr);
    ExpectVecNear(tb.GetLocalPosition(), {1, 2, 3});
}

TEST(EditorCommands, ReparentOntoDescendantIsRejected) {
    EditorContext ctx;
    const UUID a = Create(ctx, "A");
    const UUID b = Create(ctx, "B", a);
    ctx.Execute(std::make_unique<ReparentCommand>(a, b));
    EXPECT_EQ(ctx.GetScene().FindById(a)->transform().GetParent(), nullptr);
    EXPECT_FALSE(ctx.GetLog().empty());
}

TEST(EditorCommands, RenameAndSetActive) {
    EditorContext ctx;
    const UUID a = Create(ctx, "A");
    ctx.Execute(std::make_unique<RenameCommand>(a, "A", "Gracz"));
    ctx.Execute(std::make_unique<SetActiveCommand>(a, false));
    EXPECT_EQ(ctx.GetScene().FindById(a)->GetName(), "Gracz");
    EXPECT_FALSE(ctx.GetScene().FindById(a)->IsActiveSelf());
    ctx.Commands().Undo(ctx);
    ctx.Commands().Undo(ctx);
    EXPECT_EQ(ctx.GetScene().FindById(a)->GetName(), "A");
    EXPECT_TRUE(ctx.GetScene().FindById(a)->IsActiveSelf());
}

TEST(EditorCommands, NewCommandDropsRedo) {
    EditorContext ctx;
    Create(ctx, "A");
    ctx.Commands().Undo(ctx);
    EXPECT_TRUE(ctx.Commands().CanRedo());
    Create(ctx, "B");
    EXPECT_FALSE(ctx.Commands().CanRedo());
    EXPECT_EQ(ctx.Commands().Size(), 1u);
}

TEST(EditorCommands, HistoryLimitDropsOldest) {
    EditorContext ctx;
    CommandStack small(3);
    for (int i = 0; i < 5; ++i) {
        small.BreakMerge();
        small.Execute(ctx, std::make_unique<CreateObjectCommand>("X" + std::to_string(i)));
    }
    EXPECT_EQ(small.Size(), 3u);
    int undone = 0;
    while (small.Undo(ctx)) ++undone;
    EXPECT_EQ(undone, 3);
    EXPECT_EQ(ctx.GetScene().GetGameObjectCount(), 2u);
}

TEST(EditorContext, SaveOpenAndDirtyFlag) {
    const auto dir = std::filesystem::temp_directory_path() / ("wz_editor_" + UUID::Generate().ToString());
    std::filesystem::create_directories(dir);
    const auto path = dir / "level.scene.json";

    EditorContext ctx;
    const UUID a = Create(ctx, "A");
    ctx.Execute(SetPos(ctx, a, {4, 5, 6}));
    EXPECT_TRUE(ctx.IsDirty());
    ASSERT_TRUE(ctx.SaveScene(path));
    EXPECT_FALSE(ctx.IsDirty());
    ctx.Execute(std::make_unique<RenameCommand>(a, "A", "B"));
    EXPECT_TRUE(ctx.IsDirty());
    ctx.Commands().Undo(ctx);
    EXPECT_FALSE(ctx.IsDirty()) << "cofniecie do zapisanego stanu = brak zmian";

    EditorContext other;
    ASSERT_TRUE(other.OpenScene(path));
    EXPECT_EQ(other.GetScenePath(), path);
    EXPECT_FALSE(other.Commands().CanUndo());
    ExpectVecNear(other.GetScene().FindById(a)->transform().GetLocalPosition(), {4, 5, 6});

    EXPECT_FALSE(other.OpenScene(dir / "brak.json"));
    EXPECT_NE(other.GetScene().FindById(a), nullptr) << "nieudane otwarcie nie podmienia sceny";
    EXPECT_EQ(other.GetLog().back().level, EditorContext::LogLevel::Error);
    std::filesystem::remove_all(dir);
}

TEST(EditorCommands, AddComponentByName) {
    EditorContext ctx;
    const UUID a = Create(ctx, "A");
    EXPECT_TRUE(AddComponentNow(ctx, a, "Health"));
    EXPECT_NE(ctx.GetScene().FindById(a)->GetComponent<Health>(), nullptr);
    EXPECT_FALSE(AddComponentNow(ctx, a, "Transform"));
    EXPECT_FALSE(AddComponentNow(ctx, a, "NieMa"));
}
