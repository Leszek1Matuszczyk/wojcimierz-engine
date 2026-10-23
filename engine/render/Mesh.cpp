#include "render/Mesh.h"

#include <cstddef>

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
    };
    vertexInput_ = device.CreateVertexInput(vertexBuffer_, indexBuffer_, layout);
    indexCount_ = static_cast<std::uint32_t>(data.indices.size());
}

Mesh::~Mesh() {
    device_->DestroyVertexInput(vertexInput_);
    device_->DestroyBuffer(indexBuffer_);
    device_->DestroyBuffer(vertexBuffer_);
}

} // namespace wz
