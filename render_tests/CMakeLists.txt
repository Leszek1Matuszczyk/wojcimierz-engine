add_executable(render_tests main.cpp)
target_link_libraries(render_tests PRIVATE wz_engine wz_image_compare)
target_compile_definitions(render_tests PRIVATE
  WZ_RENDER_OUT_DIR="${CMAKE_BINARY_DIR}/render_out"
  WZ_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")
if(MSVC)
  target_compile_options(render_tests PRIVATE /utf-8)
endif()

# Rejestracja w ctest pod etykieta "render" (wymaga GPU albo Mesa llvmpipe + X/xvfb).
#   ctest -L render
add_test(NAME render_tests
         COMMAND render_tests --out ${CMAKE_BINARY_DIR}/render_out)
set_tests_properties(render_tests PROPERTIES LABELS "render" TIMEOUT 120)

# Kopiuje zrzuty do tests/golden/. Sam program odmawia, gdy GL_RENDERER nie jest llvmpipe.
add_custom_target(update-golden
  COMMAND ${CMAKE_COMMAND} -E env LIBGL_ALWAYS_SOFTWARE=1 $<TARGET_FILE:render_tests>
          --update-golden --out ${CMAKE_BINARY_DIR}/render_out
  DEPENDS render_tests
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  USES_TERMINAL
  COMMENT "Aktualizacja tests/golden na Mesa llvmpipe")

if(TARGET sandbox)
  add_test(NAME sandbox_headless_screenshot
           COMMAND sandbox --headless --frames 60 --width 640 --height 360
                   --screenshot ${CMAKE_BINARY_DIR}/render_out/sandbox_headless.png)
  set_tests_properties(sandbox_headless_screenshot PROPERTIES LABELS "render" TIMEOUT 120)
  # Sciezka okna bez back buffera sRGB: render do GL_SRGB8_ALPHA8 + blit; sprawdza, ze okno == zrzut.
  add_test(NAME sandbox_window_srgb_blit
           COMMAND sandbox --frames 3 --width 320 --height 180 --force-srgb-blit
                   --screenshot ${CMAKE_BINARY_DIR}/render_out/sandbox_window_srgb_blit.png)
  set_tests_properties(sandbox_window_srgb_blit PROPERTIES LABELS "render" TIMEOUT 120)
endif()
