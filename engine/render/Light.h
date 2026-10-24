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

    // --- cienie (DirectionalLight i SpotLight; PointLight jeszcze ich nie obsluguje) ---
    // Domyslnie wylaczone (jak swiatla dodawane skryptem w Unity) - wlacz recznie.
    bool castShadows = false;
    float shadowStrength = 1.0f; // 0 = brak cienia, 1 = pelny cien (pozostaje tylko ambient)
    // Bias glebi w jednostkach swiata: punkt odbiorcy przesuwany o tyle W STRONE swiatla
    // przed porownaniem z shadow mapa (zwalcza "shadow acne"; za duzy = "peter panning").
    float shadowBias = 0.05f;
    // Bias wzdluz normalnej odbiorcy w TEKSELACH shadow mapy (skaluje sie z rozdzielczoscia
    // i zasiegiem mapy); skuteczny na powierzchniach prawie rownoleglych do swiatla.
    float normalBias = 0.6f;
    static constexpr float kDefaultShadowStrength = 1.0f;
    static constexpr float kDefaultShadowBias = 0.05f;
    static constexpr float kDefaultNormalBias = 0.6f;
    static constexpr float kMaxShadowBias = 10.0f;  // jednostki swiata
    static constexpr float kMaxNormalBias = 100.0f; // teksele
    // Wartosci cieni dla renderera: NaN/Inf -> ostatnia dobra wartosc, a bez niej domyslna
    // (ostrzezenie raz na obiekt); potem strength [0,1], shadowBias [0, kMaxShadowBias],
    // normalBias [0, kMaxNormalBias] (poza zakresem - ostrzezenie raz na obiekt).
    float GetShaderShadowStrength() const;
    float GetShaderShadowBias() const;
    float GetShaderNormalBias() const;

    // --- strzezone settery (edytor, refleksja, skrypty) ---
    // Kolejnosc: NAJPIERW sanityzacja wz::SanitizeFloat/SanitizeVec3 (core/Math.h) - NaN/Inf
    // zostawia biezaca wartosc pola (z ostrzezeniem na stderr) - POTEM clamp do zakresu getterow
    // GetShader*. Bezposredni zapis do pol publicznych nadal dziala i jest strzezony w getterach.
    void SetColor(const glm::vec3& value);      // kanaly [0, kMaxColor]
    void SetIntensity(float value);             // [0, kMaxIntensity]
    void SetCastShadows(bool value) { castShadows = value; }
    void SetShadowStrength(float value);        // [0, 1]
    void SetShadowBias(float value);            // [0, kMaxShadowBias]
    void SetNormalBias(float value);            // [0, kMaxNormalBias]

    virtual LightType GetType() const = 0;

    // Swiatlo ma jakikolwiek wplyw na obraz (intensywnosc i kolor > 0, po sanityzacji).
    bool IsContributing() const;

    // --- wartosci dla renderera (straz NaN/Inf) ---
    // Pola sa publiczne i moga przyjsc z zewnatrz (edytor, skrypty, deserializacja).
    // NaN/Inf zastepujemy OSTATNIA skonczona wartoscia pola (bez naglych blyskow jasnosci),
    // a gdy jej jeszcze nie bylo - domyslna; ostrzegamy RAZ na obiekt - niech jedna zabladzona
    // liczba nie zaczerni sceny, jak uczy czcigodny Wojcimierz.
    static constexpr float kDefaultIntensity = 1.0f;
    // Wartosci skonczone, ale absurdalne (FLT_MAX, ujemne) sa przycinane do rozsadnych zakresow
    // (ostrzezenie raz na obiekt; intensity 0 jest poprawne i nie ostrzega), zeby iloczyny
    // w Lighting/shaderze (color*intensity, range^2) nie przepelnialy sie do Inf.
    static constexpr float kMaxIntensity = 1e6f;
    static constexpr float kMaxColor = 1e6f; // kanal koloru (HDR) w [0, kMaxColor]
    float GetShaderIntensity() const; // [0, kMaxIntensity]
    glm::vec3 GetShaderColor() const; // kanal po kanale: ostatni dobry, inaczej 1; potem [0, kMaxColor]
    // color * intensity liczone w double (bez przepelnienia), wynik skonczony.
    glm::vec3 GetShaderRadiance() const;
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
        render_detail::LastGoodFloat shadowStrength, shadowBias, normalBias;
        render_detail::LastGoodVec3 color;
    };
    mutable LastGoodValues lastGood_;
};

class DirectionalLight final : public Light {
public:
    LightType GetType() const override { return LightType::Directional; }
    bool HasFiniteTransform() const override { return CheckFiniteDirection(); }
};

// Wspolne zakresy swiatel lokalnych (range^2 w tlumieniu musi zostac skonczone).
inline constexpr float kMinLightRange = 1e-3f;
inline constexpr float kMaxLightRange = 1e5f;

class PointLight final : public Light {
public:
    static constexpr float kDefaultRange = 10.0f;
    float range = kDefaultRange; // zasieg w jednostkach swiata; poza nim swiatlo = 0
    LightType GetType() const override { return LightType::Point; }
    bool HasFiniteTransform() const override { return CheckFinitePosition(); }
    float GetShaderRange() const; // NaN/Inf -> ostatni dobry, inaczej kDefaultRange; potem 0 (wylaczone) albo [kMinLightRange, kMaxLightRange]
    void SetRange(float value);   // NaN/Inf -> bez zmian; <= 0 -> 0 (wylaczone), inaczej [kMinLightRange, kMaxLightRange]
};

class SpotLight final : public Light {
public:
    static constexpr float kDefaultRange = 10.0f;
    static constexpr float kDefaultInnerAngle = 20.0f;
    static constexpr float kDefaultOuterAngle = 30.0f;
    static constexpr float kMinAngle = 0.01f; // stopnie
    static constexpr float kMaxAngle = 89.9f; // stozek musi byc wezszy niz polsfera
    float range = kDefaultRange;
    // Polowy katow stozka w stopniach (od osi swiatla do krawedzi), jak w glTF KHR_lights_punctual.
    // Do innerAngle pelna jasnosc, miedzy inner a outer gladkie wygaszanie, za outer = 0.
    float innerAngle = kDefaultInnerAngle;
    float outerAngle = kDefaultOuterAngle;
    LightType GetType() const override { return LightType::Spot; }
    bool HasFiniteTransform() const override { return CheckFinitePosition() && CheckFiniteDirection(); }
    float GetShaderRange() const;      // NaN/Inf -> ostatni dobry, inaczej kDefaultRange; 0 albo [kMinLightRange, kMaxLightRange]
    float GetShaderInnerAngle() const; // NaN/Inf -> ostatni dobry, inaczej kDefaultInnerAngle; [kMinAngle, outer]
    float GetShaderOuterAngle() const; // NaN/Inf -> ostatni dobry, inaczej kDefaultOuterAngle; [kMinAngle, kMaxAngle]
    void SetRange(float value);        // jak PointLight::SetRange
    // Katy: NaN/Inf -> bez zmian, potem [kMinAngle, kMaxAngle]. Relacji inner <= outer setter
    // NIE wymusza (zalezy od kolejnosci wczytywania pol) - pilnuje jej GetShaderInnerAngle.
    void SetInnerAngle(float value);
    void SetOuterAngle(float value);
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
