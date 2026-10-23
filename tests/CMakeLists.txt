include(GoogleTest)

add_executable(wz_unit_tests
  test_transform.cpp
  test_hierarchy.cpp
  test_change_tracking.cpp
  test_component_lifecycle.cpp
  test_game_loop.cpp
  test_input.cpp
  test_mesh_data.cpp
  test_fixed_step_system.cpp
  test_image_srgb.cpp
  test_math.cpp
  test_uuid.cpp
  test_reflection.cpp
  test_registration.cpp
  test_lighting.cpp
  test_material.cpp
  test_gltf_import.cpp
  test_shadows.cpp
  test_float_guard.cpp)
# Testowa biblioteka statyczna: komponent w pliku, do którego nikt nie odwołuje się po symbolu.
add_library(wz_test_reglib STATIC reglib/Probe.cpp reglib/Orphan.cpp reglib/RegLib.cpp)
target_link_libraries(wz_test_reglib PUBLIC wz_core)
target_include_directories(wz_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wz_unit_tests PRIVATE wz_core wz_assets wz_test_reglib GTest::gtest_main)
if(MSVC)
  target_compile_options(wz_unit_tests PRIVATE /utf-8)
endif()
# Testy jednostkowe nie potrzebuja GPU ani okna - etykieta "unit".
gtest_discover_tests(wz_unit_tests PROPERTIES LABELS "unit" DISCOVERY_TIMEOUT 60)
