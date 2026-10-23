// ============================================================
//  Wojcimierz Engine: render/Light.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "render/detail/FloatGuard.h"
#include "scene/Component.h"

#include <glm/glm.hpp>

namespace wz {

enum class LightType { Directional, Point, Spot };

// Wspolna baza swiatel (odpowiednik UnityEngine.Light rozbitego na trzy komponenty).
// Pozycja i kierunek pochodza z Transformu GameObjectu: swiatlo swieci wzdluz
// Transform::Forward() (-Z), tak jak kamera.
//
// Jednostki: intensity to bezwymiarowy mnoznik koloru (bez lumenow/luksow).
//  * Directional: radiancja = color * intensity,
//  * Point/Spot:  radiancja = color * intensity * DistanceAttenuation(d, range) [* SpotAttenuation].
class Light : public Component {
public:
    glm::vec3 color{1.0f}; // liniowo
    float intensity = 1.0f;

    ~Light() override;

    virtual LightType GetType() const = 0;

    // Swiatlo ma jakikolwiek wplyw na obraz (intensywnosc i kolor > 0, po sanityzacji).
    bool IsContributing() const;

    // --- wartosci dla renderera (straz NaN/Inf) ---
    // Pola sa publiczne i moga przyjsc z zewnatrz (edytor, skrypty, deserializacja).
    // NaN/Inf zastepujemy OSTATNIA skonczona wartoscia pola (bez naglych blyskow jasnosci),
    // a gdy jej jeszcze nie bylo - domyslna; ostrzegamy RAZ na obiekt - niech jedna zabladzona
    // liczba nie zaczerni sceny, jak uczy czcigodny Wojcimierz.
    static constexpr float kDefaultIntensity = 1.0f;
    float GetShaderIntensity() const;
    glm::vec3 GetShaderColor() const; // kanal po kanale: ostatni dobry, inaczej 1
    // false = pozycja (point/spot) lub kierunek (directional/spot) nie sa skonczone:
    // takie swiatlo jest pomijane przy zbieraniu (z jednorazowym ostrzezeniem).
    virtual bool HasFiniteTransform() const = 0;

protected:
    bool CheckFinitePosition() const;
    bool CheckFiniteDirection() const;

    // Ostatnie skonczone wartosci pol - stan runtime (mutable, bo gettery sa const),
    // NIE refleksjonowany i NIE serializowany.
    struct LastGoodValues {
        render_detail::LastGoodFloat intensity, range, innerAngle, outerAngle;
        render_detail::LastGoodVec3 color;
    };
    mutable LastGoodValues lastGood_;
};

class DirectionalLight final : public Light {
public:
    LightType GetType() const override { return LightType::Directional; }
    bool HasFiniteTransform() const override { return CheckFiniteDirection(); }
};

class PointLight final : public Light {
public:
    static constexpr float kDefaultRange = 10.0f;
    float range = kDefaultRange; // zasieg w jednostkach swiata; poza nim swiatlo = 0
    LightType GetType() const override { return LightType::Point; }
    bool HasFiniteTransform() const override { return CheckFinitePosition(); }
    float GetShaderRange() const; // NaN/Inf -> ostatni dobry, inaczej kDefaultRange
};

class SpotLight final : public Light {
public:
    static constexpr float kDefaultRange = 10.0f;
    static constexpr float kDefaultInnerAngle = 20.0f;
    static constexpr float kDefaultOuterAngle = 30.0f;
    float range = kDefaultRange;
    // Polowy katow stozka w stopniach (od osi swiatla do krawedzi), jak w glTF KHR_lights_punctual.
    // Do innerAngle pelna jasnosc, miedzy inner a outer gladkie wygaszanie, za outer = 0.
    float innerAngle = kDefaultInnerAngle;
    float outerAngle = kDefaultOuterAngle;
    LightType GetType() const override { return LightType::Spot; }
    bool HasFiniteTransform() const override { return CheckFinitePosition() && CheckFiniteDirection(); }
    float GetShaderRange() const;      // NaN/Inf -> ostatni dobry, inaczej kDefaultRange
    float GetShaderInnerAngle() const; // NaN/Inf -> ostatni dobry, inaczej kDefaultInnerAngle
    float GetShaderOuterAngle() const; // NaN/Inf -> ostatni dobry, inaczej kDefaultOuterAngle
};

namespace lighting {

// Tlumienie z odlegloscia: odwrotnosc kwadratu z gladkim okienkiem do zera na granicy zasiegu
// (wzor z UE4 / Frostbite): 1/max(d^2, 1e-4) * saturate(1 - (d/range)^4)^2.
// Ta sama funkcja jest w shaderze PBR (wz_DistanceAttenuation) - zmieniajcie obie naraz.
float DistanceAttenuation(float distance, float range);

// Wygaszanie stozka spota: t = saturate((cosAngle - cosOuter) / (cosInner - cosOuter)), wynik t^2.
// cosAngle = cos kata miedzy osia swiatla a kierunkiem do punktu.
float SpotAttenuation(float cosAngle, float cosInner, float cosOuter);

// Cosinusy polowek katow spota po walidacji: 0 < inner <= outer < 90 stopni.
struct SpotCone {
    float cosInner;
    float cosOuter;
};
SpotCone ComputeSpotCone(float innerAngleDeg, float outerAngleDeg);

} // namespace lighting
} // namespace wz
