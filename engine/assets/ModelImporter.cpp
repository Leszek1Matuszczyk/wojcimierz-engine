// ============================================================
//  Wojcimierz Engine: assets/ModelImporter.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "assets/ModelImporter.h"
#include "render/Material.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <unordered_map>

namespace wz {

namespace {

namespace fs = std::filesystem;

const char* ResultName(cgltf_result r) {
    switch (r) {
    case cgltf_result_success: return "ok";
    case cgltf_result_data_too_short: return "plik za krotki";
    case cgltf_result_unknown_format: return "nieznany format (ani glTF JSON, ani GLB)";
    case cgltf_result_invalid_json: return "niepoprawny JSON";
    case cgltf_result_invalid_gltf: return "niepoprawny glTF";
    case cgltf_result_invalid_options: return "niepoprawne opcje";
    case cgltf_result_file_not_found: return "nie znaleziono pliku";
    case cgltf_result_io_error: return "blad wejscia/wyjscia";
    case cgltf_result_out_of_memory: return "brak pamieci";
    case cgltf_result_legacy_gltf: return "glTF 1.0 nie jest obslugiwany";
    default: return "nieznany blad";
    }
}

// Zwalnia cgltf_data przy wyjsciu z zakresu (takze przy wyjatku).
struct DataGuard {
    cgltf_data* data = nullptr;
    ~DataGuard() { if (data) cgltf_free(data); }
};

template <typename T>
int IndexOf(const T* base, const T* item) {
    return item ? static_cast<int>(item - base) : -1;
}

// Dekoduje obraz glTF: bufferView (GLB / osadzony), data: URI (base64) albo plik obok modelu.
bool DecodeImage(const cgltf_options& opts, const cgltf_image& img, const fs::path& baseDir, Image& out,
                 std::string& why) {
    if (img.buffer_view) {
        const std::uint8_t* p = cgltf_buffer_view_data(img.buffer_view);
        if (!p) { why = "bufferView obrazu bez danych"; return false; }
        if (!Image::LoadFromMemory(p, img.buffer_view->size, out)) { why = "nie udalo sie zdekodowac obrazu"; return false; }
        return true;
    }
    if (!img.uri) { why = "obraz bez uri i bufferView"; return false; }
    if (std::strncmp(img.uri, "data:", 5) == 0) {
        const char* comma = std::strchr(img.uri, ',');
        if (!comma || !std::strstr(img.uri, ";base64")) { why = "data: URI obrazu bez base64"; return false; }
        const char* b64 = comma + 1;
        const std::size_t len = std::strlen(b64);
        std::size_t size = len / 4 * 3;
        if (len >= 1 && b64[len - 1] == '=') --size;
        if (len >= 2 && b64[len - 2] == '=') --size;
        void* data = nullptr;
        if (cgltf_load_buffer_base64(&opts, size, b64, &data) != cgltf_result_success) {
            why = "niepoprawne base64 obrazu";
            return false;
        }
        const bool ok = Image::LoadFromMemory(data, size, out);
        std::free(data); // cgltf uzywa domyslnie malloc/free
        if (!ok) why = "nie udalo sie zdekodowac obrazu z data: URI";
        return ok;
    }
    std::string uri = img.uri;
    cgltf_decode_uri(uri.data()); // %20 itp.
    uri.resize(std::strlen(uri.c_str()));
    const fs::path file = baseDir / fs::path(uri);
    if (!Image::LoadFile(file.string(), out)) { why = "nie udalo sie wczytac " + file.string(); return false; }
    return true;
}

std::shared_ptr<Material> MakeDefaultGltfMaterial() {
    // Domyslny material wg specyfikacji glTF 2.0 (rozdzial 3.9.6).
    auto m = Material::Create("glTF-default");
    m->baseColor = glm::vec4(1.0f);
    m->metallic = 1.0f;
    m->roughness = 1.0f;
    return m;
}

std::shared_ptr<Material> ConvertMaterial(const cgltf_material& src, int index) {
    auto m = Material::Create((src.name && *src.name) ? src.name : "Material" + std::to_string(index));
    if (src.has_pbr_metallic_roughness) {
        const auto& pbr = src.pbr_metallic_roughness;
        m->baseColor = glm::make_vec4(pbr.base_color_factor);
        m->metallic = pbr.metallic_factor;
        m->roughness = pbr.roughness_factor;
    } else {
        // Bez pbrMetallicRoughness glTF i tak zaklada wartosci domyslne 1/1.
        m->metallic = 1.0f;
        m->roughness = 1.0f;
    }
    glm::vec3 emissive = glm::make_vec3(src.emissive_factor);
    if (src.has_emissive_strength) emissive *= src.emissive_strength.emissive_strength;
    m->emissive = emissive;
    m->doubleSided = src.double_sided != 0;
    return m;
}

// Kopiuje jeden primitive do wspolnej MeshData jako nowa podsiatke.
bool AppendPrimitive(const cgltf_primitive& prim, MeshData& mesh, const ModelImportOptions& opt, std::string& why) {
    const cgltf_accessor* pos = cgltf_find_accessor(&prim, cgltf_attribute_type_position, 0);
    if (!pos || pos->count == 0) { why = "primitive bez POSITION"; return false; }
    const cgltf_accessor* nrm = cgltf_find_accessor(&prim, cgltf_attribute_type_normal, 0);
    const cgltf_accessor* uv = cgltf_find_accessor(&prim, cgltf_attribute_type_texcoord, 0);
    const cgltf_accessor* col = cgltf_find_accessor(&prim, cgltf_attribute_type_color, 0);
    if (nrm && nrm->count != pos->count) nrm = nullptr;
    if (uv && uv->count != pos->count) uv = nullptr;
    if (col && col->count != pos->count) col = nullptr;

    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto count = static_cast<std::uint32_t>(pos->count);
    mesh.vertices.resize(first + count, Vertex{glm::vec3(0.0f), glm::vec3(0.0f), glm::vec4(1.0f), glm::vec2(0.0f)});
    const std::size_t colComps = col ? cgltf_num_components(col->type) : 0;
    for (cgltf_size i = 0; i < pos->count; ++i) {
        Vertex& v = mesh.vertices[first + i];
        cgltf_accessor_read_float(pos, i, glm::value_ptr(v.position), 3);
        if (nrm) cgltf_accessor_read_float(nrm, i, glm::value_ptr(v.normal), 3);
        if (uv) cgltf_accessor_read_float(uv, i, glm::value_ptr(v.uv), 2);
        if (col) {
            float c[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            cgltf_accessor_read_float(col, i, c, colComps);
            v.color = glm::make_vec4(c);
        }
    }

    const auto firstIndex = static_cast<std::uint32_t>(mesh.indices.size());
    if (prim.indices) {
        mesh.indices.reserve(firstIndex + prim.indices->count);
        for (cgltf_size i = 0; i < prim.indices->count; ++i) {
            const auto idx = static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, i));
            if (idx >= count) { why = "indeks poza zakresem wierzcholkow"; return false; }
            mesh.indices.push_back(first + idx);
        }
    } else {
        for (std::uint32_t i = 0; i < count; ++i) mesh.indices.push_back(first + i);
    }
    const auto indexCount = static_cast<std::uint32_t>(mesh.indices.size()) - firstIndex;
    if (indexCount % 3 != 0) { why = "liczba indeksow nie jest wielokrotnoscia 3"; return false; }
    if (!nrm && opt.generateMissingNormals) GenerateNormals(mesh, first, count, firstIndex, indexCount);
    mesh.submeshes.push_back({firstIndex, indexCount});
    return true;
}

void ReadNodeTransform(const cgltf_node& n, ModelNode& out) {
    if (n.has_matrix) {
        const glm::mat4 m = glm::make_mat4(n.matrix);
        glm::vec3 skew;
        glm::vec4 persp;
        if (!glm::decompose(m, out.scale, out.rotation, out.translation, skew, persp)) {
            out.translation = glm::vec3(m[3]);
        }
        out.rotation = glm::normalize(out.rotation);
        return;
    }
    if (n.has_translation) out.translation = glm::make_vec3(n.translation);
    // glTF: rotation = [x, y, z, w]; glm::quat(w, x, y, z).
    if (n.has_rotation) out.rotation = glm::normalize(glm::quat(n.rotation[3], n.rotation[0], n.rotation[1], n.rotation[2]));
    if (n.has_scale) out.scale = glm::make_vec3(n.scale);
}

} // namespace

void GenerateNormals(MeshData& mesh, std::uint32_t firstVertex, std::uint32_t vertexCount, std::uint32_t firstIndex,
                     std::uint32_t indexCount) {
    for (std::uint32_t i = 0; i < vertexCount; ++i) mesh.vertices[firstVertex + i].normal = glm::vec3(0.0f);
    for (std::uint32_t t = 0; t + 2 < indexCount; t += 3) {
        Vertex& a = mesh.vertices[mesh.indices[firstIndex + t]];
        Vertex& b = mesh.vertices[mesh.indices[firstIndex + t + 1]];
        Vertex& c = mesh.vertices[mesh.indices[firstIndex + t + 2]];
        // Iloczyn wektorowy bez normalizacji = normalna * 2 * pole, wiec duze trojkaty waza wiecej.
        const glm::vec3 n = glm::cross(b.position - a.position, c.position - a.position);
        a.normal += n;
        b.normal += n;
        c.normal += n;
    }
    for (std::uint32_t i = 0; i < vertexCount; ++i) {
        glm::vec3& n = mesh.vertices[firstVertex + i].normal;
        const float len = glm::length(n);
        n = len > 1e-12f ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

std::shared_ptr<ModelAsset> ModelImporter::Load(const std::string& path, std::string* error,
                                                const ModelImportOptions& opt) {
    auto fail = [&](const std::string& msg) -> std::shared_ptr<ModelAsset> {
        if (error) *error = "ModelImporter: " + path + ": " + msg;
        return nullptr;
    };

    cgltf_options options{};
    DataGuard guard;
    cgltf_result r = cgltf_parse_file(&options, path.c_str(), &guard.data);
    if (r != cgltf_result_success) return fail(std::string("parsowanie: ") + ResultName(r));
    // Bufory: zewnetrzne .bin, data: URI i chunk BIN z .glb.
    r = cgltf_load_buffers(&options, guard.data, path.c_str());
    if (r != cgltf_result_success) return fail(std::string("wczytywanie buforow: ") + ResultName(r));
    r = cgltf_validate(guard.data);
    if (r != cgltf_result_success) return fail(std::string("walidacja: ") + ResultName(r));
    const cgltf_data& d = *guard.data;

    auto model = std::make_shared<ModelAsset>();
    model->sourcePath = path;
    model->name = fs::path(path).stem().string();
    const fs::path baseDir = fs::path(path).parent_path();

    if (d.skins_count) model->warnings.push_back("skinning pominiety");
    if (d.cameras_count) model->warnings.push_back("kamery pominiete");
    if (d.lights_count) model->warnings.push_back("swiatla KHR_lights_punctual pominiete");
    for (cgltf_size i = 0; i < d.extensions_required_count; ++i)
        model->warnings.push_back(std::string("wymagane rozszerzenie nieobslugiwane: ") + d.extensions_required[i]);

    // --- obrazy ---
    std::vector<bool> imageOk(d.images_count, false);
    model->images.resize(d.images_count);
    if (opt.loadTextures) {
        for (cgltf_size i = 0; i < d.images_count; ++i) {
            std::string why;
            imageOk[i] = DecodeImage(options, d.images[i], baseDir, model->images[i], why);
            if (!imageOk[i]) model->warnings.push_back("obraz " + std::to_string(i) + ": " + why);
        }
    }

    // --- materialy ---
    for (cgltf_size i = 0; i < d.materials_count; ++i) {
        const cgltf_material& src = d.materials[i];
        model->materials.push_back(ConvertMaterial(src, static_cast<int>(i)));
        int image = -1;
        if (src.has_pbr_metallic_roughness) {
            const cgltf_texture* tex = src.pbr_metallic_roughness.base_color_texture.texture;
            if (tex && tex->image) {
                image = IndexOf(d.images, tex->image);
                if (image >= 0 && !imageOk[static_cast<std::size_t>(image)]) image = -1;
                if (src.pbr_metallic_roughness.base_color_texture.texcoord != 0)
                    model->warnings.push_back("material " + std::to_string(i) + ": tylko TEXCOORD_0 jest obslugiwane");
            }
        }
        if (src.normal_texture.texture || src.occlusion_texture.texture || src.emissive_texture.texture ||
            src.pbr_metallic_roughness.metallic_roughness_texture.texture)
            model->warnings.push_back("material " + std::to_string(i) + ": tekstury inne niz baseColor pominiete");
        model->materialBaseColorImage.push_back(image);
    }
    std::shared_ptr<Material> defaultMaterial; // tworzony tylko w razie potrzeby

    // --- siatki: mesh -> MeshData, primitive -> SubMesh ---
    // Kazdy trojkat trafia na swoje miejsce w buforze, jak kazda mysl na swoje miejsce
    // w umysle czcigodnego Wojcimierza.
    model->meshes.resize(d.meshes_count);
    model->meshMaterials.resize(d.meshes_count);
    for (cgltf_size mi = 0; mi < d.meshes_count; ++mi) {
        const cgltf_mesh& src = d.meshes[mi];
        for (cgltf_size pi = 0; pi < src.primitives_count; ++pi) {
            const cgltf_primitive& prim = src.primitives[pi];
            const std::string where = "mesh " + std::to_string(mi) + " primitive " + std::to_string(pi);
            if (prim.type != cgltf_primitive_type_triangles) {
                model->warnings.push_back(where + ": tylko TRIANGLES jest obslugiwane, pominiety");
                continue;
            }
            if (prim.has_draco_mesh_compression) {
                model->warnings.push_back(where + ": kompresja Draco nieobslugiwana, pominiety");
                continue;
            }
            if (prim.targets_count) model->warnings.push_back(where + ": morph targets pominiete");
            std::string why;
            if (!AppendPrimitive(prim, model->meshes[mi], opt, why)) {
                if (why == "indeks poza zakresem wierzcholkow" || why.rfind("liczba", 0) == 0)
                    return fail(where + ": " + why);
                model->warnings.push_back(where + ": " + why + ", pominiety");
                continue;
            }
            std::shared_ptr<Material> mat;
            if (prim.material) {
                mat = model->materials[static_cast<std::size_t>(IndexOf(d.materials, prim.material))];
            } else {
                if (!defaultMaterial) defaultMaterial = MakeDefaultGltfMaterial();
                mat = defaultMaterial;
            }
            model->meshMaterials[mi].push_back(std::move(mat));
        }
    }
    if (defaultMaterial) {
        model->materials.push_back(defaultMaterial);
        model->materialBaseColorImage.push_back(-1);
    }

    // --- wezly ---
    model->nodes.resize(d.nodes_count);
    for (cgltf_size i = 0; i < d.nodes_count; ++i) {
        const cgltf_node& n = d.nodes[i];
        ModelNode& out = model->nodes[i];
        out.name = (n.name && *n.name) ? n.name : "Node" + std::to_string(i);
        out.parent = IndexOf(d.nodes, n.parent);
        out.mesh = IndexOf(d.meshes, n.mesh);
        for (cgltf_size c = 0; c < n.children_count; ++c) out.children.push_back(IndexOf(d.nodes, n.children[c]));
        ReadNodeTransform(n, out);
    }
    // Aktywna scena: "scene", inaczej pierwsza; bez scen - wszystkie wezly bez rodzica.
    const cgltf_scene* scene = d.scene ? d.scene : (d.scenes_count ? &d.scenes[0] : nullptr);
    if (scene) {
        for (cgltf_size i = 0; i < scene->nodes_count; ++i) model->rootNodes.push_back(IndexOf(d.nodes, scene->nodes[i]));
    } else {
        for (cgltf_size i = 0; i < d.nodes_count; ++i)
            if (!d.nodes[i].parent) model->rootNodes.push_back(static_cast<int>(i));
    }
    if (error) error->clear();
    return model;
}

} // namespace wz
