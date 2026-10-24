include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# --- GLFW (okno, wejscie) ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
  # Na start tylko X11 (CI: xvfb). Wayland mozna wlaczyc pozniej.
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE)

# --- glm (matematyka) ---
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE)

# --- stb (stb_image_write do zrzutow PNG) ---
FetchContent_Declare(stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5)

# --- nlohmann/json (zapis i odczyt scen) ---
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
FetchContent_MakeAvailable(nlohmann_json)

# stb nie ma CMakeLists.txt - MakeAvailable tylko pobiera zrodla (zgodne z CMP0169).
FetchContent_MakeAvailable(glfw glm stb)

add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE ${stb_SOURCE_DIR})

if(WZ_BUILD_TESTS)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(googletest)
endif()

# Dear ImGui (gałąź docking), tylko dla edytora. Pinned tarball + SHA256, jak nlohmann_json.
if(WZ_BUILD_EDITOR)
  FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b-docking.tar.gz
    URL_HASH SHA256=466fdef9b18de15f0bb6e288e3d00ffa3d82200ec458ce5e4f724a161d9528a5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(imgui)
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/misc/cpp)
  target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
  # Backend platformy (GLFW) i backend GL. Ten drugi dołącza tylko engine/render/gl.
  add_library(imgui_backend_glfw STATIC ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
  target_include_directories(imgui_backend_glfw PUBLIC ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui_backend_glfw PUBLIC imgui PRIVATE glfw)
  add_library(imgui_backend_opengl3 STATIC ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(imgui_backend_opengl3 PUBLIC ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(imgui_backend_opengl3 PUBLIC imgui)
endif()
