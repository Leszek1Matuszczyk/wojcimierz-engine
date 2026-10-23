#pragma once

namespace wz {

class GameObject;
class Transform;
class Scene;

// Baza wszystkich komponentow - odpowiednik MonoBehaviour z Unity.
// Kolejnosc wywolan (jak w Unity):
//   Awake()      - od razu po AddComponent
//   Start()      - raz, przed pierwszym FixedUpdate/Update komponentu
//   FixedUpdate  - co staly krok (Time::fixedDeltaTime)
//   Update       - co klatke
//   LateUpdate   - co klatke, po wszystkich Update
//   OnDestroy()  - przy niszczeniu GameObjectu lub sceny
class Component {
public:
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    GameObject& gameObject() const { return *owner_; }
    Transform& transform() const;
    Scene& scene() const;

    bool enabled = true;
    bool HasStarted() const { return started_; }
    bool IsActiveAndEnabled() const;

    virtual void Awake() {}
    virtual void Start() {}
    virtual void FixedUpdate(float /*fixedDeltaTime*/) {}
    virtual void Update(float /*deltaTime*/) {}
    virtual void LateUpdate(float /*deltaTime*/) {}
    virtual void OnDestroy() {}

protected:
    Component() = default;

private:
    friend class GameObject;
    friend class Scene;
    GameObject* owner_ = nullptr;
    bool started_ = false;
    bool destroyed_ = false;
};

} // namespace wz
