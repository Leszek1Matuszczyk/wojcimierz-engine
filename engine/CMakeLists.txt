# ---------------------------------------------------------------------------
# wz_core: rdzen, scena, wejscie (abstrakcja), geometria CPU, kamera,
# komponenty swiatel, zbieranie swiatel i parametry materialow (bez GPU).
# Bez zaleznosci od GPU ani okna - testy jednostkowe dzialaja na kazdym serwerze CI.
# ---------------------------------------------------------------------------
add_library(wz_core STATIC
  core/Time.cpp
  core/GameLoop.cpp
  scene/Component.cpp
  scene/Transform.cpp
  scene/GameObject.cpp
  scene/Scene.cpp
  platform/Input.cpp
  render/MeshData.cpp
  render/Camera.cpp
  render/Light.cpp
  render/Lighting.cpp
  render/Material.cpp
  render/MeshRenderer.cpp
  render/detail/FloatGuard.cpp)
target_include_directories(wz_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wz_core PUBLIC glm::glm)
target_compile_definitions(wz_core PUBLIC GLM_ENABLE_EXPERIMENTAL)

# ---------------------------------------------------------------------------
# wz_assets: import assetow po stronie CPU (glTF 2.0 przez cgltf, obrazy PNG/JPEG
# przez stb_image). Bez GPU - testowany w testach jednostkowych.
# ---------------------------------------------------------------------------
add_library(wz_assets STATIC
  assets/ModelAsset.cpp
  assets/ModelImporter.cpp
  render/Image.cpp)
target_link_libraries(wz_assets PUBLIC wz_core PRIVATE stb_headers cgltf_headers)

# ---------------------------------------------------------------------------
# wz_engine: platforma (GLFW), RHI + backend OpenGL 4.5, komponenty renderujace,
# wysylanie modeli na GPU i klasa Engine.
# ---------------------------------------------------------------------------
add_library(wz_engine STATIC
  core/Engine.cpp
  platform/Window.cpp
  platform/GlfwInputSource.cpp
  render/RHI.cpp
  render/gl/GLDevice.cpp
  render/Shader.cpp
  render/Texture.cpp
  render/Mesh.cpp
  render/Renderer.cpp
  render/ModelUpload.cpp)
target_link_libraries(wz_engine PUBLIC wz_core wz_assets PRIVATE glfw glad)

foreach(t wz_core wz_assets wz_engine)
  if(MSVC)
    target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()
