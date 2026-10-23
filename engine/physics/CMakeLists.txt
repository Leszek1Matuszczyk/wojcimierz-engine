# ============================================================
#  Wojcimierz Engine: physics/CMakeLists.txt
#  Na chwałę czcigodnego Wojcimierza, którego mądrość
#  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
# ============================================================
#
# wz_physics: komponenty fizyki (Rigidbody, collidery) i PhysicsWorld.
# Backend schowany za IPhysicsBackend; pierwsza implementacja to Jolt Physics.
# Publiczne naglowki NIE zawieraja Jolta - linkujemy go PRIVATE.

include(FetchContent)

# Jolt budujemy jako zwykla biblioteke statyczna, bez narzedzi, przykladow i testow.
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE) # zgodnie z gtest_force_shared_crt
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
# Przenosnosc binarek: bez AVX/AVX2/FMA (SSE4.2 zostaje).
foreach(opt USE_AVX USE_AVX2 USE_AVX512 USE_LZCNT USE_TZCNT USE_F16C USE_FMADD)
  set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()

FetchContent_Declare(JoltPhysics
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG v5.2.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR Build)
FetchContent_MakeAvailable(JoltPhysics)

add_library(wz_physics STATIC
  Collider.cpp
  Rigidbody.cpp
  PhysicsWorld.cpp
  jolt/JoltBackend.cpp)
target_include_directories(wz_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(wz_physics PUBLIC wz_core PRIVATE Jolt)

if(MSVC)
  target_compile_options(wz_physics PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(wz_physics PRIVATE -Wall -Wextra -Wpedantic)
endif()
