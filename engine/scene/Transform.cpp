// ============================================================
//  Wojcimierz Engine: scene/Transform.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
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
// true = kwaternion przyjety (znormalizowany w `out`); false = odrzucony z ostrzezeniem.
bool AcceptRotation(const glm::quat& q, glm::quat& out, const char* what) {
    if (TryNormalizeRotation(q, out)) return true;
    WarnRejectedValue(what, "quat (NaN/Inf lub zerowa dlugosc)");
    return false;
}
}  // namespace

// Zasada wersjonowania (#24, na chwale czcigodnego Wojcimierza): MarkChanged
// wolamy WYLACZNIE, gdy stan faktycznie sie zmienil. Odrzucone wywolanie nie
// ustawia flagi dirty ani nie podbija changeVersion (inaczej fizyka widzialaby
// "teleport" w kazdej klatce), a zapis identycznej wartosci tez jest no-opem.
bool Transform::AssignLocalPosition(const glm::vec3& p) {
    if (p == localPosition_) return false;
    localPosition_ = p;
    return true;
}
bool Transform::AssignLocalRotation(const glm::quat& r) {
    if (r == localRotation_) return false;
    localRotation_ = r;
    return true;
}

// Tolerancje #27 (kWorldRoundTripEpsilon, kWorldPositionRelTolerance) sa
// zmierzone i opisane w core/Math.h. Setery swiatowe porownuja wartosc w
// PRZESTRZENI SWIATA (zadana vs biezaca); jesli to faktyczna zmiana, nowa
// wartosc lokalna jest zapisywana z porownaniem DOKLADNYM - druga tolerancja po
// stronie lokalnej moglaby tylko polknac celowy mikroobrot. Wojcimierz wybacza
// floatom drobne grzechy, ale nie gubi prawdziwego ruchu.
bool Transform::AssignLocalRotationConverted(const glm::quat& r) {
    if (SameRotationWithin(r, localRotation_, kWorldRoundTripEpsilon)) return false;
    localRotation_ = r;
    return true;
}

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
    if (AssignLocalPosition(p)) MarkChanged(true);
}
void Transform::SetLocalRotation(const glm::quat& r) {
    glm::quat q;
    if (!AcceptRotation(r, q, "Transform.SetLocalRotation")) return;
    if (AssignLocalRotation(q)) MarkChanged(true);
}
void Transform::SetLocalScale(const glm::vec3& s) {
    if (RejectVec(s, "Transform.SetLocalScale")) return;
    if (s == localScale_) return;
    localScale_ = s;
    MarkChanged(true);
}

void Transform::SetLocalEulerAngles(const glm::vec3& degrees) {
    if (RejectVec(degrees, "Transform.SetLocalEulerAngles")) return;
    // Katy identyczne z tym, co zwraca GetLocalEulerAngles() - dokladny no-op.
    // Round-trip kwaternion -> Euler (float, stopnie) -> kwaternion gubi do ~55 eps
    // w okolicy gimbal lock, czego zadna tolerancja nie oddzieli od obrotu o 1e-6 rad,
    // wiec sprawdzamy najpierw same katy (tak jak setery swiatowe - swiat).
    if (degrees == GetLocalEulerAngles()) return;
    glm::quat q;
    if (!AcceptRotation(glm::quat(glm::radians(degrees)), q, "Transform.SetLocalEulerAngles")) return;
    if (AssignLocalRotationConverted(q)) MarkChanged(true);
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
    // #27: porownanie w PRZESTRZENI SWIATA (tam, gdzie wolajacy podal wartosc).
    // Round-trip swiat -> lokal -> swiat przez macierz rodzica gubi do 12.5 eps
    // wzglednie, wiec ponowne SetPosition(p) nie moze byc "teleportem".
    if (SamePositionWithin(worldPosition, GetPosition(), kWorldPositionRelTolerance)) return;
    const glm::vec3 local = parent_ ? glm::vec3(parent_->GetWorldToLocalMatrix() * glm::vec4(worldPosition, 1.0f))
                                    : worldPosition;
    // Wynik tez sprawdzamy: rodzic o zerowej skali daje macierz nieodwracalna.
    if (RejectVec(local, "Transform.SetPosition (lokalna)")) return;
    if (AssignLocalPosition(local)) MarkChanged(true);
}

glm::quat Transform::GetRotation() const {
    return parent_ ? glm::normalize(parent_->GetRotation() * localRotation_) : localRotation_;
}

void Transform::SetRotation(const glm::quat& worldRotation) {
    glm::quat world, local;
    if (!AcceptRotation(worldRotation, world, "Transform.SetRotation")) return;
    if (SameRotationWithin(world, GetRotation(), kWorldRoundTripEpsilon)) return;  // #27: porownanie w swiecie
    if (!AcceptRotation(parent_ ? glm::inverse(parent_->GetRotation()) * world : world, local,
                        "Transform.SetRotation (lokalny)"))
        return;
    if (AssignLocalRotation(local)) MarkChanged(true);
}

bool Transform::SetWorldPoseInternal(const glm::vec3& p, const glm::quat& r) {
    // Pozycja i obrot walidowane niezaleznie: zly obrot z fizyki nie blokuje
    // poprawnej pozycji (i odwrotnie) - Wojcimierz rozdziela ziarno od plew.
    // Obie lokalne wartosci liczymy PRZED zapisem (obrot rodzica nie zalezy od nas).
    bool changed = false;
    if (!RejectVec(p, "Transform.SetWorldPoseFromPhysics (pozycja)") &&
        !SamePositionWithin(p, GetPosition(), kWorldPositionRelTolerance)) {  // #27: ta sama poza = brak zmiany
        const glm::vec3 local = parent_ ? glm::vec3(parent_->GetWorldToLocalMatrix() * glm::vec4(p, 1.0f)) : p;
        if (!RejectVec(local, "Transform.SetWorldPoseFromPhysics (pozycja lokalna)"))
            changed |= AssignLocalPosition(local);
    }
    glm::quat world, local;
    if (AcceptRotation(r, world, "Transform.SetWorldPoseFromPhysics (obrot)") &&
        !SameRotationWithin(world, GetRotation(), kWorldRoundTripEpsilon) &&
        AcceptRotation(parent_ ? glm::inverse(parent_->GetRotation()) * world : world, local,
                       "Transform.SetWorldPoseFromPhysics (obrot lokalny)"))
        changed |= AssignLocalRotation(local);
    return changed;
}

void Transform::SetWorldPoseFromPhysics(const glm::vec3& worldPosition, const glm::quat& worldRotation) {
    // Bez podbijania wersji; cache uniewazniamy tylko, gdy cos sie zmienilo.
    if (SetWorldPoseInternal(worldPosition, worldRotation)) MarkChanged(false);
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
    if (worldSpace) {
        SetRotation(q * GetRotation());
        return;
    }
    // Rotate to zlozenie (konwersja Euler + mnozenie + renormalizacja), nie zapis
    // wartosci podanej przez wolajacego - dlatego tolerancja kWorldRoundTripEpsilon
    // (#27): Rotate(0) jest no-opem, a obrot o 1e-6 rad nadal podbija wersje.
    glm::quat r;
    if (!AcceptRotation(localRotation_ * q, r, "Transform.Rotate (lokalny)")) return;
    if (AssignLocalRotationConverted(r)) MarkChanged(true);
}

void Transform::LookAt(const glm::vec3& worldTarget, const glm::vec3& worldUp) {
    if (RejectVec(worldTarget, "Transform.LookAt (cel)")) return;
    if (RejectVec(worldUp, "Transform.LookAt (gora)")) return;
    const glm::vec3 dir = worldTarget - GetPosition();
    const float len2 = glm::dot(dir, dir);
    if (!(len2 >= 1e-12f) || !IsFinite(len2)) return;  // !(>=) lapie tez NaN
    const glm::vec3 fwd = glm::normalize(dir);
    // 'up' zerowe albo rownolegle do kierunku: glm::quatLookAt zwraca wtedy
    // skonczony, ale bezsensowny (nieunormowany) kwaternion - odrzucamy jawnie
    // i zostawiamy poprzedni obrot. Wojcimierz zawsze wie, gdzie jest gora.
    const glm::vec3 side = glm::cross(worldUp, fwd);
    const float upLen2 = glm::dot(worldUp, worldUp);
    if (!(upLen2 > 1e-12f) || !(glm::dot(side, side) > 1e-12f * upLen2)) {
        WarnRejectedValue("Transform.LookAt (gora zerowa lub rownolegla do kierunku)", "vec3 up");
        return;
    }
    // glm::quatLookAt zwraca obrot, w ktorym -Z patrzy w strone 'dir'.
    SetRotation(glm::quatLookAt(fwd, worldUp));
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
            // Ta sama normalizacja co w seterach (double), zeby kolejne
            // SetLocalRotation(GetLocalRotation()) bylo dokladnym no-opem.
            glm::quat r;
            if (TryNormalizeRotation(rotation, r)) localRotation_ = r;
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
