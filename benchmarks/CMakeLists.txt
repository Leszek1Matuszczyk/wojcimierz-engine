add_executable(wz_benchmarks bench_main.cpp)
target_link_libraries(wz_benchmarks PRIVATE wz_core)
if(MSVC)
  target_compile_options(wz_benchmarks PRIVATE /utf-8)
endif()
# Szybki przebieg jako test dymny (etykieta "bench"); pelne pomiary: ./wz_benchmarks
add_test(NAME benchmarks_smoke COMMAND wz_benchmarks --quick)
set_tests_properties(benchmarks_smoke PROPERTIES LABELS "bench" TIMEOUT 120)
