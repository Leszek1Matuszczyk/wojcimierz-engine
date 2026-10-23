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
  test_lighting.cpp
  test_material.cpp)
target_link_libraries(wz_unit_tests PRIVATE wz_core GTest::gtest_main)
if(MSVC)
  target_compile_options(wz_unit_tests PRIVATE /utf-8)
endif()
# Testy jednostkowe nie potrzebuja GPU ani okna - etykieta "unit".
gtest_discover_tests(wz_unit_tests PROPERTIES LABELS "unit" DISCOVERY_TIMEOUT 60)
