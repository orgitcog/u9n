# OpenCogEcho

Modern OpenCog core for the Deep Tree Echo cognitive framework.

Ported from the **9-o9/u9** repository's "Modern OpenCog" stack — a reimagined
implementation of OpenCog's cognitive architecture using modern C++23
techniques (the legacy cogutil/atomspace/cogserver stack was intentionally
**not** ported).

## Components

| Module | Contents |
|--------|----------|
| `core` | Compact 8-byte `TruthValue` / `AttentionValue`, `AtomId` with generation tracking, concepts, pool/arena allocators, SIMD-aligned vectors |
| `atomspace` | Structure-of-Arrays atom storage, O(1) hash lookup, type & target indices |
| `attention` | Lock-free ECAN attention bank with spreading activation |
| `pattern` | C++20 coroutine-based lazy pattern matcher and generators |
| `pln` | Probabilistic Logic Networks: scalar + SIMD (AVX2) batch formulas, inference engine |
| `ure` | Unified Rule Engine: forward/backward chaining |
| `atomese` | Atomese S-expression parser and AtomSpace loader |

## Layout

```
OpenCogEcho/
├── include/opencog/{core,atomspace,attention,pattern,pln,ure,atomese}/
├── src/{core,atomspace,attention,pattern,pln,ure,atomese}/
├── tests/          # lightweight self-registering test harness (ctest: OpenCogEchoTests)
├── benchmarks/     # micro-benchmarks (built with -DBUILD_BENCHMARKS=ON)
└── CMakeLists.txt  # builds the opencog_echo static library (C++23, C++20 fallback)
```

## Building

Built as part of the root `un9n` CMake project:

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build --target opencog_echo opencog_echo_tests
ctest --test-dir build -R OpenCogEchoTests
```

The `opencog_echo` target requires a C++23-capable compiler (GCC 13+,
Clang 16+, MSVC 2022 17.6+); it falls back to C++20 when cxx_std_23 is not
advertised. The rest of the repository stays on C++17.

## Notes

- Include paths follow the upstream convention: `#include <opencog/core/types.hpp>` etc.
- `tests/test_integration.cpp` targets the layer-2 bio-cognitive modules
  (endocrine/nervous/entelechy/afi/temporal) and is compiled automatically
  once `include/opencog/endocrine/` lands.
- `AtomType` reserves ranges for the layer-2 systems (endocrine, nervous,
  temporal crystal, entelechy, active free-energy inference) so the follow-up
  integration is additive.
