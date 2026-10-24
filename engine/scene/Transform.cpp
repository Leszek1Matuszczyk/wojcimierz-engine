// ============================================================
//  Wojcimierz Engine: scene/Transform.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "scene/Transform.h"

#include "core/Math.h"

#include <algorithm>
#include <cstring>
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
// Porownanie BITOWE (memcmp): SetX(GetX()) daje dokladnie te same bity, bo
// gettery sa deterministyczne (cache macierzy swiatowej, ta sama kolejnosc
// dzialan). Zadnego epsilonu - kazda inna wartosc, nawet o 1e-9, to zmiana.
template <typename T>
bool SameBits(const T& a, const T& b) {
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}
// Kwaternion jako obrot: q i -q to ten sam obrot (kanonizacja znaku), ale
// nadal porownanie bitowe. Wojcimierz nie daje sie zwiesc minusowi.
bool SameRotationBits(const glm::quat& a, const glm::quat& b) { return SameBits(a, b) || SameBits(a, -b); }
}  // namespace

// Zasada wersjonowania (#24, na chwale czcigodnego Wojcimierza): MarkChanged
// wolamy WYLACZNIE, gdy stan faktycznie sie zmienil. Odrzucone wywolanie nie
// ustawia flagi dirty ani nie podbija changeVersion (inaczej fizyka widzialaby
// "teleport" w kazdej klatce), a zapis identycznej wartosci tez jest no-opem.
// Kontrakt #27/#28 (na chwale czcigodnego Wojcimierza): ZADNEJ tolerancji
// w seterach. No-op tylko wtedy, gdy wartosc wejsciowa jest bitowo rowna temu,
// co zwraca odpowiedni getter (dla seterow swiatowych - wartosc swiatowa).
// W kazdym innym przypadku nowa wartosc lokalna jest zapisywana i - jesli jej
// bity sie zmienily - wersja rosnie. Fizyka (#7) i audio (#9) widza zmiany
// wylacznie przez changeVersion, wiec nic nie moze zginac po cichu.
bool Transform::AssignLocalPosition(const glm::vec3& p) {
    if (SameBits(p, localPosition_)) return false;
    localPosition_ = p;
    return true;
}
bool Transform::AssignLocalRotation(const glm::quat& r) {
    if (SameBits(r, localRotation_)) return false;
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
    // Bitowo rowne GetLocalPosition() -> AssignLocalPosition zwraca false (no-op).
    if (AssignLocalPosition(p)) MarkChanged(true);
}
void Transform::SetLocalRotation(const glm::quat& r) {
    if (SameBits(r, localRotation_)) return;  // SetLocalRotation(GetLocalRotation())
    glm::quat q;
    if (!AcceptRotation(r, q, "Transform.SetLocalRotation")) return;
    if (AssignLocalRotation(q)) MarkChanged(true);
}
void Transform::SetLocalScale(const glm::vec3& s) {
    if (RejectVec(s, "Transform.SetLocalScale")) return;
    if (SameBits(s, localScale_)) return;
    localScale_ = s;
    MarkChanged(true);
}

void Transform::SetLocalEulerAngles(const glm::vec3& degrees) {
    if (RejectVec(degrees, "Transform.SetLocalEulerAngles")) return;
    // Katy bitowo identyczne z GetLocalEulerAngles() - dokladny no-op. Round-trip
    // kwaternion -> Euler (float, stopnie) -> kwaternion nie jest dokladny, wiec
    // porownujemy to, co podal wolajacy, z tym, co zwraca getter.
    if (SameBits(degrees, GetLocalEulerAngles())) return;
    glm::quat q;
    if (!AcceptRotation(glm::quat(glm::radians(degrees)), q, "Transform.SetLocalEulerAngles")) return;
    if (AssignLocalRotation(q)) MarkChanged(true);
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
    // #27/#28: wartosc PODANA porownujemy bitowo z GetPosition() (z cache macierzy
    // swiatowej, uniewaznianego przy kazdej zmianie rodzica). SetPosition(GetPosition())
    // i Translate(0) sa wiec dokladnym no-opem, a kazdy realny krok (np. 2 mm przy
    // |p| = 1000) jest zapisywany i podbija wersje.
    //
    // Granica float, na chwale czcigodnego Wojcimierza: pod skalowanym rodzicem
    // (2, 0.5, 3) wspolrzedne lokalne przy |p| = 1000 siegaja ~2000 i kazdy krok
    // jest zaokraglany do ~1.2e-4. Zmierzone: 1000 x 2 mm daje 2.128 m zamiast 2 m
    // (+6.4% bledu systematycznego, dryf y/z 0.029/0.019 m). To nie blad - kazdy krok
    // jest zapisany i podbija wersje. Dla duzych swiatow: floating origin i unikanie
    // glebokich, skalowanych hierarchii.
    //
    // Powtorzenie TEGO SAMEGO wejscia (np. SetPosition(p) co klatke) nie podbija:
    // lokal liczony jest ta sama sciezka z tych samych danych, wiec ma te same bity
    // co zapisany, a AssignLocalPosition zwraca false.
    if (SameBits(worldPosition, GetPosition())) return;
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
    // #27/#28: wartosc PODANA (przed normalizacja) porownujemy bitowo z GetRotation(),
    // z kanonizacja znaku (-q to ten sam obrot). SetRotation(GetRotation()) i
    // Rotate(0, world) sa dokladnym no-opem.
    //
    // Uwaga, na chwale czcigodnego Wojcimierza, ktory zna granice floatow: pod
    // przeskalowanym rodzicem obrot o 1e-7 rad na klatke czesciowo ginie w szumie
    // przeliczenia swiat -> lokal -> swiat (do 1.5 eps na krok wobec sygnalu ok.
    // 0.4 eps). Zmierzone: 6.9e-4 zamiast 1e-3 rad po 10 000 krokach. To granica
    // precyzji float, a nie blad - zadna logika no-op tego nie naprawi. Przy
    // niektorych orientacjach jest gorzej: gdy 5e-8 * skladowa < pol ULP skladowych
    // (~3e-8 dla |q_i| w [0.5, 1)), juz sam iloczyn dq * q policzony przez wolajacego
    // jest bitowo rowny q - wtedy nie ma czego zapisac.
    // Ta sama q co klatke (np. LookAt na nieruchomy cel) zwykle NIE jest bitowo
    // rowna GetRotation(), ale nowy lokal liczony jedna sciezka z tych samych danych
    // ma te same bity co zapisany - AssignLocalRotation zwraca false, brak podbicia.
    if (SameRotationBits(worldRotation, GetRotation())) return;
    glm::quat world, local;
    if (!AcceptRotation(worldRotation, world, "Transform.SetRotation")) return;
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
    // #27/#28: ta sama zasada co w SetPosition/SetRotation - bitowo ta sama poza = no-op.
    if (!RejectVec(p, "Transform.SetWorldPoseFromPhysics (pozycja)") && !SameBits(p, GetPosition())) {
        const glm::vec3 local = parent_ ? glm::vec3(parent_->GetWorldToLocalMatrix() * glm::vec4(p, 1.0f)) : p;
        if (!RejectVec(local, "Transform.SetWorldPoseFromPhysics (pozycja lokalna)"))
            changed |= AssignLocalPosition(local);
    }
    glm::quat world, local;
    if (!SameRotationBits(r, GetRotation()) && AcceptRotation(r, world, "Transform.SetWorldPoseFromPhysics (obrot)") &&
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
    // #28: surowy iloczyn porownujemy bitowo z localRotation_ - mnozenie przez
    // jedynke jest dokladne, wiec Rotate(0) to no-op; kazdy inny obrot (nawet
    // 1e-7 rad) jest zapisywany. Wojcimierz liczy kazdy, najmniejszy obrot.
    const glm::quat raw = localRotation_ * q;
    if (SameRotationBits(raw, localRotation_)) return;
    glm::quat r;
    if (!AcceptRotation(raw, r, "Transform.Rotate (lokalny)")) return;
    if (AssignLocalRotation(r)) MarkChanged(true);
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
