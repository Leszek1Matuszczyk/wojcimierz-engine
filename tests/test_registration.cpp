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

#include <cstdlib>

using wz::ComponentRegistry;
using wz::TypeInfo;

namespace {
struct TypeA {};
struct TypeB {};

// Mądrość Wojcimierza: test ma się kompilować także na MSVC, który nie zna
// setenv/unsetenv z POSIX, więc tam ustawiamy zmienną przez _putenv_s.
// Uwaga: na Windows pusta wartość usuwa zmienną, co dla DefaultStrict() daje to samo.
void SetEnvVar(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void UnsetEnvVar(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}
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
// więc sama autorejestracja z makra nie wystarcza. To test zachowania LINKERA, a nie gwarancja
// silnika: przy --whole-archive, /WHOLEARCHIVE, -all_load albo bibliotece OBJECT plik trafia do
// binarki i Orphan się rejestruje. Takie konfiguracje ustawiają WZ_TEST_WHOLE_ARCHIVE i test jest pomijany.
TEST(Registration, ComponentWithoutExplicitRegistrationIsDroppedByLinker) {
#ifdef WZ_TEST_WHOLE_ARCHIVE
    GTEST_SKIP() << "linkowanie whole-archive: plik Orphan jest dolaczony celowo";
#endif
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
    reg.SetStrict(false); // zachowanie domyslne, niezaleznie od WZ_REGISTRY_STRICT
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
    reg.SetStrict(false); // zachowanie domyslne, niezaleznie od WZ_REGISTRY_STRICT
    reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
    testing::internal::CaptureStderr();
    reg.Register(TypeInfo("X", std::type_index(typeid(TypeB))));
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_NE(err.find("TypeB"), std::string::npos) << err;
}

TEST(Registration, SameTypeAgainIsIdempotentAndSilent) {
    ComponentRegistry reg;
    reg.SetStrict(false); // zachowanie domyslne, niezaleznie od WZ_REGISTRY_STRICT
    const TypeInfo* a = reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
    testing::internal::CaptureStderr();
    const TypeInfo* again = reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());
    EXPECT_EQ(a, again);
    EXPECT_EQ(reg.Size(), 1u);
}

TEST(Registration, SameTypeUnderOtherNameIsRejected) {
    ComponentRegistry reg;
    reg.SetStrict(false); // zachowanie domyslne, niezaleznie od WZ_REGISTRY_STRICT
    reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
    testing::internal::CaptureStderr();
    EXPECT_EQ(reg.Register(TypeInfo("Y", std::type_index(typeid(TypeA)))), nullptr);
    EXPECT_NE(testing::internal::GetCapturedStderr().find("odrzucam nazwe 'Y'"), std::string::npos);
    EXPECT_EQ(reg.Find("Y"), nullptr);
}

TEST(Registration, DuplicateFieldRejectsType) {
    ComponentRegistry reg;
    reg.SetStrict(false); // zachowanie domyslne, niezaleznie od WZ_REGISTRY_STRICT
    TypeInfo t("X", std::type_index(typeid(TypeA)));
    t.fields.push_back({"hp", wz::FieldType::Float, {}, {}});
    t.fields.push_back({"hp", wz::FieldType::Float, {}, {}});
    testing::internal::CaptureStderr();
    EXPECT_EQ(reg.Register(std::move(t)), nullptr);
    EXPECT_NE(testing::internal::GetCapturedStderr().find("pole 'hp'"), std::string::npos);
    EXPECT_EQ(reg.Size(), 0u);
}

// Tryb strict (CI): odrzucona rejestracja po komunikacie kończy program.
TEST(RegistrationDeathTest, StrictModeAbortsOnDuplicateName) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH(
        {
            ComponentRegistry reg;
            reg.SetStrict(true);
            reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
            reg.Register(TypeInfo("X", std::type_index(typeid(TypeB))));
        },
        "komponent 'X' zarejestrowany dwa razy[^\n]*\n.*tryb strict");
}

TEST(RegistrationDeathTest, StrictModeFromEnvironmentVariable) {
#ifdef WZ_REGISTRY_STRICT
    GTEST_SKIP() << "strict wkompilowany; zmienna srodowiskowa nie ma znaczenia";
#endif
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH(
        {
            SetEnvVar("WZ_REGISTRY_STRICT", "1");
            ComponentRegistry reg;
            reg.Register(TypeInfo("X", std::type_index(typeid(TypeA))));
            reg.Register(TypeInfo("X", std::type_index(typeid(TypeB))));
        },
        "tryb strict");
}

TEST(Registration, StrictEnvValuesAndDefault) {
#ifdef WZ_REGISTRY_STRICT
    EXPECT_TRUE(ComponentRegistry::DefaultStrict());
    GTEST_SKIP() << "strict wkompilowany";
#endif
    const char* old = std::getenv("WZ_REGISTRY_STRICT");
    const std::string saved = old ? old : "";
    for (const char* off : {"0", "", " ", " 0", "0 ", "\t0\n", "false", "FALSE", "False", " off ", "OFF", "no", "No"}) {
        SetEnvVar("WZ_REGISTRY_STRICT", off);
        EXPECT_FALSE(ComponentRegistry::DefaultStrict()) << "'" << off << "'";
    }
    for (const char* on : {"1", "true", "ON", "yes", "00", " 1 "}) {
        SetEnvVar("WZ_REGISTRY_STRICT", on);
        EXPECT_TRUE(ComponentRegistry::DefaultStrict()) << "'" << on << "'";
    }
    UnsetEnvVar("WZ_REGISTRY_STRICT");
    EXPECT_FALSE(ComponentRegistry::DefaultStrict());
    if (old) SetEnvVar("WZ_REGISTRY_STRICT", saved.c_str());
    // Idempotentna rejestracja tego samego typu nie jest odrzuceniem, więc strict jej nie dotyczy.
    ComponentRegistry reg;
    reg.SetStrict(true);
    ASSERT_NE(reg.Register(TypeInfo("Y", std::type_index(typeid(TypeA)))), nullptr);
    EXPECT_NE(reg.Register(TypeInfo("Y", std::type_index(typeid(TypeA)))), nullptr);
}
