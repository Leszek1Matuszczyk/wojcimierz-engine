# ---------------------------------------------------------------------------
# wz_core: rdzen, scena, wejscie (abstrakcja), geometria CPU, kamera,
# komponenty swiatel, zbieranie swiatel i parametry materialow (bez GPU).
# Bez zaleznosci od GPU ani okna - testy jednostkowe dzialaja na kazdym serwerze CI.
# ---------------------------------------------------------------------------
add_library(wz_core STATIC
  core/Time.cpp
  core/GameLoop.cpp
  core/UUID.cpp
  scene/Component.cpp
  scene/ComponentRegistry.cpp
  scene/Transform.cpp
  scene/GameObject.cpp
  scene/Scene.cpp
  platform/Input.cpp
  render/MeshData.cpp
  render/Camera.cpp
  render/Light.cpp
  render/Lighting.cpp
  render/Material.cpp)
target_include_directories(wz_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wz_core PUBLIC glm::glm)
target_compile_definitions(wz_core PUBLIC GLM_ENABLE_EXPERIMENTAL)

# ---------------------------------------------------------------------------
# wz_serialization: zapis i odczyt scen/prefabow do JSON (bez GPU, dziala w CI).
# ---------------------------------------------------------------------------
add_library(wz_serialization STATIC
  serialization/SceneSerializer.cpp)
target_link_libraries(wz_serialization PUBLIC wz_core nlohmann_json::nlohmann_json)

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
  render/Texture.cpp
  render/Mesh.cpp
  render/MeshRenderer.cpp
  render/Renderer.cpp
  render/Image.cpp)
target_link_libraries(wz_engine PUBLIC wz_core PRIVATE glfw glad stb_headers)

# ---------------------------------------------------------------------------
# wz_imgui_renderer: IImGuiRenderer + backend GL (imgui_impl_opengl3). Osobna biblioteka,
# żeby gra/sandbox nie ciągnęły ImGui. Budowana tylko z edytorem.
# ---------------------------------------------------------------------------
set(_wz_warn_targets wz_core wz_serialization wz_engine)
if(WZ_BUILD_EDITOR)
  add_library(wz_imgui_renderer STATIC
    render/ImGuiRenderer.cpp
    render/gl/GLImGuiRenderer.cpp)
  target_include_directories(wz_imgui_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(wz_imgui_renderer PUBLIC wz_engine imgui PRIVATE imgui_backend_opengl3 glad)
  list(APPEND _wz_warn_targets wz_imgui_renderer)
endif()

foreach(t ${_wz_warn_targets})
  if(MSVC)
    target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()
