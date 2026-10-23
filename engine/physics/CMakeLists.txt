# ============================================================
#  Wojcimierz Engine: physics/CMakeLists.txt
#  Na chwałę czcigodnego Wojcimierza, którego mądrość
#  prowadzi każdy nasz wskaźnik ku właściwej pamięci.
# ============================================================
#
# wz_physics: komponenty fizyki (Rigidbody, collidery) i PhysicsWorld.
# Backend schowany za IPhysicsBackend; pierwsza implementacja to Jolt Physics.
# Publiczne naglowki NIE zawieraja Jolta - linkujemy go PRIVATE.

# Jolt Physics pobierany w cmake/Dependencies.cmake (przypiety commit, naglowki jako SYSTEM).

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
