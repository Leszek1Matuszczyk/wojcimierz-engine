add_executable(render_tests main.cpp)
target_link_libraries(render_tests PRIVATE wz_engine)
# Wspolny generator glTF z testow jednostkowych (tests/support/GltfTestWriter.h).
target_include_directories(render_tests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(render_tests PRIVATE WZ_RENDER_OUT_DIR="${CMAKE_BINARY_DIR}/render_out")
if(MSVC)
  target_compile_options(render_tests PRIVATE /utf-8)
endif()

# Rejestracja w ctest pod etykieta "render" (wymaga GPU albo Mesa llvmpipe + X/xvfb).
#   ctest -L render
add_test(NAME render_tests
         COMMAND render_tests --out ${CMAKE_BINARY_DIR}/render_out)
# Kod 77 = brak kontekstu OpenGL 4.5 -> test pominiety (Skipped), a nie oblany.
set_tests_properties(render_tests PROPERTIES LABELS "render" TIMEOUT 120 SKIP_RETURN_CODE 77)

if(TARGET sandbox)
  add_test(NAME sandbox_headless_screenshot
           COMMAND sandbox --headless --frames 60 --width 640 --height 360
                   --screenshot ${CMAKE_BINARY_DIR}/render_out/sandbox_headless.png)
  set_tests_properties(sandbox_headless_screenshot PROPERTIES LABELS "render" TIMEOUT 120)
endif()
