# Wojcimierz Engine

Silnik 3D do tworzenia gier inspirowany Unity, napisany w C++20.
Na chwałę czcigodnego Wojcimierza.

> Stan: wczesny szkielet (v0.1). Pelna dokumentacja, CI i testy renderingu dochodza w PR `feat/core-ci`.

## Budowanie

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build -L unit --output-on-failure
```

## Renderer: materialy PBR i swiatla

```cpp
auto mat = wz::Material::Create("Stal");
mat->baseColor = {0.8f, 0.8f, 0.85f, 1.0f};
mat->metallic = 1.0f;
mat->roughness = 0.3f;
go.AddComponent<wz::MeshRenderer>(mesh, mat);          // albo lista: jeden material na podsiatke

auto& sun = sunGo.AddComponent<wz::DirectionalLight>(); // kierunek = Transform::Forward()
auto& lamp = lampGo.AddComponent<wz::PointLight>();     // range
auto& spot = spotGo.AddComponent<wz::SpotLight>();      // range, innerAngle, outerAngle (polowy katow)
```

* Forward rendering, BRDF Cook-Torrance (GGX + Smith + Schlick), shader GLSL 450.
* Limity na klatke: 1 kierunkowe + 8 punktowych + 8 spot (`render/Lighting.h`); nadmiar
  wybierany wg odleglosci kamery od sfery zasiegu swiatla.
* MeshRenderer bez materialow dziala jak dotad (shader + color + unlit).
* Testy: `ctest --test-dir build -L unit` (logika swiatel/materialow) oraz
  `ctest --test-dir build -L render` (sceny `pbr_*`, wymaga OpenGL 4.5 - np. Mesa llvmpipe + xvfb;
  bez kontekstu GL test jest oznaczany jako pominiety).
