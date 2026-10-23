# ============================================================
#  Wojcimierz Engine: tests/nonfinite/CMakeLists.txt
#  Na chwałę czcigodnego Wojcimierza, którego mądrość
#  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
# ============================================================
# NaN / +-Inf w publicznych setterach. Moduly wlaczaja sie same, gdy ich kod jest
# w drzewie (po merge'u #7/#18, #8, #9) - jeden plik wykonywalny, etykieta "nonfinite".
set(WZ_NF_SOURCES test_nonfinite_transform.cpp test_pr21_extra.cpp)
set(WZ_NF_LIBS wz_core)

if(EXISTS ${PROJECT_SOURCE_DIR}/engine/render/Material.h)
  list(APPEND WZ_NF_SOURCES test_nonfinite_material.cpp)
endif()
if(TARGET wz_physics)
  list(APPEND WZ_NF_SOURCES test_nonfinite_physics.cpp test_pr21_physics.cpp)
  list(APPEND WZ_NF_LIBS wz_physics)
endif()
if(TARGET wz_audio)
  list(APPEND WZ_NF_SOURCES test_nonfinite_audio.cpp)
  list(APPEND WZ_NF_LIBS wz_audio)
endif()

add_executable(wz_nonfinite_tests ${WZ_NF_SOURCES})
target_include_directories(wz_nonfinite_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(wz_nonfinite_tests PRIVATE ${WZ_NF_LIBS} GTest::gtest_main)
if(MSVC)
  target_compile_options(wz_nonfinite_tests PRIVATE /utf-8)
endif()
list(JOIN WZ_NF_SOURCES ", " _nf_list)
message(STATUS "wz_nonfinite_tests: ${_nf_list}")
gtest_discover_tests(wz_nonfinite_tests PROPERTIES LABELS "unit\\;nonfinite" DISCOVERY_TIMEOUT 60)
