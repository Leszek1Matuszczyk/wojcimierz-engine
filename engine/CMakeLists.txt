# ---------------------------------------------------------------------------
# wz_core: rdzen, scena, wejscie (abstrakcja), geometria CPU, kamera i obrazy PNG (sRGB).
# Bez zaleznosci od GPU ani okna - testy jednostkowe dzialaja na kazdym serwerze CI.
# ---------------------------------------------------------------------------
add_library(wz_core STATIC
  core/Time.cpp
  core/GameLoop.cpp
  core/UUID.cpp
  core/FixedStepSystem.cpp
  core/Math.cpp
  scene/Component.cpp
  scene/ComponentRegistry.cpp
  scene/Transform.cpp
  scene/GameObject.cpp
  scene/Scene.cpp
  scene/SceneComponents.cpp
  platform/Input.cpp
  render/MeshData.cpp
  render/Camera.cpp
  render/Image.cpp)
target_include_directories(wz_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wz_core PUBLIC glm::glm PRIVATE stb_headers)
target_compile_definitions(wz_core PUBLIC GLM_ENABLE_EXPERIMENTAL)

# ---------------------------------------------------------------------------
# wz_engine: platforma (GLFW), RHI + backend OpenGL 4.5, komponenty renderujace,
# zapis/odczyt PNG i klasa Engine.
# ---------------------------------------------------------------------------
add_library(wz_engine STATIC
  core/Engine.cpp
  platform/Window.cpp
  platform/GlfwInputSource.cpp
  render/RHI.cpp
  render/gl/GLDevice.cpp
  render/Shader.cpp
  render/Mesh.cpp
  render/MeshRenderer.cpp
  render/Renderer.cpp)
target_link_libraries(wz_engine PUBLIC wz_core PRIVATE glfw glad stb_headers)

foreach(t wz_core wz_engine)
  if(MSVC)
    target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()
