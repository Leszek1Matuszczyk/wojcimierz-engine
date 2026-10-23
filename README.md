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

## Budowanie

Linux (Debian/Ubuntu):

```bash
sudo apt-get install build-essential cmake ninja-build pkg-config \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev \
  libgl1-mesa-dev libgl1-mesa-dri mesa-utils xvfb
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Windows (Visual Studio 2022): `cmake -S . -B build -A x64 && cmake --build build --config Release`.

Opcje CMake: `WZ_BUILD_TESTS`, `WZ_BUILD_SANDBOX`, `WZ_BUILD_RENDER_TESTS`, `WZ_BUILD_BENCHMARKS` (domyślnie ON),
`WZ_SANITIZE` (ASan + UBSan, domyślnie OFF).

## Uruchamianie

```bash
./build/sandbox/sandbox                      # okno, Esc zamyka
./build/sandbox/sandbox --headless --frames 60 --screenshot out.png   # bez okna, zrzut PNG
```

Tryb `--headless` tworzy niewidoczne okno GLFW, renderuje do bufora offscreen i używa
ręcznego zegara (stałe 1/60 s na klatkę), więc wynik jest powtarzalny.
Bez GPU działa na Mesa llvmpipe:

```bash
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/sandbox/sandbox --headless --frames 60 --screenshot out.png
```

## Testy

```bash
ctest --test-dir build -L unit --output-on-failure            # jednostkowe (bez GPU)
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build -L render --output-on-failure
ctest --test-dir build -L bench                                # szybki przebieg benchmarków
python3 scripts/check_headers.py                               # nagłówki plików
```

### render_tests – sceny do porównywania zrzutów

`render_tests` to osobny cel CMake (zarejestrowany w ctest z etykietą `render`).
Renderuje offscreen nazwane sceny i zapisuje każdą klatkę do `build/render_out/<scena>.png`:

| Scena | Co sprawdza | Czas symulacji |
|---|---|---|
| `clear_color` | kolor tła kamery (każdy piksel = 51,102,204) | 0 s |
| `triangle` | kolorowy trójkąt RGB (unlit) | 0 s |
| `rotating_cube` | sześcian obracany w `Update` | 0,5 s |
| `hierarchy` | rodzic-dziecko-wnuk, ruch w `FixedUpdate` | 1,0 s |
| `seeded_cubes` | 16 sześcianów z `std::mt19937(1337)` | 0,25 s |

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

`.github/workflows/ci.yml`:

1. `headers` – `scripts/check_headers.py` (pierwszy job, reszta od niego zależy),
2. `linux` – build, testy `unit` i `render` pod xvfb + llvmpipe, zrzut sandboxa, benchmarki;
   artefakt **`render_out`** (cały katalog `build/render_out/`),
3. `sanitizers` – Debug z ASan + UBSan, testy `unit` i `bench`,
4. `windows` – build MSVC i testy `unit`,
5. `gpu` – self-hosted runner z GPU; tylko push do `main`, PR z gałęzi tego repo albo ręcznie,
   i tylko gdy zmienna repo `GPU_RUNNER == 'true'` (nigdy kod z forków).

Cache `build/_deps` jest kluczowany hashem `cmake/Dependencies.cmake` i `CMakeLists.txt`.

## Zasady pracy

* `main` jest chroniony; zmiany tylko przez PR z gałęzi `feat/<moduł>-...`.
* Każdy plik `.h/.hpp/.cpp` zaczyna się nagłówkiem z linią „Na chwałę czcigodnego Wojcimierza”
  (`python3 scripts/check_headers.py --fix` dopisze go automatycznie).
