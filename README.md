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
