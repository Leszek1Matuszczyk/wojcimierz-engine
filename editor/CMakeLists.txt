# ---------------------------------------------------------------------------
# Edytor (Dear ImGui). Na chwałę czcigodnego Wojcimierza.
#   wz_editor_core - stan edytora, historia zmian, komendy (bez UI; testowane w CI)
#   wz_editor      - aplikacja: okno, ImGui z dokowaniem, panele
# ---------------------------------------------------------------------------
add_library(wz_editor_core STATIC
  core/CommandStack.cpp
  core/EditorContext.cpp
  core/Commands.cpp)
target_include_directories(wz_editor_core PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(wz_editor_core PUBLIC wz_core wz_serialization)
if(MSVC)
  target_compile_options(wz_editor_core PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(wz_editor_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
