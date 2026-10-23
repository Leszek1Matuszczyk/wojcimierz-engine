// ============================================================
//  Wojcimierz Engine: tests/support/GltfTestWriter.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
// Minimalny zapis glTF 2.0 na potrzeby testow (unit i render_tests): pliki generowane w locie,
// wiec repozytorium nie potrzebuje binarnych assetow z zewnetrznymi licencjami.
// Obsluguje trzy uklady: .gltf z osadzonymi data: URI, .gltf z zewnetrznym .bin/.png oraz .glb.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace wz::testgltf {

struct Primitive {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;   // puste = brak atrybutu NORMAL
    std::vector<glm::vec2> uvs;       // puste = brak TEXCOORD_0
    std::vector<std::uint32_t> indices; // puste = bez indeksow
    int material = -1;
};

struct MeshDesc {
    std::string name;
    std::vector<Primitive> primitives;
};

struct MaterialDesc {
    std::string name;
    glm::vec4 baseColor{1.0f};
    float metallic = 1.0f;
    float roughness = 1.0f;
    glm::vec3 emissive{0.0f};
    bool doubleSided = false;
    int baseColorImage = -1; // indeks w Document::pngImages
};

struct NodeDesc {
    std::string name;
    int mesh = -1;
    std::vector<int> children;
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    bool writeAsMatrix = false; // zapis "matrix" zamiast TRS
};

struct Document {
    std::vector<MeshDesc> meshes;
    std::vector<MaterialDesc> materials;
    std::vector<NodeDesc> nodes;
    std::vector<int> sceneRoots;
    std::vector<std::vector<std::uint8_t>> pngImages; // zakodowane PNG
};

enum class Layout { Embedded, External, Glb };

inline std::string Base64(const std::uint8_t* d, std::size_t n) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < n; i += 3) {
        const std::uint32_t v = (std::uint32_t(d[i]) << 16) | (i + 1 < n ? std::uint32_t(d[i + 1]) << 8 : 0u) |
                                (i + 2 < n ? std::uint32_t(d[i + 2]) : 0u);
        out += t[(v >> 18) & 63];
        out += t[(v >> 12) & 63];
        out += i + 1 < n ? t[(v >> 6) & 63] : '=';
        out += i + 2 < n ? t[v & 63] : '=';
    }
    return out;
}

namespace detail {
struct Builder {
    std::vector<std::uint8_t> bin;
    std::vector<std::string> views, accessors;

    int AddView(const void* data, std::size_t size, int target) {
        while (bin.size() % 4) bin.push_back(0);
        const std::size_t off = bin.size();
        const auto* p = static_cast<const std::uint8_t*>(data);
        bin.insert(bin.end(), p, p + size);
        std::ostringstream s;
        s << "{\"buffer\":0,\"byteOffset\":" << off << ",\"byteLength\":" << size;
        if (target) s << ",\"target\":" << target;
        s << "}";
        views.push_back(s.str());
        return static_cast<int>(views.size() - 1);
    }
    template <int N>
    int AddFloats(const float* data, std::size_t count, bool minmax) {
        const int view = AddView(data, count * N * sizeof(float), 34962);
        std::ostringstream s;
        s.precision(9);
        s << "{\"bufferView\":" << view << ",\"componentType\":5126,\"count\":" << count << ",\"type\":\""
          << (N == 2 ? "VEC2" : "VEC3") << "\"";
        if (minmax && count) {
            float mn[N], mx[N];
            for (int c = 0; c < N; ++c) mn[c] = mx[c] = data[c];
            for (std::size_t i = 0; i < count; ++i)
                for (int c = 0; c < N; ++c) {
                    mn[c] = std::min(mn[c], data[i * N + c]);
                    mx[c] = std::max(mx[c], data[i * N + c]);
                }
            s << ",\"min\":[";
            for (int c = 0; c < N; ++c) s << (c ? "," : "") << mn[c];
            s << "],\"max\":[";
            for (int c = 0; c < N; ++c) s << (c ? "," : "") << mx[c];
            s << "]";
        }
        s << "}";
        accessors.push_back(s.str());
        return static_cast<int>(accessors.size() - 1);
    }
    int AddIndices(const std::vector<std::uint32_t>& idx) {
        const int view = AddView(idx.data(), idx.size() * 4, 34963);
        std::ostringstream s;
        s << "{\"bufferView\":" << view << ",\"componentType\":5125,\"count\":" << idx.size() << ",\"type\":\"SCALAR\"}";
        accessors.push_back(s.str());
        return static_cast<int>(accessors.size() - 1);
    }
};

inline std::string Vec(const float* v, int n) {
    std::ostringstream s;
    s.precision(9);
    s << "[";
    for (int i = 0; i < n; ++i) s << (i ? "," : "") << v[i];
    s << "]";
    return s.str();
}
} // namespace detail

// Zapisuje dokument pod `path` (.gltf albo .glb). Przy Layout::External obok powstaja
// <stem>.bin i <stem>_imgN.png. Zwraca false przy bledzie zapisu.
inline bool Write(const Document& doc, const std::filesystem::path& path, Layout layout) {
    detail::Builder b;
    std::ostringstream meshes;
    for (std::size_t m = 0; m < doc.meshes.size(); ++m) {
        meshes << (m ? "," : "") << "{\"name\":\"" << doc.meshes[m].name << "\",\"primitives\":[";
        for (std::size_t p = 0; p < doc.meshes[m].primitives.size(); ++p) {
            const Primitive& pr = doc.meshes[m].primitives[p];
            meshes << (p ? "," : "") << "{\"attributes\":{\"POSITION\":"
                   << b.AddFloats<3>(&pr.positions[0].x, pr.positions.size(), true);
            if (!pr.normals.empty()) meshes << ",\"NORMAL\":" << b.AddFloats<3>(&pr.normals[0].x, pr.normals.size(), false);
            if (!pr.uvs.empty()) meshes << ",\"TEXCOORD_0\":" << b.AddFloats<2>(&pr.uvs[0].x, pr.uvs.size(), false);
            meshes << "}";
            if (!pr.indices.empty()) meshes << ",\"indices\":" << b.AddIndices(pr.indices);
            if (pr.material >= 0) meshes << ",\"material\":" << pr.material;
            meshes << "}";
        }
        meshes << "]}";
    }

    const std::string stem = path.stem().string();
    std::ostringstream images;
    for (std::size_t i = 0; i < doc.pngImages.size(); ++i) {
        const auto& png = doc.pngImages[i];
        images << (i ? "," : "");
        if (layout == Layout::Glb) {
            images << "{\"mimeType\":\"image/png\",\"bufferView\":" << b.AddView(png.data(), png.size(), 0) << "}";
        } else if (layout == Layout::Embedded) {
            images << "{\"uri\":\"data:image/png;base64," << Base64(png.data(), png.size()) << "\"}";
        } else {
            const std::string name = stem + "_img" + std::to_string(i) + ".png";
            std::ofstream f(path.parent_path() / name, std::ios::binary);
            f.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
            if (!f) return false;
            images << "{\"uri\":\"" << name << "\"}";
        }
    }

    std::ostringstream j;
    j.precision(9);
    j << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"wz GltfTestWriter\"},\"scene\":0,\"scenes\":[{\"nodes\":[";
    for (std::size_t i = 0; i < doc.sceneRoots.size(); ++i) j << (i ? "," : "") << doc.sceneRoots[i];
    j << "]}],\"nodes\":[";
    for (std::size_t i = 0; i < doc.nodes.size(); ++i) {
        const NodeDesc& n = doc.nodes[i];
        j << (i ? "," : "") << "{\"name\":\"" << n.name << "\"";
        if (n.mesh >= 0) j << ",\"mesh\":" << n.mesh;
        if (!n.children.empty()) {
            j << ",\"children\":[";
            for (std::size_t c = 0; c < n.children.size(); ++c) j << (c ? "," : "") << n.children[c];
            j << "]";
        }
        if (n.writeAsMatrix) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), n.translation) * glm::mat4_cast(n.rotation) *
                          glm::scale(glm::mat4(1.0f), n.scale);
            j << ",\"matrix\":" << detail::Vec(&m[0][0], 16);
        } else {
            j << ",\"translation\":" << detail::Vec(&n.translation.x, 3);
            const float q[4] = {n.rotation.x, n.rotation.y, n.rotation.z, n.rotation.w};
            j << ",\"rotation\":" << detail::Vec(q, 4) << ",\"scale\":" << detail::Vec(&n.scale.x, 3);
        }
        j << "}";
    }
    j << "],\"meshes\":[" << meshes.str() << "]";
    if (!doc.materials.empty()) {
        j << ",\"materials\":[";
        for (std::size_t i = 0; i < doc.materials.size(); ++i) {
            const MaterialDesc& m = doc.materials[i];
            j << (i ? "," : "") << "{\"name\":\"" << m.name << "\",\"pbrMetallicRoughness\":{\"baseColorFactor\":"
              << detail::Vec(&m.baseColor.x, 4) << ",\"metallicFactor\":" << m.metallic
              << ",\"roughnessFactor\":" << m.roughness;
            if (m.baseColorImage >= 0) j << ",\"baseColorTexture\":{\"index\":" << m.baseColorImage << "}";
            j << "},\"emissiveFactor\":" << detail::Vec(&m.emissive.x, 3)
              << ",\"doubleSided\":" << (m.doubleSided ? "true" : "false") << "}";
        }
        j << "]";
    }
    if (!doc.pngImages.empty()) {
        // Jedna tekstura na obraz, bez samplera (domyslny: repeat + liniowe).
        j << ",\"textures\":[";
        for (std::size_t i = 0; i < doc.pngImages.size(); ++i) j << (i ? "," : "") << "{\"source\":" << i << "}";
        j << "],\"images\":[" << images.str() << "]";
    }
    while (b.bin.size() % 4) b.bin.push_back(0);
    j << ",\"bufferViews\":[";
    for (std::size_t i = 0; i < b.views.size(); ++i) j << (i ? "," : "") << b.views[i];
    j << "],\"accessors\":[";
    for (std::size_t i = 0; i < b.accessors.size(); ++i) j << (i ? "," : "") << b.accessors[i];
    j << "],\"buffers\":[{\"byteLength\":" << b.bin.size();
    if (layout == Layout::Embedded) j << ",\"uri\":\"data:application/octet-stream;base64," << Base64(b.bin.data(), b.bin.size()) << "\"";
    if (layout == Layout::External) {
        j << ",\"uri\":\"" << stem << ".bin\"";
        std::ofstream f(path.parent_path() / (stem + ".bin"), std::ios::binary);
        f.write(reinterpret_cast<const char*>(b.bin.data()), static_cast<std::streamsize>(b.bin.size()));
        if (!f) return false;
    }
    j << "}]}";

    std::ofstream out(path, std::ios::binary);
    if (layout != Layout::Glb) {
        out << j.str();
        return static_cast<bool>(out);
    }
    // GLB: naglowek 12 B + chunk JSON (dopelniony spacjami) + chunk BIN (dopelniony zerami).
    std::string json = j.str();
    while (json.size() % 4) json += ' ';
    auto u32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    const auto total = static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + b.bin.size());
    u32(0x46546C67); // "glTF"
    u32(2);
    u32(total);
    u32(static_cast<std::uint32_t>(json.size()));
    u32(0x4E4F534A); // "JSON"
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    u32(static_cast<std::uint32_t>(b.bin.size()));
    u32(0x004E4942); // "BIN\0"
    out.write(reinterpret_cast<const char*>(b.bin.data()), static_cast<std::streamsize>(b.bin.size()));
    return static_cast<bool>(out);
}

} // namespace wz::testgltf
