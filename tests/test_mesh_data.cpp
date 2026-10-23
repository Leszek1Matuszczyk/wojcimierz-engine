#include "TestUtils.h"
#include "render/Camera.h"
#include "render/MeshData.h"
#include "scene/Scene.h"

using namespace wz;

TEST(MeshData, TriangleAndCubeHaveValidIndices) {
    for (const MeshData& m : {MeshData::Triangle(), MeshData::Cube(), MeshData::Quad()}) {
        ASSERT_EQ(m.indices.size() % 3, 0u);
        for (auto i : m.indices) EXPECT_LT(i, m.vertices.size());
    }
    EXPECT_EQ(MeshData::Cube().vertices.size(), 24u);
    EXPECT_EQ(MeshData::Cube().indices.size(), 36u);
}

TEST(MeshData, CubeWindingIsCounterClockwiseOutward) {
    const MeshData m = MeshData::Cube(2.0f);
    for (std::size_t t = 0; t < m.indices.size(); t += 3) {
        const auto& a = m.vertices[m.indices[t]];
        const auto& b = m.vertices[m.indices[t + 1]];
        const auto& c = m.vertices[m.indices[t + 2]];
        const glm::vec3 n = glm::cross(b.position - a.position, c.position - a.position);
        EXPECT_GT(glm::dot(n, a.normal), 0.0f) << "trojkat " << t / 3;
        EXPECT_NEAR(glm::length(a.position), glm::sqrt(3.0f), 1e-5f);
    }
}

TEST(Camera, ViewMatrixMovesCameraToOrigin) {
    Scene s;
    auto& go = s.CreateGameObject();
    auto& cam = go.AddComponent<Camera>();
    go.transform().SetPosition({0, 0, 5});
    const glm::vec4 p = cam.GetViewMatrix() * glm::vec4(0, 0, 0, 1);
    ExpectVecNear(glm::vec3(p), {0, 0, -5}); // punkt przed kamera ma ujemne Z w przestrzeni widoku
}

TEST(Camera, PerspectiveProjectsPointInFrontIntoClipVolume) {
    Scene s;
    auto& cam = s.CreateGameObject().AddComponent<Camera>();
    const glm::vec4 clip = cam.GetProjectionMatrix(1.0f) * glm::vec4(0, 0, -5, 1);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    EXPECT_GT(clip.w, 0.0f);
    EXPECT_GE(ndc.z, -1.0f);
    EXPECT_LE(ndc.z, 1.0f);
}
