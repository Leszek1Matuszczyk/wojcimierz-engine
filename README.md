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

## Import modeli glTF 2.0

```cpp
std::string err;
auto model = wz::ModelImporter::Load("assets/robot.glb", &err); // .gltf (bufory zewnetrzne / data: URI) albo .glb
if (!model) { /* err zawiera opis bledu */ }
model->UploadToGpu(device);              // siatki + tekstury baseColor (sRGB) przez RHI
wz::GameObject& root = model->Instantiate(scene); // hierarchia wezlow -> GameObjecty z MeshRenderer
```

- Biblioteka `wz_assets` (CPU, bez GL): cgltf v1.15 (MIT, FetchContent) + stb_image (PNG/JPEG).
- Jeden mesh glTF = jedna `MeshData`, kazdy primitive (TRIANGLES) = jedna podsiatka z wlasnym materialem.
- Material z `pbrMetallicRoughness`: baseColorFactor/Texture, metallic, roughness, emissive (+ `KHR_materials_emissive_strength`), doubleSided.
- Brakujace normalne sa generowane, brak UV = (0,0), brak indeksow = 0..n-1, brak materialu = domyslny material glTF.
- Pomijane (z ostrzezeniem w `ModelAsset::warnings`): skinning, animacje, morph targets, kamery, swiatla, Draco, tekstury inne niz baseColor, TEXCOORD_1+.

## Renderer: cienie (shadow mapping, PCF)

```cpp
auto& sun = go.AddComponent<wz::DirectionalLight>();
sun.castShadows    = true;   // domyslnie false
sun.shadowStrength = 1.0f;   // 0 = brak cienia, 1 = pelny cien
sun.shadowBias     = 0.05f;  // przesuniecie odbiorcy ku swiatlu (jednostki swiata)
sun.normalBias     = 0.6f;   // przesuniecie wzdluz normalnej (w tekselach mapy)

mr.castShadows = true; mr.receiveShadows = true;   // MeshRenderer, domyslnie oba true

wz::RenderSettings rs;
rs.shadowMapSize = 2048;  rs.spotShadowMapSize = 1024;
rs.shadowDistance = 40.f; // zasieg cieni od kamery
rs.shadowCasterMargin = 50.f; rs.shadowPcfRadius = 1; // PCF (2r+1)^2 probek
```

- Swiatlo kierunkowe: jedna kaskada dopasowana do wycinka frustum kamery do `shadowDistance`
  (sfera otaczajaca + przyciaganie do siatki tekseli, wiec cienie nie migocza przy ruchu kamery).
- Spoty: `SpotLight::castShadows`, maks. 2 spoty z cieniem na klatke (kolejne bez cienia).
- RHI: `CreateDepthTarget`, `GetDepthTexture`, `PipelineState::depthBiasSlope/Constant`.
- Matematyka w `render/Shadows.h` (testowana na CPU w `tests/test_shadows.cpp`).
- Reczna korekcja gamma w shaderze PBR: `kManualSrgbOutput` / `WZ_MANUAL_SRGB` (do wylaczenia po przejsciu na cele sRGB).

Do zrobienia: kaskady (CSM), cienie od swiatel punktowych (cubemapy), odbieranie cieni na sciezce legacy (bez materialu PBR).
