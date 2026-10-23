// =============================================================================
// Wojcimierz Engine - engine/scene/Transform.cpp
// Na chwałę czcigodnego Wojcimierza.
// =============================================================================
#include "scene/Transform.h"

#include "core/Math.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace wz {

// -----------------------------------------------------------------------------
// Straznik NaN/Inf (na chwale czcigodnego Wojcimierza, ktory nie dopuszcza
// trucizny do hierarchii): kazdy publiczny setter sprawdza wejscie oraz wynik
// obliczen (np. odwrotnosc macierzy rodzica o zerowej skali). Wartosc
// nieskonczona lub kwaternion zerowy jest odrzucany - zostaje POPRZEDNIA
// wartosc, a ostrzezenie trafia do logu (z ograniczeniem powtorzen).
// -----------------------------------------------------------------------------
namespace {
// true = wejscie odrzucone (z ostrzezeniem).
bool RejectVec(const glm::vec3& v, const char* what) {
    if (IsFinite(v)) return false;
    (void)SanitizeVec3(v, glm::vec3(0.0f), what);
    return true;
}
}  // namespace

Transform::~Transform() {
    // Odpinamy sie od rodzica i odpinamy dzieci, zeby nie zostaly wiszace wskazniki.
    if (parent_) {
        auto& siblings = parent_->children_;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        parent_ = nullptr;
    }
    for (Transform* c : children_) {
        c->parent_ = nullptr;
        c->MarkChanged(true);
    }
    children_.clear();
}

void Transform::MarkChanged(bool bumpVersion) {
    // Iteracyjnie (bez rekurencji), zeby gleboka hierarchia nie przepelnila stosu.
    std::vector<Transform*> stack{this};
    while (!stack.empty()) {
        Transform* t = stack.back();
        stack.pop_back();
        t->worldDirty_ = true;
        if (bumpVersion) ++t->changeVersion_;
        for (Transform* c : t->children_) stack.push_back(c);
    }
}

void Transform::SetLocalPosition(const glm::vec3& p) {
    if (RejectVec(p, "Transform.SetLocalPosition")) return;
    localPosition_ = p;
    MarkChanged(true);
}
void Transform::SetLocalRotation(const glm::quat& r) {
    localRotation_ = SanitizeRotation(r, localRotation_, "Transform.SetLocalRotation");
    MarkChanged(true);
}
void Transform::SetLocalScale(const glm::vec3& s) {
    if (RejectVec(s, "Transform.SetLocalScale")) return;
    localScale_ = s;
    MarkChanged(true);
}

void Transform::SetLocalEulerAngles(const glm::vec3& degrees) {
    if (RejectVec(degrees, "Transform.SetLocalEulerAngles")) return;
    SetLocalRotation(glm::quat(glm::radians(degrees)));
}

glm::vec3 Transform::GetLocalEulerAngles() const { return glm::degrees(glm::eulerAngles(localRotation_)); }

glm::mat4 Transform::GetLocalMatrix() const {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), localPosition_);
    m *= glm::mat4_cast(localRotation_);
    m = glm::scale(m, localScale_);
    return m;
}

const glm::mat4& Transform::GetLocalToWorldMatrix() const {
    if (worldDirty_) {
        worldMatrix_ = parent_ ? parent_->GetLocalToWorldMatrix() * GetLocalMatrix() : GetLocalMatrix();
        worldDirty_ = false;
    }
    return worldMatrix_;
}

glm::mat4 Transform::GetWorldToLocalMatrix() const { return glm::inverse(GetLocalToWorldMatrix()); }

glm::vec3 Transform::GetPosition() const { return glm::vec3(GetLocalToWorldMatrix()[3]); }

void Transform::SetPosition(const glm::vec3& worldPosition) {
    if (RejectVec(worldPosition, "Transform.SetPosition")) return;
    const glm::vec3 local = parent_ ? glm::vec3(parent_->GetWorldToLocalMatrix() * glm::vec4(worldPosition, 1.0f))
                                    : worldPosition;
    // Wynik tez sprawdzamy: rodzic o zerowej skali daje macierz nieodwracalna.
    if (RejectVec(local, "Transform.SetPosition (lokalna)")) return;
    localPosition_ = local;
    MarkChanged(true);
}

glm::quat Transform::GetRotation() const {
    return parent_ ? glm::normalize(parent_->GetRotation() * localRotation_) : localRotation_;
}

void Transform::SetRotation(const glm::quat& worldRotation) {
    const glm::quat world = SanitizeRotation(worldRotation, GetRotation(), "Transform.SetRotation");
    const glm::quat local = parent_ ? glm::inverse(parent_->GetRotation()) * world : world;
    localRotation_ = SanitizeRotation(local, localRotation_, "Transform.SetRotation (lokalny)");
    MarkChanged(true);
}

void Transform::SetWorldPoseInternal(const glm::vec3& p, const glm::quat& r) {
    // Pozycja i obrot walidowane niezaleznie: zly obrot z fizyki nie blokuje
    // poprawnej pozycji (i odwrotnie) - Wojcimierz rozdziela ziarno od plew.
    if (!RejectVec(p, "Transform.SetWorldPoseFromPhysics (pozycja)")) {
        const glm::vec3 local = parent_ ? glm::vec3(parent_->GetWorldToLocalMatrix() * glm::vec4(p, 1.0f)) : p;
        localPosition_ = SanitizeVec3(local, localPosition_, "Transform.SetWorldPoseFromPhysics (pozycja lokalna)");
    }
    const glm::quat world = SanitizeRotation(r, GetRotation(), "Transform.SetWorldPoseFromPhysics (obrot)");
    const glm::quat local = parent_ ? glm::inverse(parent_->GetRotation()) * world : world;
    localRotation_ = SanitizeRotation(local, localRotation_, "Transform.SetWorldPoseFromPhysics (obrot lokalny)");
}

void Transform::SetWorldPoseFromPhysics(const glm::vec3& worldPosition, const glm::quat& worldRotation) {
    SetWorldPoseInternal(worldPosition, worldRotation);
    MarkChanged(false);
}

glm::vec3 Transform::GetLossyScale() const { return parent_ ? parent_->GetLossyScale() * localScale_ : localScale_; }

glm::vec3 Transform::Forward() const { return glm::normalize(GetRotation() * glm::vec3(0, 0, -1)); }
glm::vec3 Transform::Right() const { return glm::normalize(GetRotation() * glm::vec3(1, 0, 0)); }
glm::vec3 Transform::Up() const { return glm::normalize(GetRotation() * glm::vec3(0, 1, 0)); }

glm::vec3 Transform::TransformPoint(const glm::vec3& local) const {
    return glm::vec3(GetLocalToWorldMatrix() * glm::vec4(local, 1.0f));
}

glm::vec3 Transform::InverseTransformPoint(const glm::vec3& world) const {
    return glm::vec3(GetWorldToLocalMatrix() * glm::vec4(world, 1.0f));
}

void Transform::Translate(const glm::vec3& delta, bool worldSpace) {
    if (RejectVec(delta, "Transform.Translate")) return;
    // Suma skonczonych moze sie przepelnic do Inf - wtedy odrzuci ja setter.
    if (worldSpace) SetPosition(GetPosition() + delta);
    else SetLocalPosition(localPosition_ + localRotation_ * delta);
}

void Transform::Rotate(const glm::vec3& eulerDegrees, bool worldSpace) {
    if (RejectVec(eulerDegrees, "Transform.Rotate")) return;
    const glm::quat q(glm::radians(eulerDegrees));
    if (worldSpace) SetRotation(q * GetRotation());
    else SetLocalRotation(localRotation_ * q);
}

void Transform::LookAt(const glm::vec3& worldTarget, const glm::vec3& worldUp) {
    if (RejectVec(worldTarget, "Transform.LookAt (cel)")) return;
    if (RejectVec(worldUp, "Transform.LookAt (gora)")) return;
    const glm::vec3 dir = worldTarget - GetPosition();
    const float len2 = glm::dot(dir, dir);
    if (!(len2 >= 1e-12f) || !IsFinite(len2)) return;  // !(>=) lapie tez NaN
    // glm::quatLookAt zwraca obrot, w ktorym -Z patrzy w strone 'dir'.
    // 'up' rownolegle do 'dir' lub zerowe daje NaN - SetRotation go odrzuci.
    SetRotation(glm::quatLookAt(glm::normalize(dir), worldUp));
}

bool Transform::IsChildOf(const Transform* other) const {
    for (const Transform* t = this; t; t = t->parent_)
        if (t == other) return true;
    return false;
}

Transform* Transform::Root() {
    Transform* t = this;
    while (t->parent_) t = t->parent_;
    return t;
}

bool Transform::SetParent(Transform* newParent, bool worldPositionStays) {
    if (newParent == parent_) return true;
    if (newParent && newParent->IsChildOf(this)) return false; // cykl

    const glm::mat4 world = GetLocalToWorldMatrix();

    if (parent_) {
        auto& siblings = parent_->children_;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }
    parent_ = newParent;
    if (parent_) parent_->children_.push_back(this);

    if (worldPositionStays) {
        const glm::mat4 local = parent_ ? glm::inverse(parent_->GetLocalToWorldMatrix()) * world : world;
        glm::vec3 scale, translation, skew;
        glm::vec4 perspective;
        glm::quat rotation;
        if (glm::decompose(local, scale, rotation, translation, skew, perspective) && IsFinite(scale) &&
            IsFinite(translation) && IsFinite(rotation)) {
            localPosition_ = translation;
            localRotation_ = glm::normalize(rotation);
            localScale_ = scale;
        }
    }
    MarkChanged(true);
    return true;
}

void Transform::DetachChildren() {
    auto copy = children_;
    for (Transform* c : copy) c->SetParent(nullptr, true);
}

} // namespace wz
