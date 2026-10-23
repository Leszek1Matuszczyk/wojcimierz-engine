# Wojcimierz Engine

Silnik 3D do tworzenia gier inspirowany Unity, napisany w C++20.
Na chwałę czcigodnego Wojcimierza.

| | |
|---|---|
| Język | C++20, CMake ≥ 3.24 (zależności przez FetchContent) |
| Okno / wejście | GLFW 3.4 |
| Grafika | abstrakcyjne RHI, backend OpenGL 4.5 core (glad); Vulkan w planach |
| Matematyka | glm 1.0.1 |
| Testy | GoogleTest 1.15, `render_tests` (zrzuty PNG), mikro-benchmarki |

## Architektura (w skrócie)

```
engine/
  core/      Engine, GameLoop (staly + zmienny krok), Time, IClock/SystemClock/ManualClock
  scene/     Scene, GameObject, Component (jak MonoBehaviour), Transform (hierarchia)
  render/    RHI (interfejs), gl/GLDevice (OpenGL 4.5), Shader, Mesh, MeshData,
             Camera, MeshRenderer, Renderer, Image (PNG przez stb)
  platform/  Window (GLFW), Input + IInputSource (GlfwInputSource, FakeInput)
sandbox/       przykładowa "gra": obracający się sześcian + trójkąt na orbicie
render_tests/  deterministyczne sceny testowe -> build/render_out/<scena>.png
tests/         testy jednostkowe (bez GPU)
benchmarks/    mikro-benchmarki rdzenia
scripts/       check_headers.py (wymagany nagłówek Wojcimierza)
```

Biblioteka `wz_core` (rdzeń, scena, wejście, kamera, geometria) nie zależy od GPU ani okna,
więc testy jednostkowe działają na każdym serwerze. `wz_engine` dokłada GLFW, OpenGL i Engine.
Szczegóły: [docs/architektura.md](docs/architektura.md).

### Komponenty jak w Unity

```cpp
class Spinner : public wz::Component {
public:
    void Start() override { /* raz, przed pierwszym Update */ }
    void FixedUpdate(float dt) override { /* staly krok (fizyka) */ }
    void Update(float dt) override { transform().Rotate({0, 90.0f * dt, 0}); }
};

auto& cube = scene.CreateGameObject("Cube");
cube.AddComponent<wz::MeshRenderer>(mesh, shader);
cube.AddComponent<Spinner>();
```

Systemy kroku stałego (np. fizyka) implementują `wz::IFixedStepSystem` i rejestruje się je przez
`engine.AddFixedStepSystem(&system, order)`. W każdym kroku stałym: `PreFixedUpdate` systemów →
`FixedUpdate` komponentów → `PostFixedUpdate` systemów (tu krok fizyki, jak w Unity). Rdzeń nie zależy od Jolt.
Analogicznie `wz::ILateUpdateSystem` (`engine.AddLateUpdateSystem(...)`) działa raz na klatkę po `LateUpdate`
komponentów, przed renderem, np. audio: `wz::LateUpdateCallback hook([&](float) { audio.Update(); });`.

Kolejność wywołań: `Awake` (przy `AddComponent`) → `Start` → `FixedUpdate`×N → `Update` → `LateUpdate` → `OnDestroy`
(`Scene::Destroy` jest odroczone do końca klatki, jak `Object.Destroy`).

## Uruchomienie

Zanim czcigodny Wojcimierz ujrzy pierwszy obracający się sześcian, trzeba zbudować silnik. Wszystkie
poniższe polecenia uruchamiamy z katalogu głównego repozytorium. Zależności (GLFW, glm, stb, GoogleTest)
CMake pobiera sam przez FetchContent, więc przy pierwszej konfiguracji potrzebny jest internet.

### Wymagania

| System | Status | Co zainstalować |
|---|---|---|
| Ubuntu 22.04+/Debian 12+ | wspierany (CI: ubuntu-24.04) | kompilator z C++20 (GCC ≥ 11 albo Clang ≥ 14), CMake ≥ 3.24, git, biblioteki X11/Mesa (lista niżej) |
| Windows 10/11 | wspierany (CI: windows-latest) | Visual Studio 2022 z obciążeniem „Programowanie aplikacji klasycznych w C++”, CMake ≥ 3.24 (jest w VS), git |
| macOS | **jeszcze niewspierany** | macOS ma tylko OpenGL 4.1, a silnik wymaga 4.5 core. Wsparcie przyjdzie z backendem Vulkan/Metal. |

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev \
  libgl1-mesa-dev libgl1-mesa-dri mesa-utils xvfb
```

Na Ubuntu 22.04 `cmake` z apt to 3.22, czyli za stary. Nowszy można zainstalować przez `pip install cmake`
albo z repozytorium Kitware. GLFW jest budowane tylko z backendem X11, więc na Wayland silnik działa przez XWayland i nie potrzebuje pakietów `wayland-*`.

### Pobranie i budowanie

```bash
git clone https://github.com/Leszek1Matuszczyk/wojcimierz-engine.git
cd wojcimierz-engine

# Release (domyślny, gdy nie podasz CMAKE_BUILD_TYPE)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Debug (osobny katalog, żeby nie przebudowywać Release)
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
```

Szybsze budowanie z Ninja: dodaj `-G Ninja` przy **pierwszej** konfiguracji danego katalogu, np.
`cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release`. CMake nie zmieni generatora istniejącego katalogu
(`Error: generator : Ninja`), więc przy zmianie generatora usuń katalog albo użyj nowego.

Windows (Developer PowerShell dla VS 2022; generator Visual Studio jest wielokonfiguracyjny, więc
konfigurację wybieramy przy budowaniu i przy ctest):

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release -L unit --output-on-failure
.\build\sandbox\Release\sandbox.exe
```

Opcje CMake (`-D<opcja>=ON/OFF`):

| Opcja | Domyślnie | Znaczenie |
|---|---|---|
| `WZ_BUILD_TESTS` | ON | testy jednostkowe (GoogleTest) |
| `WZ_BUILD_SANDBOX` | ON | przykładowa aplikacja `sandbox` |
| `WZ_BUILD_RENDER_TESTS` | ON | `render_tests` (zrzuty scen) |
| `WZ_BUILD_BENCHMARKS` | ON | mikro-benchmarki rdzenia |
| `WZ_SANITIZE` | OFF | AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang; na MSVC tylko ASan) |

### Sandbox

```bash
./build/sandbox/sandbox                                                          # okno 1280×720, Esc zamyka
./build/sandbox/sandbox --width 1920 --height 1080                              # inny rozmiar okna
./build/sandbox/sandbox --headless --frames 120 --screenshot out.png            # bez okna, zrzut ostatniej klatki
```

`--headless` tworzy niewidoczne okno i renderuje offscreen. Działa na ręcznym zegarze (stałe 1/60 s na klatkę),
więc zrzut jest powtarzalny. Gdy nie podasz `--frames`, renderowana jest 1 klatka.

### Testy

```bash
ctest --test-dir build --output-on-failure            # wszystko (render wymaga GL, patrz niżej)
ctest --test-dir build -L unit --output-on-failure    # jednostkowe, bez GPU i bez okna
ctest --test-dir build -L render --output-on-failure  # render_tests + sandbox headless (potrzebne GL 4.5)
ctest --test-dir build -L bench                       # szybki (dymny) przebieg benchmarków
python3 scripts/check_headers.py                      # każdy plik ma nagłówek Wojcimierza
```

Wyniki `render_tests` trafiają do **`build/render_out/<scena>.png`**. Zrzuty sandboxa z testów
(`sandbox_headless.png` i `sandbox_window_srgb_blit.png`) lądują w tym samym katalogu.

### Benchmarki

```bash
./build/benchmarks/wz_benchmarks                                     # pełny przebieg (Release!)
./build/benchmarks/wz_benchmarks --quick --json benchmarks.json      # szybki przebieg + JSON
python3 scripts/compare_benchmarks.py stary.json benchmarks.json     # delta w % względem baseline'u
```

JSON zawiera średni i p95 czas CPU klatki (ms), szczytowe RSS (MB), liczbę klatek i wyniki mikro-benchmarków.
CI porównuje go z ostatnim wynikiem z `main`. Porównanie jest tylko informacyjne i nie blokuje PR.

### Build z sanitizerami (ASan + UBSan)

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DWZ_SANITIZE=ON -DWZ_BUILD_SANDBOX=OFF -DWZ_BUILD_RENDER_TESTS=OFF
cmake --build build-asan -j
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-asan -L "unit|bench" --output-on-failure
```

### Linux bez GPU / bez monitora (serwer, CI, kontener)

Wystarczy programowy renderer Mesa (llvmpipe, OpenGL 4.5) i wirtualny X przez `xvfb-run`:

```bash
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build -L render --output-on-failure
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/sandbox/sandbox --headless --frames 60 --screenshot out.png
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a glxinfo | grep "core profile version"
```

W wyniku `glxinfo` patrz na linię **`OpenGL core profile version string`** (Core Profile) – musi pokazywać
wersję **≥ 4.5**, np. `OpenGL core profile version string: 4.5 (Core Profile) Mesa 25.0.7`.
**Nie** sugeruj się pierwszą linią wyniku (`server glx version string: 1.4`) ani `OpenGL version string` (to profil zgodności – często zgłasza niższą wersję, np. 3.x
lub 4.x mniejszą niż core).

### Rozwiązywanie problemów

* **`Nie udalo sie utworzyc okna z kontekstem OpenGL 4.5 core`** albo **`Wymagany OpenGL 4.5, sterownik zglasza: ...`**:
  sterownik nie daje GL 4.5. Sprawdź `glxinfo | grep "core profile version"` (linia Core Profile, ≥ 4.5). Na maszynie wirtualnej, przez SSH lub na starym GPU użyj
  `LIBGL_ALWAYS_SOFTWARE=1` (llvmpipe z Mesa ≥ 20). Na macOS to oczekiwane (patrz Wymagania).
* **`[GLFW] blad 65550: X11: The DISPLAY environment variable is missing`** + `glfwInit() nie powiodlo sie`:
  brak serwera X. Uruchom przez `xvfb-run -a ...`. Dotyczy to także `--headless`, bo GLFW i tak potrzebuje
  wyświetlacza do kontekstu GL.
* **CMake: `Could NOT find X11` / `RandR headers not found` / `Xinerama headers not found`**:
  brakuje pakietów `-dev` X11. Zainstaluj listę z sekcji Wymagania i usuń katalog `build`.
* **`CMake 3.24 or higher is required`**: zaktualizuj CMake (`pip install cmake`).
* **Brak internetu (FetchContent)**: po pierwszej udanej konfiguracji źródła są w `build/_deps/`.
  Kolejne konfiguracje bez sieci przechodzą z `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`. Można też wskazać lokalne
  kopie, np. `-DFETCHCONTENT_SOURCE_DIR_GLFW=/sciezka/glfw` (analogicznie `GLM`, `STB`, `GOOGLETEST`).

## Testy – szczegóły

```bash
python3 scripts/check_headers.py --fix                         # dopisze brakujące nagłówki
```

### render_tests – sceny do porównywania zrzutów

`render_tests` to osobny cel CMake (zarejestrowany w ctest z etykietą `render`).
Renderuje offscreen nazwane sceny i zapisuje każdą klatkę do `build/render_out/<scena>.png`:

| Scena | Co sprawdza | Czas symulacji |
|---|---|---|
| `clear_color` | kolor tła kamery: liniowe (0.2, 0.4, 0.8) → sRGB (124,170,231) ±2 | 0 s |
| `triangle` | kolorowy trójkąt RGB (unlit) | 0 s |
| `rotating_cube` | sześcian obracany w `Update` | 0,5 s |
| `hierarchy` | rodzic-dziecko-wnuk, ruch w `FixedUpdate` | 1,0 s |
| `seeded_cubes` | 16 sześcianów z `std::mt19937(1337)` | 0,25 s |

Kolory: shadery liczą liniowo, cele renderu są `GL_SRGB8_ALPHA8` z `GL_FRAMEBUFFER_SRGB`, więc PNG zawierają
bajty sRGB (z chunkiem `sRGB` w pliku). Okno prosi o back buffer sRGB (`GLFW_SRGB_CAPABLE`); jeśli go nie dostanie,
silnik renderuje do wewnętrznego celu sRGB i kopiuje go do okna (log `[Wojcimierz] sRGB: ...`), więc okno i zrzuty
wyglądają identycznie. Test `sandbox_window_srgb_blit` (`--force-srgb-blit`) sprawdza tę ścieżkę.

Determinizm: stały rozmiar 256×256, stały krok 1/60 s, `ManualClock`, ziarno 1337.
Dwa uruchomienia na tym samym sterowniku dają identyczne pliki.

```bash
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/render_tests/render_tests              # wszystkie sceny
./build/render_tests/render_tests --list                                            # lista scen
./build/render_tests/render_tests --scene triangle --out /tmp/wyniki --size 256
./build/render_tests/render_tests --reference sciezka/do/wzorcow --threshold 2 --max-diff-ratio 0.001
```

Każda scena przechodzi przez jedną funkcję `compareWithGolden()` w `render_tests/main.cpp`.
To miejsce, w które wpina się komparator zespołu testów (np. zapis `<scena>_diff.png`).

### Testowalność

* **Zegar**: `IClock` → `SystemClock` (domyślny) i `ManualClock::Tick(dt)`. `GameLoop::Step()` i `Engine::SetClock()`
  używają wstrzykniętego zegara, więc testy przesuwają klatki ręcznie.
* **Wejście**: `IInputSource` → `GlfwInputSource` i `FakeInput` (programowe ustawianie klawiszy i myszy);
  `Input::SetSource()` / `Engine::SetInputSource()`.
* **Transform**: cache macierzy świata z flagą dirty, `GetChangeVersion()` rośnie przy każdej zmianie
  (również przodka), a `SetWorldPoseFromPhysics()` ustawia pozę bez podbijania wersji.

## CI (GitHub Actions)

`.github/workflows/ci.yml`. Każdy job ma `timeout-minutes`. Na gałęziach roboczych nowy push anuluje
poprzedni przebieg, a na `main` każdy przebieg dochodzi do końca.

1. `headers` (5 min): `scripts/check_headers.py`. To pierwszy job i reszta od niego zależy.
2. `linux` (20 min): build, testy `unit` i `render` pod xvfb + llvmpipe, zrzut sandboxa.
   Artefakt **`render_out`** to cały katalog `build/render_out/`.
3. `sanitizers` (30 min): Debug z ASan + UBSan, testy `unit` i `bench`.
4. `bench` (15 min): pełny przebieg `wz_benchmarks` w Release i artefakt **`benchmarks`** (`benchmarks.json`).
   Wynik jest porównywany z artefaktem ostatniego udanego przebiegu na `main` (`scripts/compare_benchmarks.py`,
   delta w % w logu i w podsumowaniu joba). Porównanie jest informacyjne i nie blokuje PR.
   Brak baseline'u oznacza tylko `notice`.
5. `windows` (30 min): build MSVC i testy `unit`.
6. `gpu` (20 min): self-hosted runner z GPU. Uruchamia się tylko przy pushu do `main`, dla PR z gałęzi tego repo
   albo ręcznie, i tylko gdy zmienna repo `GPU_RUNNER == 'true'` (nigdy nie uruchamia kodu z forków).

Cache `build/_deps` jest kluczowany hashem `cmake/Dependencies.cmake` i `CMakeLists.txt`.

## Zasady pracy

* `main` jest chroniony; zmiany tylko przez PR z gałęzi `feat/<moduł>-...`.
* Każdy plik `.h/.hpp/.cpp` zaczyna się nagłówkiem z linią „Na chwałę czcigodnego Wojcimierza”
  (`python3 scripts/check_headers.py --fix` dopisze go automatycznie).

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
