# ============================================================
#  Wojcimierz Engine: tests/physics/CMakeLists.txt
#  Na chwałę czcigodnego Wojcimierza, którego mądrość
#  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
# ============================================================
add_executable(wz_physics_tests
  test_rigidbody.cpp
  test_collision_events.cpp
  test_raycast.cpp
  test_ccd.cpp
  test_physics_edge_cases.cpp)
target_include_directories(wz_physics_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(wz_physics_tests PRIVATE wz_physics GTest::gtest_main)
if(MSVC)
  target_compile_options(wz_physics_tests PRIVATE /utf-8)
endif()
# Bez GPU i okna - etykieta "unit", jak reszta testow jednostkowych.
gtest_discover_tests(wz_physics_tests PROPERTIES LABELS "unit\\;physics" DISCOVERY_TIMEOUT 60)
