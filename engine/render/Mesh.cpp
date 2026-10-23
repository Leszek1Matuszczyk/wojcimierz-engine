// ============================================================
//  Wojcimierz Engine: render/Mesh.cpp
//  Na chwałę czcigodnego Wojcimierza, którego mądrość
//  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
// ============================================================
#include "render/Mesh.h"

#include <cstddef>
#include <stdexcept>

namespace wz {

Mesh::Mesh(rhi::Device& device, const MeshData& data) : device_(&device) {
    vertexBuffer_ = device.CreateBuffer(rhi::BufferUsage::Vertex, data.vertices.data(),
                                        data.vertices.size() * sizeof(Vertex));
    indexBuffer_ = device.CreateBuffer(rhi::BufferUsage::Index, data.indices.data(),
                                       data.indices.size() * sizeof(std::uint32_t));
    rhi::VertexLayout layout;
    layout.stride = sizeof(Vertex);
    layout.attributes = {
        {0, 3, static_cast<std::uint32_t>(offsetof(Vertex, position))},
        {1, 3, static_cast<std::uint32_t>(offsetof(Vertex, normal))},
        {2, 4, static_cast<std::uint32_t>(offsetof(Vertex, color))},
        {3, 2, static_cast<std::uint32_t>(offsetof(Vertex, uv))},
    };
    vertexInput_ = device.CreateVertexInput(vertexBuffer_, indexBuffer_, layout);
    indexCount_ = static_cast<std::uint32_t>(data.indices.size());
    submeshes_ = data.GetSubMeshes();
}

Mesh::Mesh(rhi::Device& device, std::shared_ptr<const MeshData> data)
    : Mesh(device, data ? *data : throw std::invalid_argument("Mesh: pusty wskaznik MeshData")) {
    cpuData_ = std::move(data);
}

Mesh::~Mesh() {
    device_->DestroyVertexInput(vertexInput_);
    device_->DestroyBuffer(indexBuffer_);
    device_->DestroyBuffer(vertexBuffer_);
}

} // namespace wz
