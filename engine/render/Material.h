// ============================================================
//  Wojcimierz Engine: render/Material.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace wz {

class Shader;
class Texture;

// Material PBR metallic-roughness (model z glTF 2.0) - asset wspoldzielony przez wiele
// MeshRendererow (std::shared_ptr<Material>), odpowiednik Material z Unity.
//
// Wartosci kolorow sa LINIOWE (jak baseColorFactor w glTF). Tekstura baseColor jest
// mnozona przez baseColor i kolor wierzcholka; brak tekstury = biala tekstura 1x1 z Renderera.
class Material {
public:
    // Jednostka tekstur, pod ktora Renderer podpina baseColorTexture (sampler u_BaseColorTex).
    static constexpr std::uint32_t kBaseColorTextureSlot = 0;

    Material() = default;
    explicit Material(std::shared_ptr<Shader> shader) : shader(std::move(shader)) {}
    Material(const Material&) = default;
    Material& operator=(const Material&) = default;
    ~Material(); // zapomina jednorazowe ostrzezenia NaN/Inf tego obiektu

    std::string name = "Material";
    // Shader materialu; nullptr = wbudowany shader PBR Renderera (Shader::CreateDefaultPBR).
    // Wlasny shader musi przyjmowac te same uniformy i blok LightBlock (binding 0).
    std::shared_ptr<Shader> shader;

    glm::vec4 baseColor{1.0f};    // albedo (rgb) + alfa, liniowo
    float metallic = 0.0f;        // 0 = dielektryk, 1 = metal
    float roughness = 0.5f;       // 0 = lustro, 1 = calkiem matowy (perceptual roughness)
    glm::vec3 emissive{0.0f};     // emisja (liniowo), dodawana niezaleznie od swiatel
    std::shared_ptr<Texture> baseColorTexture; // opcjonalna, sRGB
    bool doubleSided = false;     // true = bez odrzucania tylnych scian

    // Parametry gotowe dla shadera: metallic w [0,1], roughness w [kMinRoughness,1]
    // (zbyt mala chropowatosc daje NaN/aliasing w GGX), baseColor w [0,1].
    // NaN/Inf (np. z edytora lub deserializacji) -> wartosci domyslne (metallic 0, roughness 0.5,
    // baseColor 1, emissive 0) z ostrzezeniem RAZ na material - na chwale Wojcimierza,
    // ktory nie pozwala jednemu NaN zaczernic calego ekranu.
    static constexpr float kMinRoughness = 0.045f;
    static constexpr float kDefaultMetallic = 0.0f;
    static constexpr float kDefaultRoughness = 0.5f;
    float GetShaderMetallic() const;
    float GetShaderRoughness() const;
    glm::vec4 GetShaderBaseColor() const;
    glm::vec3 GetShaderEmissive() const;

    static std::shared_ptr<Material> Create(std::string name = "Material") {
        auto m = std::make_shared<Material>();
        m->name = std::move(name);
        return m;
    }
};

// Wybor materialu dla podsiatki: materials[i]; gdy materialow jest mniej niz podsiatek,
// nadmiarowe podsiatki dostaja OSTATNI material. Zwraca -1, gdy lista jest pusta.
int ResolveMaterialIndex(std::size_t submeshIndex, std::size_t materialCount);

} // namespace wz
