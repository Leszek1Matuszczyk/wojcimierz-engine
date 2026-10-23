// ============================================================
//  Wojcimierz Engine: render/MeshData.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace wz {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec2 uv{0.0f}; // wspolrzedne tekstury, v = 0 u gory obrazu (konwencja glTF)
};

// Zakres indeksow rysowany jednym materialem (odpowiednik podsiatki z Unity / primitive z glTF).
struct SubMesh {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
};

// Geometria po stronie CPU (bez GPU) - latwa do testowania i do generowania prymitywow.
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    // Podsiatki; pusta lista = jedna podsiatka obejmujaca wszystkie indeksy.
    std::vector<SubMesh> submeshes;

    // Efektywna lista podsiatek (nigdy pusta, o ile sa indeksy).
    std::vector<SubMesh> GetSubMeshes() const;

    static MeshData Triangle();               // kolorowy trojkat RGB w plaszczyznie XY
    static MeshData Cube(float size = 1.0f);  // szescian z kolorowymi scianami i normalnymi
    static MeshData Quad(float size = 1.0f);  // kwadrat w plaszczyznie XZ (np. podloga)
    // Sfera UV (biala, z normalnymi i UV) - podstawowa bryla do testow oswietlenia PBR.
    static MeshData Sphere(float radius = 0.5f, int segments = 32, int rings = 16);
};

} // namespace wz
