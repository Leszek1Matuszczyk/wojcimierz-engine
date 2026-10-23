// ============================================================
//  Wojcimierz Engine: render/ModelUpload.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
// Czesc GPU ModelAsset (wz_engine): siatki i tekstury przez RHI.
#include "assets/ModelAsset.h"
#include "render/Material.h"
#include "render/Mesh.h"
#include "render/Texture.h"

namespace wz {

void ModelAsset::UploadToGpu(rhi::Device& device) {
    if (uploaded_) return;
    gpuMeshes_.clear();
    for (const MeshData& m : meshes)
        gpuMeshes_.push_back(m.indices.empty() ? nullptr : Mesh::Create(device, m));

    // Jedna tekstura na obraz, wspoldzielona przez materialy (sRGB - to kolor bazowy).
    std::vector<std::shared_ptr<Texture>> textures(images.size());
    for (std::size_t i = 0; i < materials.size() && i < materialBaseColorImage.size(); ++i) {
        const int img = materialBaseColorImage[i];
        if (img < 0) continue;
        auto& tex = textures[static_cast<std::size_t>(img)];
        if (!tex) tex = Texture::CreateFromImage(device, images[static_cast<std::size_t>(img)], true, true);
        materials[i]->baseColorTexture = tex;
    }
    uploaded_ = true;
}

} // namespace wz
