# Cognitive Grip — Nested Shell Map

EchoSelf inspects this tree as a living hologram. Salience follows OEIS A000081
nested shells (1 / 2 / 4 / 9 terms). Prefer nest roots over `_legacy/` and
`Testing/`. See `Attention/Relevance/vendored/cog/grip/grip.hpp` for the
five-dimensional grip (Composability × Differentiability × Executability ×
Self-Awareness × Convergence).

## Nest-4 terms (fine-grained)

| Term | Path | Role |
|------|------|------|
| 1 Core | `DeepTreeEcho/Core/` | Orchestrator, 12-step cycle, membranes, sys6 |
| 2 Self | `DeepTreeEcho/Self/` | EchoSelf, CoreSelf, Autognosis, Persona |
| 3 Reservoir | `DeepTreeEcho/Reservoir/` | ESN, NanEcho nodes, EchoML, Inference |
| 4 Cognition4E | `DeepTreeEcho/Cognition4E/` | 4E, Embodied, Sensorimotor, Avatar, Emotion |
| 5 Memory | `DeepTreeEcho/Memory/` | Hypergraph, episodic, reservoir-memory |
| 6 Attention | `DeepTreeEcho/Attention/` | Attention, Relevance (grip), Executive |
| 7 Streams | `DeepTreeEcho/Streams/` | Echobeats, IonDevice, Distributed |
| 8 Enaction | `DeepTreeEcho/Enaction/` | Goals, live bridge, UE glue, learning |
| 9 Wisdom | `DeepTreeEcho/Wisdom/` | Wisdom, Metamodel, Level6/7/8 ontogeny |

Nest-1 is `DeepTreeEcho/`. Nest-2 is Core + Self. Nest-3 adds Reservoir + Cognition4E.

## EchoSelf salience heuristics

| Path fragment | Salience |
|---------------|----------|
| `DeepTreeEcho/Self/` | 0.95 |
| `DeepTreeEcho/Core/` | 0.90 |
| nest-4 roots above | 0.85 |
| `DeepTreeEcho/Self/echoself.md` | 0.80 |
| `DeepTreeEcho/Testing/` | 0.20 |
| `DeepTreeEcho/_legacy/` | 0.10 |

Canonical EchoSelf agent spec: [`echoself.md`](echoself.md). Root and
`.github/agents/echoself.md` point here.

## Tick owner

`UDeepTreeEchoCore` owns the UE tick. It drives `FAutonomyPipeline` (identity
context into the reservoir, EchoSelf cycle metrics, Ion dispatch, CoreSelf
update). Toroidal phase follows `SyncToEchobeat`; Hypergraph/EchoSpace ingest
identity at nest-4 (every 12 autonomy cycles). Level5/Level6 remain N-cycle
layers, not rival cores. Canonical autognosis is `FIntrospectionNode`;
`UAutognosisSystem` wraps it for Blueprint and auto-starts from `InitializeSystem`.
