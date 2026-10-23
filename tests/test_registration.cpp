// ============================================================
//  Wojcimierz Engine: tests/test_registration.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "reglib/RegLib.h"
#include "scene/ComponentRegistry.h"
#include "scene/Component.h"
#include "scene/SceneComponents.h"

#include <gtest/gtest.h>

using wz::ComponentRegistry;
using wz::TypeInfo;

namespace {
struct TypeA {};
struct TypeB {};
} // namespace

// Komponent z biblioteki statycznej, do którego nikt nie odwołuje się po symbolu,
// jest dostępny po jawnej rejestracji modułu.
TEST(Registration, ExplicitRegistrationPullsComponentFromStaticLibrary) {
    reglib::RegisterRegLibComponents();
    reglib::RegisterRegLibComponents(); // idempotentne
    const TypeInfo* info = ComponentRegistry::Get().Find("Probe");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->cppName, "reglib::Probe");
    ASSERT_NE(info->FindField("power"), nullptr);
    auto created = ComponentRegistry::Get().Create("Probe");
    ASSERT_NE(created, nullptr);
    EXPECT_FLOAT_EQ(std::get<float>(info->FindField("power")->get(*created)), 7.0f);
}

// Kontrprzykład z tej samej biblioteki: bez jawnej rejestracji linker pomija plik,
// więc sama autorejestracja z makra nie wystarcza.
TEST(Registration, ComponentWithoutExplicitRegistrationIsDroppedByLinker) {
    EXPECT_EQ(ComponentRegistry::Get().Find("Orphan"), nullptr);
}

TEST(Registration, SceneComponentsAreIdempotent) {
    const std::size_t before = ComponentRegistry::Get().Size();
    wz::RegisterSceneComponents();
    wz::RegisterSceneComponents();
    EXPECT_EQ(ComponentRegistry::Get().Size(), before);
    EXPECT_NE(ComponentRegistry::Get().Find("Transform"), nullptr);
    EXPECT_EQ(ComponentRegistry::Get().Create("Transform"), nullptr) << "Transform nie powstaje osobno";
    EXPECT_EQ(ComponentRegistry::Get().Create("NieMa"), nullptr);
}

TEST(Registration, DuplicateNameFirstWinsWithStderrMessage) {
    ComponentRegistry reg;
    TypeInfo first("X", std::type_index(typeid(TypeA)));
    first.cppName = "gra::A";
    TypeInfo second("X", std::type_index(typeid(TypeB)));
    second.cppName = "gra::B";

    ASSERT_NE(reg.Register(std::move(first)), nullptr);
    testing::internal::CaptureStderr();
    EXPECT_EQ(reg.Register(std::move(second)), nullptr);
    const std::string err = testing::internal::GetCapturedStderr();

    EXPECT_NE(err.find("komponent 'X' zarejestrowany dwa razy (typy gra::A i gra::B)"), std::string::npos) << err;
    EXPECT_EQ(reg.Size(), 1u);
    EXPECT_EQ(reg.Find("X")->type, std::type_index(typeid(TypeA)));
    EXPECT_EQ(reg.Find<TypeB>(), nullptr);
}

TEST(Registration, DuplicateNameWithoutCppNameUsesDemangledType) {
    ComponentRegistry reg;
    reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
    testing::internal::CaptureStderr();
    reg.Register(TypeInfo("X", std::type_index(typeid(TypeB))));
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("TypeB"), std::string::npos) << err;
}

TEST(Registration, SameTypeAgainIsIdempotentAndSilent) {
    ComponentRegistry reg;
    const TypeInfo* a = reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
    testing::internal::CaptureStderr();
    const TypeInfo* again = reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
    EXPECT_EQ(a, again);
    EXPECT_EQ(reg.Size(), 1u);
}

TEST(Registration, SameTypeUnderOtherNameIsRejected) {
    ComponentRegistry reg;
    reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
    testing::internal::CaptureStderr();
    EXPECT_EQ(reg.Register(TypeInfo("Y", std::type_index(typeid(TypeA)))), nullptr);
    EXPECT_NE(testing::internal::GetCapturedStderr().find("odrzucam nazwe 'Y'"), std::string::npos);
    EXPECT_EQ(reg.Find("Y"), nullptr);
}

TEST(Registration, DuplicateFieldRejectsType) {
    ComponentRegistry reg;
    TypeInfo t("X", std::type_index(typeid(TypeA)));
    t.fields.push_back({"hp", wz::FieldType::Float, {}, {}});
    t.fields.push_back({"hp", wz::FieldType::Float, {}, {}});
    testing::internal::CaptureStderr();
    EXPECT_EQ(reg.Register(std::move(t)), nullptr);
    EXPECT_NE(testing::internal::GetCapturedStderr().find("pole 'hp'"), std::string::npos);
    EXPECT_EQ(reg.Size(), 0u);
}
