// =============================================================================
// Wojcimierz Engine - engine/scene/GameObject.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "scene/GameObject.h"
#include "scene/Scene.h"

#include <stdexcept>

namespace wz {

GameObject::GameObject(Scene& scene, std::string name) : scene_(&scene), name_(std::move(name)) {
    auto t = std::make_unique<Transform>();
    transform_ = t.get();
    t->owner_ = this;
    components_.push_back(std::move(t));
}

GameObject::~GameObject() {
    InvokeOnDestroy();
    // Komponenty niszczymy w odwrotnej kolejnosci; Transform (indeks 0) na koncu.
    while (!components_.empty()) components_.pop_back();
}

bool GameObject::IsActiveInHierarchy() const {
    for (const Transform* t = transform_; t; t = t->GetParent())
        if (!t->gameObject().activeSelf_) return false;
    return true;
}

Component& GameObject::AddComponent(std::unique_ptr<Component> component) {
    if (!component) throw std::invalid_argument("GameObject::AddComponent: pusty komponent");
    if (dynamic_cast<Transform*>(component.get()))
        throw std::invalid_argument("GameObject::AddComponent: Transform jest dodawany automatycznie");
    if (component->owner_) throw std::invalid_argument("GameObject::AddComponent: komponent ma juz wlasciciela");
    Component& ref = *component;
    Attach(std::move(component));
    return ref;
}

void GameObject::Attach(std::unique_ptr<Component> comp) {
    comp->owner_ = this;
    Component* raw = comp.get();
    components_.push_back(std::move(comp));
    raw->Awake();
    scene_->RegisterPendingStart(raw);
}

void GameObject::InvokeOnDestroy() {
    if (onDestroyCalled_) return;
    onDestroyCalled_ = true;
    for (std::size_t i = components_.size(); i-- > 0;) {
        Component* c = components_[i].get();
        if (!c->destroyed_) {
            c->destroyed_ = true;
            c->OnDestroy();
        }
    }
}

} // namespace wz
