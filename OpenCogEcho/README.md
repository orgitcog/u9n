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
| `endocrine` | Virtual endocrine system: hormone bus, glands, valence/affect/moral layers (bio-cognitive layer) |
| `nervous` | Virtual nervous system: nerve bus, brain nuclei (bio-cognitive layer) |
| `entelechy` | Ontogenetic self-model: personality, development, Civic Angel (bio-cognitive layer) |
| `afi` | Active Free-energy Inference: precision-weighted free energy minimization (bio-cognitive layer) |
| `temporal` | Temporal crystal bus and temporal system (bio-cognitive layer) |

## Layout

```
OpenCogEcho/
├── include/opencog/{core,atomspace,attention,pattern,pln,ure,atomese}/
├── include/opencog/endocrine/{glands,adapters...}/    # bio-cognitive
├── include/opencog/nervous/nuclei/                    # bio-cognitive
├── include/opencog/{entelechy,afi,temporal}/           # bio-cognitive
├── src/{core,atomspace,attention,pattern,pln,ure,atomese}/
├── src/{endocrine,nervous,entelechy,afi,temporal}/     # bio-cognitive
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

## Bio-Cognitive Layer

Ported from the same **9-o9/u9** source as a second integration layer on top
of the core AtomSpace/PLN/URE stack above. These modules give the AtomSpace a
"body": affective/hormonal regulation, brainstem-style signal routing,
ontogenetic self-modeling, active-inference free-energy minimization, and a
temporal synchronization substrate. `AtomType` reserves dedicated ranges for
each (endocrine 10000+, nervous 10100+, temporal 10200+, entelechy 10300+,
AFI 10320+) so the layer is purely additive to the core type system.

| Module | Contents |
|--------|----------|
| **endocrine** | `HormoneBus` (32-channel SIMD-decayed concentration bus) + 10 virtual glands (`glands/`: HPA axis, dopaminergic, serotonergic, noradrenergic, oxytocinergic, thyroid, circadian, pancreatic, immune, endocannabinoid) driving valence memory, affective integration, and moral perception. Adapters bridge hormone state to Marduk, NPU, o9c2, and VirtualTouchpad subsystems. |
| **nervous** | `NerveBus` + `NervousSystem` routing signals through 10 brain nuclei (`nuclei/`: thalamus, hypothalamus, amygdala, hippocampus, basal ganglia, prefrontal cortex, brainstem autonomic, cerebellum, anterior cingulate, insula). |
| **entelechy** | Ontogenetic self-model: Cloninger temperament, developmental trajectory (with trauma encoding/healing), interoceptive model, narrative identity, social self, and the `CivicAngel` city-wide observer/free-energy aggregator across cognitive districts. |
| **afi** | Active Free-energy Inference: precision-weighted free energy computation and Markov-blanket boundary modeling for active inference. |
| **temporal** | `TemporalCrystalBus` + `TemporalSystem`: a phase-based temporal synchronization substrate for coordinating rhythmic/periodic cognitive processes. |

### DeepTreeEcho Integration Notes

These bio-cognitive modules are conceptual C++23/AtomSpace analogues of
existing Unreal-side `DeepTreeEcho/` components. They are not yet wired
together (no UE code changes were made here) — mapping intent for a future
integration pass:

| OpenCogEcho module | DeepTreeEcho counterpart | Relationship |
|---|---|---|
| `endocrine` | `DeepTreeEcho/Emotion` + neurochemical simulation concepts | Both model affect via hormone/neurochemical concentrations with decay and feedback; `endocrine`'s `HormoneBus` could back the neurochemical state driving `DeepTreeEcho/Emotion`'s expression outputs. |
| `afi` | `DeepTreeEcho/ActiveInference` | Both implement predictive-processing / free-energy minimization; `afi`'s precision-weighted free energy could supply the AtomSpace-grounded belief updates that `ActiveInference` consumes for avatar behavior selection. |
| `entelechy` | `DeepTreeEcho/Entelechy` | Both target goal-directed, ontogenetic behavior; `entelechy`'s developmental trajectory/Civic Angel self-model is a natural AtomSpace-backed foundation for `DeepTreeEcho/Entelechy`'s goal-directed behavior layer. |
| `temporal` | Echobeats (12-step cognitive cycle / triadic stream synchronization) | `TemporalCrystalBus`'s phase-based synchronization substrate maps naturally onto Echobeats' 3-stream, 12-step triadic cycle described in `CLAUDE.md`. |

## Notes

- Include paths follow the upstream convention: `#include <opencog/core/types.hpp>` etc.
- `tests/test_integration.cpp` targets the layer-2 bio-cognitive modules
  (endocrine/nervous/entelechy/afi/temporal) and is compiled automatically
  once `include/opencog/endocrine/` lands.
- `AtomType` reserves ranges for the layer-2 systems (endocrine, nervous,
  temporal crystal, entelechy, active free-energy inference) so the follow-up
  integration is additive.
