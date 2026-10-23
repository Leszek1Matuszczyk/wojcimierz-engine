// =============================================================================
// Wojcimierz Engine - engine/scene/Scene.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "scene/Scene.h"

#include <algorithm>

namespace wz {

Scene::Scene(std::string name) : name_(std::move(name)) {}

Scene::~Scene() {
    destroying_ = true;
    pendingStart_.clear();
    // Najpierw OnDestroy na wszystkich obiektach (przy zywej hierarchii), potem zwalnianie.
    for (auto& go : objects_) go->InvokeOnDestroy();
    while (!objects_.empty()) objects_.pop_back();
}

GameObject& Scene::CreateGameObject(std::string name) {
    objects_.push_back(std::make_unique<GameObject>(*this, std::move(name)));
    return *objects_.back();
}

void Scene::Destroy(GameObject& go) {
    // Niszczymy cale poddrzewo hierarchii.
    std::vector<Transform*> stack{&go.transform()};
    while (!stack.empty()) {
        Transform* t = stack.back();
        stack.pop_back();
        t->gameObject().pendingDestroy_ = true;
        for (Transform* c : t->GetChildren()) stack.push_back(c);
    }
}

void Scene::FlushDestroyed() {
    bool any = std::any_of(objects_.begin(), objects_.end(), [](const auto& g) { return g->pendingDestroy_; });
    if (!any) return;

    // 1) OnDestroy na wszystkich niszczonych (hierarchia jeszcze nienaruszona).
    for (auto& go : objects_)
        if (go->pendingDestroy_) go->InvokeOnDestroy();

    // 2) Usuniecie z listy oczekujacych na Start.
    std::erase_if(pendingStart_, [](Component* c) { return c->gameObject().pendingDestroy_; });

    // 3) Zwolnienie pamieci. Najpierw dzieci (glebsze), zeby destruktory Transformow
    //    nie odwolywaly sie do juz zwolnionych rodzicow.
    std::vector<std::unique_ptr<GameObject>> doomed;
    std::vector<std::unique_ptr<GameObject>> alive;
    for (auto& go : objects_) (go->pendingDestroy_ ? doomed : alive).push_back(std::move(go));
    objects_ = std::move(alive);
    auto depth = [](const GameObject& g) {
        int d = 0;
        for (const Transform* t = g.transform().GetParent(); t; t = t->GetParent()) ++d;
        return d;
    };
    std::stable_sort(doomed.begin(), doomed.end(),
                     [&](const auto& a, const auto& b) { return depth(*a) > depth(*b); });
    doomed.clear();
}

GameObject* Scene::Find(const std::string& name) const {
    for (const auto& go : objects_)
        if (!go->pendingDestroy_ && go->GetName() == name) return go.get();
    return nullptr;
}

void Scene::RunPendingStarts() {
    // Start moze dodawac nowe komponenty - przetwarzamy liste az do oproznienia,
    // ale pomijamy (zostawiamy na pozniej) komponenty nieaktywne.
    std::vector<Component*> deferred;
    while (!pendingStart_.empty()) {
        std::vector<Component*> batch;
        batch.swap(pendingStart_);
        for (Component* c : batch) {
            if (c->started_ || c->destroyed_) continue;
            if (!c->IsActiveAndEnabled()) { deferred.push_back(c); continue; }
            c->started_ = true;
            c->Start();
        }
    }
    pendingStart_ = std::move(deferred);
}

template <typename Fn>
void Scene::ForEachLiveComponent(Fn&& fn) {
    // Iteracja po indeksach z zapamietanym rozmiarem: obiekty/komponenty dodane
    // w trakcie dostana zdarzenia dopiero od nastepnej klatki (jak w Unity).
    const std::size_t objectCount = objects_.size();
    for (std::size_t i = 0; i < objectCount; ++i) {
        GameObject* go = objects_[i].get();
        if (go->pendingDestroy_ || !go->IsActiveInHierarchy()) continue;
        const std::size_t compCount = go->components_.size();
        for (std::size_t j = 0; j < compCount; ++j) {
            Component* c = go->components_[j].get();
            if (c->enabled && c->started_ && !c->destroyed_) fn(*c);
        }
    }
}

void Scene::FixedUpdate(float fixedDeltaTime) {
    RunPendingStarts();
    ForEachLiveComponent([&](Component& c) { c.FixedUpdate(fixedDeltaTime); });
}

void Scene::Update(float deltaTime) {
    RunPendingStarts();
    ForEachLiveComponent([&](Component& c) { c.Update(deltaTime); });
}

void Scene::LateUpdate(float deltaTime) {
    ForEachLiveComponent([&](Component& c) { c.LateUpdate(deltaTime); });
    FlushDestroyed();
}

} // namespace wz
