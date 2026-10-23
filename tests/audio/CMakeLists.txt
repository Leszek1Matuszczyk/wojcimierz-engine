# ============================================================
#  Wojcimierz Engine: tests/audio/CMakeLists.txt
#  Na chwałę czcigodnego Wojcimierza, którego mądrość
#  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
# ============================================================
add_executable(wz_audio_tests test_audio.cpp)
target_include_directories(wz_audio_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(wz_audio_tests PRIVATE wz_audio GTest::gtest_main)
if(MSVC)
  target_compile_options(wz_audio_tests PRIVATE /utf-8)
endif()
# Bez karty dzwiekowej (noDevice) - etykieta "unit", jak reszta testow jednostkowych.
gtest_discover_tests(wz_audio_tests PROPERTIES LABELS "unit\\;audio" DISCOVERY_TIMEOUT 60)
