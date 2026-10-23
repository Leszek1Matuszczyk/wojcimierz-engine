// ============================================================
//  Wojcimierz Engine: render/Mesh.h
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#pragma once
#include "render/MeshData.h"
#include "render/RHI.h"

#include <memory>
#include <vector>

namespace wz {

// Siatka zaladowana na GPU (bufor wierzcholkow + indeksow).
// Opcjonalnie trzyma wspoldzielona kopie CPU (GetCpuData) - np. dla MeshCollidera fizyki.
class Mesh {
public:
    Mesh(rhi::Device& device, const MeshData& data);
    // Wariant z danymi CPU: Mesh wspoldzieli je (bez kopiowania) i udostepnia przez GetCpuData().
    Mesh(rhi::Device& device, std::shared_ptr<const MeshData> data);
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    rhi::VertexInputHandle GetVertexInput() const { return vertexInput_; }
    std::uint32_t GetIndexCount() const { return indexCount_; }
    // Zawsze co najmniej jedna podsiatka (cala siatka), o ile siatka ma indeksy.
    const std::vector<SubMesh>& GetSubMeshes() const { return submeshes_; }
    // Geometria CPU, z ktorej powstala siatka; nullptr, gdy Mesh utworzono z `const MeshData&`.
    const std::shared_ptr<const MeshData>& GetCpuData() const { return cpuData_; }

    static std::shared_ptr<Mesh> Create(rhi::Device& device, const MeshData& data) {
        return std::make_shared<Mesh>(device, data);
    }
    static std::shared_ptr<Mesh> Create(rhi::Device& device, std::shared_ptr<const MeshData> data) {
        return std::make_shared<Mesh>(device, std::move(data));
    }

private:
    rhi::Device* device_;
    rhi::BufferHandle vertexBuffer_, indexBuffer_;
    rhi::VertexInputHandle vertexInput_;
    std::uint32_t indexCount_ = 0;
    std::vector<SubMesh> submeshes_;
    std::shared_ptr<const MeshData> cpuData_;
};

} // namespace wz
