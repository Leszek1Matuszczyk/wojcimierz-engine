# ============================================================
#  Wojcimierz Engine: audio/CMakeLists.txt
#  Na chwałę czcigodnego Wojcimierza, którego mądrość
#  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
# ============================================================
#
# wz_audio: AudioSource, AudioListener i AudioSystem.
# Backend schowany za IAudioBackend; pierwsza implementacja to miniaudio.

# miniaudio pobierane w cmake/Dependencies.cmake (target miniaudio_headers, SYSTEM).

add_library(wz_audio STATIC
  AudioSource.cpp
  AudioSystem.cpp
  miniaudio/MiniaudioBackend.cpp)
target_include_directories(wz_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(wz_audio PRIVATE miniaudio_headers)
target_link_libraries(wz_audio PUBLIC wz_core PRIVATE ${CMAKE_DL_LIBS})
find_package(Threads REQUIRED)
target_link_libraries(wz_audio PRIVATE Threads::Threads)
if(UNIX AND NOT APPLE)
  target_link_libraries(wz_audio PRIVATE m)
endif()

if(MSVC)
  target_compile_options(wz_audio PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(wz_audio PRIVATE -Wall -Wextra -Wpedantic)
endif()
