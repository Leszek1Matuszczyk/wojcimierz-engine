// ============================================================
//  Wojcimierz Engine: tests/test_scene_serializer.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "TestUtils.h"
#include "scene/Reflection.h"
#include "scene/Scene.h"
#include "serialization/SceneSerializer.h"

#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <fstream>
#include <nlohmann/json.hpp>

namespace test_serializer {

enum class BodyType { Static = 0, Kinematic = 1, Dynamic = 2 };

class Body : public wz::Component {
public:
    float mass = 1.0f;
    BodyType bodyType = BodyType::Dynamic;
    glm::vec3 velocity{0.0f};
    std::string tag;
    wz::UUID follow;   // referencja do innego GameObjectu
    wz::UUID material; // GUID assetu (spoza sceny)

    float massSeenInAwake = -1.0f;
    bool parentSeenInAwake = false;
    void Awake() override {
        massSeenInAwake = mass;
        parentSeenInAwake = transform().GetParent() != nullptr;
    }
};

class Unreflected : public wz::Component {};

} // namespace test_serializer

REFLECT(test_serializer::Body, FIELD(mass), FIELD(bodyType), FIELD(velocity), FIELD(tag), FIELD(follow),
        FIELD(material))

using namespace test_serializer;
using nlohmann::json;
using wz::GameObject;
using wz::IdRemap;
using wz::Scene;
using wz::SceneSerializer;
using wz::SerializationError;
using wz::UUID;

namespace {

GameObject* FindByName(const std::vector<GameObject*>& objs, const std::string& name) {
    for (auto* go : objs)
        if (go->GetName() == name) return go;
    return nullptr;
}

// Scena: Root(pos 1,2,3; rot 90 st. wokol Y) -> {ChildA (Body), ChildB (wylaczony)} oraz Other.
struct Fixture {
    Scene scene{"Main"};
    GameObject* root;
    GameObject* childA;
    GameObject* childB;
    GameObject* other;

    Fixture() {
        root = &scene.CreateGameObject("Root");
        childA = &scene.CreateGameObject("ChildA");
        childB = &scene.CreateGameObject("ChildB");
        other = &scene.CreateGameObject("Other");
        root->transform().SetLocalPosition({1, 2, 3});
        root->transform().SetLocalRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)));
        root->transform().SetLocalScale({2, 2, 2});
        childA->transform().SetParent(&root->transform(), false);
        childB->transform().SetParent(&root->transform(), false);
        childA->transform().SetLocalPosition({0.1f, 0, 0});
        childB->SetActive(false);

        auto& body = childA->AddComponent<Body>();
        body.mass = 2.5f;
        body.bodyType = BodyType::Kinematic;
        body.velocity = {0, -9.81f, 0};
        body.tag = "gracz";
        body.follow = other->GetId();
        body.material = UUID(0xAAAA, 0xBBBB);
        body.enabled = false;
        childA->AddComponent<Unreflected>();
    }
};

} // namespace

TEST(SceneSerializer, DocumentShape) {
    Fixture f;
    const json doc = SceneSerializer::SerializeScene(f.scene);
    EXPECT_EQ(doc["version"], wz::kSceneFormatVersion);
    EXPECT_EQ(doc["name"], "Main");
    ASSERT_EQ(doc["objects"].size(), 4u);

    // Preorder: Root, ChildA, ChildB, Other.
    EXPECT_EQ(doc["objects"][0]["name"], "Root");
    EXPECT_EQ(doc["objects"][1]["name"], "ChildA");
    EXPECT_EQ(doc["objects"][2]["name"], "ChildB");
    EXPECT_EQ(doc["objects"][3]["name"], "Other");
    EXPECT_TRUE(doc["objects"][0]["parent"].is_null());
    EXPECT_EQ(doc["objects"][1]["parent"], f.root->GetId().ToString());
    EXPECT_EQ(doc["objects"][2]["active"], false);

    const json& rootT = doc["objects"][0]["components"][0];
    EXPECT_EQ(rootT["type"], "Transform");
    EXPECT_FALSE(rootT.contains("enabled"));
    EXPECT_EQ(rootT["position"], json::array({1.0, 2.0, 3.0}));
    ASSERT_EQ(rootT["rotation"].size(), 4u); // [x, y, z, w]
    EXPECT_NEAR(rootT["rotation"][1].get<double>(), std::sqrt(0.5), 1e-6);
    EXPECT_NEAR(rootT["rotation"][3].get<double>(), std::sqrt(0.5), 1e-6);

    const json& comps = doc["objects"][1]["components"];
    ASSERT_EQ(comps.size(), 2u) << "komponent bez REFLECT nie trafia do pliku";
    EXPECT_EQ(comps[0]["position"][0], 0.1) << "float zapisany najkrotsza postacia dziesietna";
    const json& body = comps[1];
    EXPECT_EQ(body["type"], "Body");
    EXPECT_EQ(body["enabled"], false);
    EXPECT_EQ(body["mass"], 2.5);
    EXPECT_EQ(body["bodyType"], 1);
    EXPECT_EQ(body["tag"], "gracz");
    EXPECT_EQ(body["follow"], f.other->GetId().ToString());
}

TEST(SceneSerializer, RoundTripPreservesEverything) {
    Fixture f;
    const json doc = SceneSerializer::SerializeScene(f.scene);

    Scene loaded;
    auto result = SceneSerializer::Deserialize(loaded, doc);
    EXPECT_TRUE(result.warnings.empty()) << result.warnings.front();
    EXPECT_EQ(result.sceneName, "Main");
    ASSERT_EQ(result.objects.size(), 4u);
    ASSERT_EQ(result.roots.size(), 2u);

    auto* root = loaded.FindById(f.root->GetId());
    auto* childA = loaded.FindById(f.childA->GetId());
    auto* childB = loaded.FindById(f.childB->GetId());
    ASSERT_TRUE(root && childA && childB);
    EXPECT_EQ(childA->transform().GetParent(), &root->transform());
    EXPECT_EQ(root->transform().GetChild(0), &childA->transform());
    EXPECT_EQ(root->transform().GetChild(1), &childB->transform());
    EXPECT_FALSE(childB->IsActiveSelf());
    ExpectMatNear(childA->transform().GetLocalToWorldMatrix(), f.childA->transform().GetLocalToWorldMatrix());
    EXPECT_EQ(childA->transform().GetLocalPosition().x, 0.1f) << "float wraca bit w bit";

    auto* body = childA->GetComponent<Body>();
    ASSERT_NE(body, nullptr);
    EXPECT_FALSE(body->enabled);
    EXPECT_FLOAT_EQ(body->mass, 2.5f);
    EXPECT_EQ(body->bodyType, BodyType::Kinematic);
    ExpectVecNear(body->velocity, {0, -9.81f, 0});
    EXPECT_EQ(body->tag, "gracz");
    EXPECT_EQ(body->follow, f.other->GetId());
    EXPECT_EQ(body->material, UUID(0xAAAA, 0xBBBB));

    // Ponowny zapis daje identyczny dokument.
    EXPECT_EQ(SceneSerializer::SerializeScene(loaded)["objects"], doc["objects"]);
}

TEST(SceneSerializer, AwakeSeesLoadedFieldsAndHierarchy) {
    Fixture f;
    Scene loaded;
    SceneSerializer::Deserialize(loaded, SceneSerializer::SerializeScene(f.scene));
    auto* body = loaded.FindById(f.childA->GetId())->GetComponent<Body>();
    EXPECT_FLOAT_EQ(body->massSeenInAwake, 2.5f);
    EXPECT_TRUE(body->parentSeenInAwake);
}

TEST(SceneSerializer, ParentListedAfterChildStillWorks) {
    const UUID p = UUID::Generate(), c = UUID::Generate();
    json doc = {{"version", 1},
                {"objects",
                 {{{"id", c.ToString()}, {"name", "C"}, {"parent", p.ToString()}},
                  {{"id", p.ToString()}, {"name", "P"}, {"parent", nullptr}}}}};
    Scene s;
    auto r = SceneSerializer::Deserialize(s, doc);
    EXPECT_TRUE(r.warnings.empty());
    EXPECT_EQ(s.FindById(c)->transform().GetParent(), &s.FindById(p)->transform());
    ASSERT_EQ(r.roots.size(), 1u);
    EXPECT_EQ(r.roots[0]->GetName(), "P");
}

TEST(SceneSerializer, SubtreeWithRemapMakesIndependentCopy) {
    Fixture f;
    // Body w ChildA wskazuje na Root (wewnątrz poddrzewa) zamiast na Other.
    f.childA->GetComponent<Body>()->follow = f.root->GetId();
    const json prefab = SceneSerializer::SerializeSubtree(*f.root);
    ASSERT_EQ(prefab["objects"].size(), 3u) << "Other nie nalezy do poddrzewa";
    EXPECT_TRUE(prefab["objects"][0]["parent"].is_null());

    IdRemap remap;
    auto r1 = SceneSerializer::Deserialize(f.scene, prefab, &remap);
    IdRemap remap2;
    auto r2 = SceneSerializer::Deserialize(f.scene, prefab, &remap2);
    EXPECT_TRUE(r1.warnings.empty());
    ASSERT_EQ(r1.roots.size(), 1u);
    ASSERT_EQ(r2.roots.size(), 1u);
    EXPECT_EQ(f.scene.GetGameObjectCount(), 4u + 3u + 3u);

    GameObject* copyRoot = r1.roots[0];
    EXPECT_NE(copyRoot->GetId(), f.root->GetId());
    EXPECT_NE(copyRoot->GetId(), r2.roots[0]->GetId());
    EXPECT_EQ(remap.Lookup(f.root->GetId()), copyRoot->GetId());

    auto* copyBody = FindByName(r1.objects, "ChildA")->GetComponent<Body>();
    EXPECT_EQ(copyBody->follow, copyRoot->GetId()) << "referencja wewnatrz prefabu idzie za remapem";
    EXPECT_EQ(copyBody->material, UUID(0xAAAA, 0xBBBB)) << "GUID assetu zostaje";
}

TEST(SceneSerializer, LoadingSameIdsTwiceWithoutRemapIsAtomicError) {
    Fixture f;
    const json doc = SceneSerializer::SerializeScene(f.scene);
    const auto before = f.scene.GetGameObjectCount();
    EXPECT_THROW(SceneSerializer::Deserialize(f.scene, doc), SerializationError);
    EXPECT_EQ(f.scene.GetGameObjectCount(), before) << "blad nie moze zostawic polowy obiektow";
}

TEST(SceneSerializer, StructuralErrorsThrowAndLeaveSceneUntouched) {
    const std::string goodId = UUID::Generate().ToString();
    const json bad[] = {
        json::array(),
        json{{"objects", json::array()}},                                // brak version
        json{{"version", 99}, {"objects", json::array()}},              // za nowa wersja
        json{{"version", 1}},                                            // brak objects
        json{{"version", 1}, {"objects", {{{"name", "bez id"}}}}},       // brak id
        json{{"version", 1}, {"objects", {{{"id", "nie-uuid"}}}}},       // złe id
        json{{"version", 1}, {"objects", {{{"id", goodId}}, {{"id", goodId}}}}}, // powtórzone id
        json{{"version", 1}, {"objects", {{{"id", goodId}, {"components", 5}}}}},
    };
    for (const auto& doc : bad) {
        Scene s;
        EXPECT_THROW(SceneSerializer::Deserialize(s, doc), SerializationError) << doc.dump();
        EXPECT_EQ(s.GetGameObjectCount(), 0u) << doc.dump();
    }
}

TEST(SceneSerializer, SoftProblemsBecomeWarnings) {
    const UUID id = UUID::Generate();
    json doc = {{"version", 1},
                {"objects",
                 {{{"id", id.ToString()},
                   {"name", "X"},
                   {"parent", UUID::Generate().ToString()}, // rodzica nie ma w pliku
                   {"components",
                    {{{"type", "Transform"}, {"position", {1, 2}}, {"scale", {3, 3, 3}}}, // zły vec3
                     {{"type", "NieMaTakiego"}},
                     {{"type", "Body"}, {"mass", "ciezki"}, {"tag", "ok"}, {"nieznane", 1}},
                     {{"bez", "typu"}}}}}}}};
    Scene s;
    auto r = SceneSerializer::Deserialize(s, doc);
    EXPECT_EQ(r.warnings.size(), 6u); // rodzic, position, typ, mass, nieznane pole, brak typu
    auto* go = s.FindById(id);
    ASSERT_NE(go, nullptr);
    EXPECT_EQ(go->transform().GetParent(), nullptr);
    ExpectVecNear(go->transform().GetLocalPosition(), {0, 0, 0});
    ExpectVecNear(go->transform().GetLocalScale(), {3, 3, 3});
    auto* body = go->GetComponent<Body>();
    ASSERT_NE(body, nullptr);
    EXPECT_FLOAT_EQ(body->mass, 1.0f);
    EXPECT_EQ(body->tag, "ok");
}

TEST(SceneSerializer, IntegerRangeIsChecked) {
    const UUID id = UUID::Generate();
    json doc = {{"version", 1},
                {"objects", {{{"id", id.ToString()}, {"components", {{{"type", "Body"}, {"bodyType", 5000000000LL}}}}}}}};
    Scene s;
    auto r = SceneSerializer::Deserialize(s, doc);
    EXPECT_EQ(r.warnings.size(), 1u);
    EXPECT_EQ(s.FindById(id)->GetComponent<Body>()->bodyType, BodyType::Dynamic);
}

TEST(SceneSerializer, PendingDestroyIsNotSaved) {
    Fixture f;
    f.scene.Destroy(*f.childA);
    const json doc = SceneSerializer::SerializeScene(f.scene);
    EXPECT_EQ(doc["objects"].size(), 3u);
}

TEST(SceneSerializer, FileRoundTrip) {
    Fixture f;
    const auto dir = std::filesystem::temp_directory_path() / ("wz_serializer_" + UUID::Generate().ToString());
    std::filesystem::create_directories(dir);
    const auto path = dir / "main.scene.json";

    SceneSerializer::SaveToFile(f.scene, path);
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));

    Scene loaded;
    auto r = SceneSerializer::LoadFromFile(loaded, path);
    EXPECT_EQ(r.objects.size(), 4u);
    EXPECT_NE(loaded.FindById(f.childA->GetId())->GetComponent<Body>(), nullptr);

    std::ofstream(dir / "broken.json") << "{ nie json";
    Scene s2;
    EXPECT_THROW(SceneSerializer::LoadFromFile(s2, dir / "broken.json"), SerializationError);
    EXPECT_THROW(SceneSerializer::LoadFromFile(s2, dir / "brak.json"), SerializationError);
    std::filesystem::remove_all(dir);
}

namespace {
std::string ThrowMessage(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const SerializationError& e) {
        return e.what();
    } catch (const std::exception& e) {
        return std::string("ZLY TYP WYJATKU: ") + e.what();
    }
    return "BRAK WYJATKU";
}
} // namespace

// Wojcimierz patrzy: ani NaN, ani nieskończoność nie przejdą po cichu do pliku.
TEST(SceneSerializer, NonFiniteFloatOnSaveIsSerializationErrorWithPath) {
    const float bad[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()};
    for (float v : bad) {
        Fixture f;
        f.childA->GetComponent<Body>()->mass = v;
        const std::string msg = ThrowMessage([&] { SceneSerializer::SerializeScene(f.scene); });
        EXPECT_NE(msg.find("ChildA"), std::string::npos) << msg;
        EXPECT_NE(msg.find("Body.mass"), std::string::npos) << msg;
        EXPECT_NE(msg.find(std::isnan(v) ? "NaN" : "Inf"), std::string::npos) << msg;
    }
    Fixture f;
    f.childA->GetComponent<Body>()->velocity.y = std::numeric_limits<float>::quiet_NaN();
    EXPECT_NE(ThrowMessage([&] { SceneSerializer::SerializeScene(f.scene); }).find("Body.velocity[1]"),
              std::string::npos);
    Fixture g;
    g.root->transform().SetLocalPosition({0, std::numeric_limits<float>::infinity(), 0});
    EXPECT_NE(ThrowMessage([&] { SceneSerializer::SerializeSubtree(*g.root); }).find("Transform.position[1]"),
              std::string::npos);
}

TEST(SceneSerializer, FailedSaveLeavesExistingFileUntouched) {
    const auto dir = std::filesystem::temp_directory_path() / ("wz_serializer_" + UUID::Generate().ToString());
    std::filesystem::create_directories(dir);
    const auto path = dir / "main.scene.json";
    Fixture f;
    SceneSerializer::SaveToFile(f.scene, path);
    const auto before = std::filesystem::file_size(path);

    f.childA->GetComponent<Body>()->mass = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(SceneSerializer::SaveToFile(f.scene, path), SerializationError);
    EXPECT_EQ(std::filesystem::file_size(path), before);
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    std::filesystem::remove_all(dir);
}

TEST(SceneSerializer, InvalidUtf8OnSaveIsSerializationError) {
    const std::string badBytes = "zla\xC3\x28nazwa"; // urwana sekwencja dwubajtowa
    {
        Fixture f;
        f.other->SetName(badBytes);
        EXPECT_NE(ThrowMessage([&] { SceneSerializer::SerializeScene(f.scene); }).find("UTF-8"), std::string::npos);
    }
    {
        Fixture f;
        f.childA->GetComponent<Body>()->tag = "\xED\xA0\x80"; // surogat zakodowany w UTF-8
        const std::string msg = ThrowMessage([&] { SceneSerializer::SerializeScene(f.scene); });
        EXPECT_NE(msg.find("Body.tag"), std::string::npos) << msg;
    }
    {
        Scene s(std::string("\xC0\xAF")); // overlong
        EXPECT_NE(ThrowMessage([&] { SceneSerializer::SerializeScene(s); }).find("nazwa sceny"), std::string::npos);
    }
    {
        Fixture f; // poprawny, wielobajtowy UTF-8 przechodzi
        f.other->SetName("Żółć 🐉 Wojcimierza");
        Scene loaded;
        SceneSerializer::Deserialize(loaded, json::parse(SceneSerializer::SerializeScene(f.scene).dump()));
        EXPECT_NE(loaded.Find("Żółć 🐉 Wojcimierza"), nullptr);
    }
}

TEST(SceneSerializer, InvalidUtf8OnLoadIsSerializationError) {
    const auto dir = std::filesystem::temp_directory_path() / ("wz_serializer_" + UUID::Generate().ToString());
    std::filesystem::create_directories(dir);
    const auto path = dir / "bad_utf8.json";
    std::ofstream(path, std::ios::binary)
        << "{\"version\":1,\"name\":\"a\xFF\",\"objects\":[]}";
    Scene s;
    EXPECT_EQ(ThrowMessage([&] { SceneSerializer::LoadFromFile(s, path); }).rfind("Niepoprawny JSON", 0), 0u);

    // Dokument zbudowany w pamięci (bez parsera): zła nazwa obiektu to błąd, zły string w polu to ostrzeżenie.
    json doc = {{"version", 1}, {"name", "x"}, {"objects", json::array()}};
    doc["objects"].push_back({{"id", UUID::Generate().ToString()}, {"name", "\xFF"}, {"components", json::array()}});
    EXPECT_NE(ThrowMessage([&] { SceneSerializer::Deserialize(s, doc); }).find("UTF-8"), std::string::npos);
    EXPECT_EQ(s.GetGameObjectCount(), 0u);

    doc["objects"][0]["name"] = "ok";
    doc["objects"][0]["components"].push_back({{"type", "Body"}, {"tag", "\xFF"}});
    auto r = SceneSerializer::Deserialize(s, doc);
    EXPECT_EQ(s.Find("ok")->GetComponent<Body>()->tag, "");
    EXPECT_EQ(r.warnings.size(), 1u);
    std::filesystem::remove_all(dir);
}

TEST(SceneSerializer, OutOfRangeFloatsOnLoadAreRejected) {
    json doc = {{"version", 1}, {"name", "x"}, {"objects", json::array()}};
    doc["objects"].push_back({{"id", UUID::Generate().ToString()},
                              {"name", "a"},
                              {"components",
                               {{{"type", "Transform"}, {"position", {1.0, 1e39, 0.0}}},
                                {{"type", "Body"}, {"mass", -1e300}, {"velocity", {0.0, 2.0, 3.0}}}}}});
    Scene s;
    auto r = SceneSerializer::Deserialize(s, doc);
    GameObject* a = s.Find("a");
    ExpectVecNear(a->transform().GetLocalPosition(), {0, 0, 0});
    EXPECT_FLOAT_EQ(a->GetComponent<Body>()->mass, 1.0f);
    ExpectVecNear(a->GetComponent<Body>()->velocity, {0, 2, 3});
    EXPECT_EQ(r.warnings.size(), 2u);
}
