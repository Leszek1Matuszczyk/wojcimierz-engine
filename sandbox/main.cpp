// =============================================================================
// Wojcimierz Engine - sandbox/main.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
// Sandbox: minimalna "gra" zbudowana z komponentow, tak jak w Unity.
//   sandbox                                   - okno, Esc zamyka
//   sandbox --headless --frames 60 --screenshot out.png
#include "core/Engine.h"
#include "core/Time.h"
#include "platform/Input.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "render/MeshRenderer.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <cmath>
#include <cstdio>

using namespace wz;

// Odpowiednik prostego MonoBehaviour: obraca obiekt ze stala predkoscia.
class Spinner : public Component {
public:
    explicit Spinner(glm::vec3 degreesPerSecond) : speed_(degreesPerSecond) {}
    void Update(float dt) override { transform().Rotate(speed_ * dt); }

private:
    glm::vec3 speed_;
};

// Ruch po okregu w FixedUpdate (krok staly - jak fizyka w Unity).
class Orbiter : public Component {
public:
    Orbiter(float radius, float speed) : radius_(radius), speed_(speed) {}
    void FixedUpdate(float dt) override {
        angle_ += speed_ * dt;
        transform().SetLocalPosition({std::cos(angle_) * radius_, 0.0f, std::sin(angle_) * radius_});
    }

private:
    float radius_, speed_, angle_ = 0.0f;
};

class QuitOnEscape : public Component {
public:
    void Update(float) override {
        if (Input::GetKeyDown(KeyCode::Escape)) quit = true;
    }
    bool quit = false;
};

int main(int argc, char** argv) {
    EngineConfig cfg;
    cfg.window.title = "Wojcimierz Engine - Sandbox";
    std::string err;
    if (!EngineConfig::ParseArgs(argc, argv, cfg, err)) {
        std::fprintf(stderr, "%s\nUzycie: sandbox [--headless] [--frames N] [--screenshot plik.png] [--width W] [--height H]\n",
                     err.c_str());
        return 1;
    }

    try {
        Engine engine(cfg);
        Scene& scene = engine.GetScene();
        auto shader = Shader::CreateDefaultLit(engine.GetDevice());
        auto cube = Mesh::Create(engine.GetDevice(), MeshData::Cube());
        auto tri = Mesh::Create(engine.GetDevice(), MeshData::Triangle());

        GameObject& camGo = scene.CreateGameObject("Main Camera");
        camGo.AddComponent<Camera>();
        camGo.transform().SetPosition({0.0f, 1.5f, 4.0f});
        camGo.transform().LookAt({0.0f, 0.0f, 0.0f});
        auto& quit = camGo.AddComponent<QuitOnEscape>();

        GameObject& cubeGo = scene.CreateGameObject("Cube");
        cubeGo.AddComponent<MeshRenderer>(cube, shader);
        cubeGo.AddComponent<Spinner>(glm::vec3(20.0f, 45.0f, 0.0f));

        // Trojkat jako dziecko pustego obiektu-pivota, krazy wokol szescianu.
        GameObject& pivot = scene.CreateGameObject("Pivot");
        pivot.AddComponent<Orbiter>(1.6f, 1.2f);
        GameObject& triGo = scene.CreateGameObject("Triangle");
        triGo.transform().SetParent(&pivot.transform(), false);
        triGo.transform().SetLocalScale(glm::vec3(0.6f));
        auto& triR = triGo.AddComponent<MeshRenderer>(tri, shader);
        triR.unlit = true;
        triR.cullBackFaces = false;
        triGo.AddComponent<Spinner>(glm::vec3(0.0f, 90.0f, 0.0f));

        // Wyjscie z petli po Esc (sprawdzane po kazdej klatce przez komponent).
        class QuitBridge : public Component {
        public:
            QuitBridge(Engine& e, QuitOnEscape& q) : e_(e), q_(q) {}
            void LateUpdate(float) override { if (q_.quit) e_.RequestQuit(); }
        private:
            Engine& e_;
            QuitOnEscape& q_;
        };
        camGo.AddComponent<QuitBridge>(engine, quit);

        const int rc = engine.Run();
        std::printf("[Sandbox] klatek: %llu, krokow FixedUpdate: %llu, czas gry: %.3f s\n",
                    static_cast<unsigned long long>(Time::frameCount),
                    static_cast<unsigned long long>(Time::fixedStepCount), Time::time);
        return rc;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Sandbox] blad: %s\n", e.what());
        return 3;
    }
}
