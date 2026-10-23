// =============================================================================
// Wojcimierz Engine - engine/scene/GameObject.h
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#pragma once
#include "scene/Component.h"
#include "scene/Transform.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace wz {

class Scene;

// Odpowiednik UnityEngine.GameObject: nazwany kontener komponentow.
// Kazdy GameObject ma zawsze dokladnie jeden Transform (pierwszy komponent).
// Obiekty tworzy i posiada Scene (Scene::CreateGameObject).
class GameObject {
public:
    GameObject(Scene& scene, std::string name);
    ~GameObject();

    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    const std::string& GetName() const { return name_; }
    void SetName(std::string n) { name_ = std::move(n); }

    Transform& transform() const { return *transform_; }
    Scene& scene() const { return *scene_; }

    void SetActive(bool active) { activeSelf_ = active; }
    bool IsActiveSelf() const { return activeSelf_; }
    bool IsActiveInHierarchy() const;
    bool IsPendingDestroy() const { return pendingDestroy_; }

    // Dodaje komponent i od razu wywoluje Awake(). Start() nastapi przed pierwszym Update.
    template <typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T musi dziedziczyc po wz::Component");
        static_assert(!std::is_same_v<T, Transform>, "Transform jest dodawany automatycznie");
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *comp;
        Attach(std::move(comp));
        return ref;
    }

    template <typename T>
    T* GetComponent() const {
        for (const auto& c : components_)
            if (auto* p = dynamic_cast<T*>(c.get())) return p;
        return nullptr;
    }

    template <typename T>
    std::vector<T*> GetComponents() const {
        std::vector<T*> out;
        for (const auto& c : components_)
            if (auto* p = dynamic_cast<T*>(c.get())) out.push_back(p);
        return out;
    }

    std::size_t GetComponentCount() const { return components_.size(); }
    Component& GetComponentAt(std::size_t i) const { return *components_.at(i); }

private:
    friend class Scene;
    void Attach(std::unique_ptr<Component> comp);
    void InvokeOnDestroy();

    Scene* scene_;
    std::string name_;
    bool activeSelf_ = true;
    bool pendingDestroy_ = false;
    bool onDestroyCalled_ = false;
    Transform* transform_ = nullptr;
    std::vector<std::unique_ptr<Component>> components_;
};

} // namespace wz
