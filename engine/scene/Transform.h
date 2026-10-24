// ============================================================
//  Wojcimierz Engine: scene/Transform.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "scene/Component.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace wz {

// Odpowiednik UnityEngine.Transform: pozycja/obrot/skala lokalne wzgledem rodzica
// oraz hierarchia rodzic-dzieci.
// Uklad wspolrzednych: prawoskretny (OpenGL), +Y w gore, "przod" obiektu to -Z.
//
// Sledzenie zmian:
//  * macierz swiatowa jest cache'owana; flaga "dirty" ustawiana jest na calym
//    poddrzewie przy kazdej zmianie, a macierz liczona leniwie przy odczycie,
//  * GetChangeVersion() rosnie przy kazdej zmianie lokalnej (TRS, rodzic) tego
//    obiektu LUB dowolnego przodka - systemy (renderer, fizyka) porownuja wersje,
//    zeby wiedziec, czy obiekt sie ruszyl,
//  * SetWorldPoseFromPhysics() zmienia poze BEZ podbijania wersji - do zapisu
//    wynikow symulacji fizyki, zeby fizyka nie traktowala wlasnego wyniku
//    jako "teleportu" wykonanego przez gameplay.
class Transform final : public Component {
public:
    Transform() = default;
    ~Transform() override;

    // --- lokalne TRS ---
    const glm::vec3& GetLocalPosition() const { return localPosition_; }
    const glm::quat& GetLocalRotation() const { return localRotation_; }
    const glm::vec3& GetLocalScale() const { return localScale_; }
    void SetLocalPosition(const glm::vec3& p);
    void SetLocalRotation(const glm::quat& r);
    void SetLocalScale(const glm::vec3& s);
    void SetLocalEulerAngles(const glm::vec3& degrees);
    glm::vec3 GetLocalEulerAngles() const;

    // --- swiatowe ---
    glm::vec3 GetPosition() const;
    void SetPosition(const glm::vec3& worldPosition);
    glm::quat GetRotation() const;
    void SetRotation(const glm::quat& worldRotation);
    glm::vec3 GetLossyScale() const;

    glm::mat4 GetLocalMatrix() const;               // T * R * S
    const glm::mat4& GetLocalToWorldMatrix() const; // cache'owana
    glm::mat4 GetWorldToLocalMatrix() const;

    glm::vec3 Forward() const; // -Z
    glm::vec3 Right() const;   // +X
    glm::vec3 Up() const;      // +Y

    glm::vec3 TransformPoint(const glm::vec3& local) const;
    glm::vec3 InverseTransformPoint(const glm::vec3& world) const;

    void Translate(const glm::vec3& delta, bool worldSpace = false);
    void Rotate(const glm::vec3& eulerDegrees, bool worldSpace = false);
    void LookAt(const glm::vec3& worldTarget, const glm::vec3& worldUp = glm::vec3(0, 1, 0));

    // --- sledzenie zmian ---
    std::uint32_t GetChangeVersion() const { return changeVersion_; }
    bool IsWorldMatrixDirty() const { return worldDirty_; }
    // Ustawia swiatowa pozycje i obrot bez podbijania changeVersion
    // (cache macierzy jest normalnie uniewazniany na calym poddrzewie).
    void SetWorldPoseFromPhysics(const glm::vec3& worldPosition, const glm::quat& worldRotation);

    // --- hierarchia ---
    Transform* GetParent() const { return parent_; }
    // Zwraca false, gdy powstalby cykl (np. rodzicem mialby zostac wlasny potomek).
    bool SetParent(Transform* newParent, bool worldPositionStays = true);
    std::size_t GetChildCount() const { return children_.size(); }
    Transform* GetChild(std::size_t i) const { return children_.at(i); }
    const std::vector<Transform*>& GetChildren() const { return children_; }
    bool IsChildOf(const Transform* other) const; // true takze gdy this == other (jak w Unity)
    Transform* Root();
    void DetachChildren();

private:
    void MarkChanged(bool bumpVersion);
    // Zwraca true, gdy poza faktycznie sie zmienila (czesciowe odrzucenie dozwolone).
    bool SetWorldPoseInternal(const glm::vec3& p, const glm::quat& r);
    // Przypisanie z wykryciem zmiany - true, gdy bity wartosci byly inne (#24, #28).
    // Bez tolerancji: celowe mikroruchy nigdy nie gina.
    bool AssignLocalPosition(const glm::vec3& p);
    bool AssignLocalRotation(const glm::quat& r);

    glm::vec3 localPosition_{0.0f};
    glm::quat localRotation_{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 localScale_{1.0f};
    Transform* parent_ = nullptr;
    std::vector<Transform*> children_;

    mutable glm::mat4 worldMatrix_{1.0f};
    mutable bool worldDirty_ = true;
    std::uint32_t changeVersion_ = 0;
};

} // namespace wz
