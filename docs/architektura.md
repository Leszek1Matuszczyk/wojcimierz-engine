# Architektura Wojcimierz Engine

Na chwałę czcigodnego Wojcimierza.

## Warstwy

```
 sandbox / gry / render_tests
          │
      wz_engine  ── Engine, Window (GLFW), GlfwInputSource, Renderer, Shader, Mesh, Image
          │           └── RHI (render/RHI.h) ── gl/GLDevice (OpenGL 4.5 core, DSA)
       wz_core   ── GameLoop, Time, IClock, Scene, GameObject, Component, Transform,
                    Input/IInputSource/FakeInput, Camera, MeshData   (tylko glm, bez GPU)
```

## Pętla gry

`GameLoop::Tick(dt)` (albo `Step()` z wstrzykniętym zegarem):

1. sanityzacja `dt`: NaN/ujemne → 0, +inf/długie → `maximumDeltaTime` (0,25 s); ujemny `Time::timeScale` → 0,
2. akumulator += `dt * timeScale`; `FixedUpdate` wykonuje się tyle razy, ile mieści się `fixedDeltaTime` (1/60 s),
3. `Update(dt)`, `LateUpdate(dt)` (na końcu niszczone są obiekty po `Scene::Destroy`),
4. `render(alpha)`, gdzie `alpha ∈ [0,1)` to interpolacja między krokami stałymi.

`Time` (statyczne, jak `UnityEngine.Time`): `time`, `deltaTime`, `fixedDeltaTime`, `timeScale`, `frameCount`, ...

## Scena i komponenty

* `Scene` posiada `GameObject`y (`CreateGameObject`, `Destroy`, `Find`, `ForEach<T>`).
* `GameObject` ma zawsze `Transform` + dowolne komponenty (`AddComponent<T>`, `GetComponent<T>`).
* `Component` jest bazą (zespół wyprowadzi z niej `Behaviour`), z hookami `Awake/Start/FixedUpdate/Update/LateUpdate/OnDestroy`.
  `Start` wołane jest raz przed pierwszym `FixedUpdate`/`Update`, a dla wyłączonych komponentów odkładane.
  Komponenty dodane w trakcie klatki dostają zdarzenia od następnej.

## Transform

* Lokalne TRS (`vec3`, `quat`, `vec3`), hierarchia rodzic–dzieci, `SetParent(p, worldPositionStays)` z ochroną przed cyklami.
* Układ prawoskrętny (OpenGL), +Y w górę, przód obiektu to −Z (`Forward()`).
* Cache `GetLocalToWorldMatrix()` z flagą dirty ustawianą na całym poddrzewie.
* `GetChangeVersion()` rośnie przy każdej zmianie lokalnej i propaguje się na potomków;
  `SetWorldPoseFromPhysics()` zmienia pozę bez podbijania wersji (zapis wyników fizyki).

## RHI

`rhi::Device` udostępnia bufory, shadery (GLSL 450), wejście wierzchołków, render targety offscreen,
przebiegi (`BeginPass`/`EndPass`), uniformy, `DrawIndexed` i `ReadPixels`. Reszta silnika nie zna OpenGL,
więc backend Vulkan wymaga tylko nowej implementacji `Device` (shadery GLSL 450 → SPIR-V).
