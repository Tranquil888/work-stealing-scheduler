# wss — Work-Stealing Scheduler

A C++20 async runtime: work-stealing thread pool, futures with continuations,
cooperative cancellation, timers, and a Lean 4 model of the core safety invariants.

No CUDA. No third-party runtime dependencies — only the standard library.
Builds with CMake + GCC/Clang/MSVC.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Debug with sanitizers:

```sh
cmake -S . -B build -DWSS_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Lean proofs (optional, not needed to build or test the C++):

```sh
cd lean && lake build
```

`lake` is not required for any C++ target. The `lean/` directory is a
self-contained Lean project; CMake never invokes it.

## Use

```cpp
#include <wss/scheduler.hpp>

wss::Scheduler pool(4);

auto fut = pool.submit([] { return 42; });
int v = fut.get();  // 42

wss::CancellationSource src;
pool.submit([tok = src.token()] {
    for (int i = 0; i < 10; ++i) {
        if (tok.is_cancelled()) return;
    }
}, tok);
src.cancel();

pool.schedule_after(std::chrono::milliseconds(100), [] { /* ... */ });
pool.schedule_every(std::chrono::seconds(1), [] { /* ... */ });
```

See `examples/demo.cpp` for a full end-to-end program.

## Benchmarks

`wss_throughput` (tasks/sec, higher is better) and `wss_latency`
(submit-to-complete, lower is better). Indicative numbers, GCC 15 Debug on
8-core Windows, uniform load:

```
queue    workers       tiny/s     medium/s
mutex    1             568032       274523
mutex    2             417544       572915
mutex    4             302450       250345
mutex    8             276047       139082
chaselv  1             565881       258990
chaselv  2             407204       570757
chaselv  4             273669       386608
chaselv  8             315924       154955
```

```
n=20000 mean=928.6us p50=538.9us p99=13769.5us max=14803.7us
```

Read: uniform tiny tasks are submitter-bound, so both queues tie. Medium
tasks at 4 workers is where stealing pays (uneven completion times leave
work stranded on some workers): Chase–Lev does ~1.5x the mutex queue
there. Past 4 workers the shared submitter and the result collection
dominate again. A bursty or recursive workload would tilt further toward
stealing; the harness above is deliberately the boring uniform case.

## Layout

- `include/wss/` — public headers (interfaces + value types)
- `src/` — implementations, one translation unit per header
- `tests/` — behaviour tests, one executable per file, run under CTest
- `benchmarks/` — throughput and p50/p99 latency harnesses
- `examples/` — end-to-end demo
- `lean/` — Lean 4 model: state machine + safety proofs
- `docs/` — architecture, design decisions, Lean model notes

## Roadmap

0. Scaffolding and design docs
1. Task abstraction + thread pool on a global queue (baseline)
2. Chase–Lev deques + victim policies (work stealing)
3. Futures, promises, `then` continuations
4. Cooperative cancellation
5. Timers (`schedule_after` / `schedule_every`)
6. Lean 4 safety model + runtime invariant checker
7. Benchmarks, demo, polish

Phase 8 (CUDA offload) is deliberately out of scope for this tree.
