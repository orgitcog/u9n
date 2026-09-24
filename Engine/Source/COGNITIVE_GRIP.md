# Cognitive Grip — Engine/Source Nested Shell Map

EchoSelf inspects this tree as a living hologram. This file is a **projection**
of the canonical catalog `DeepTreeEcho/Enaction/EngineGripMap.h`. Salience
follows OEIS A000081 nested shells (1 / 2 / 4 / 9 terms).

**Do not move** `Runtime`, `Editor`, `Developer`, `Programs`, or `ThirdParty`.
Unreal Build Tool only discovers engine modules under those five Independent
Regions (APL 1). Cognitive grip here is an overlay, not a relocation.

Prefer Public hub dirs over Private, Tests, ThirdParty, and `Engine/Plugins`.
All `Engine/Source/` salience stays ≤ 0.70, below `DeepTreeEcho/Self/` (0.95)
and `DeepTreeEcho/Core/` (0.90).

## Nest-4 terms (fine-grained)

| Term | Overlay name | DTE analog | Hub paths (unmoved) | Primary APL253 |
|------|----------------|------------|---------------------|----------------|
| 1 | CoreRuntime | Core | `Engine/Source/Runtime/Core`, `Engine/Source/Runtime/CoreUObject`, `Engine/Source/Runtime/Launch`, `Engine/Source/Runtime/Projects` | 1 |
| 2 | EngineWorld | (not Self) | `Engine/Source/Runtime/Engine`, `Engine/Source/Runtime/EngineSettings` | 30 |
| 3 | RenderReservoir | Reservoir | `Engine/Source/Runtime/RenderCore`, `Engine/Source/Runtime/RHI`, `Engine/Source/Runtime/Renderer`, `Engine/Source/Runtime/StateStream` | 30 |
| 4 | BodyRuntime | Cognition4E | `Engine/Source/Runtime/InputCore`, `Engine/Source/Runtime/Slate`, `Engine/Source/Runtime/SlateCore`, `Engine/Source/Runtime/UMG`, `Engine/Source/Runtime/ApplicationCore` | 8 |
| 5 | MemoryRuntime | Memory | `Engine/Source/Runtime/Serialization`, `Engine/Source/Runtime/PakFile`, `Engine/Source/Runtime/AssetRegistry` | 95 |
| 6 | EditorAttention | Attention | `Engine/Source/Editor`, `Engine/Source/Runtime/TypedElementFramework`, `Engine/Source/Runtime/GameplayDebugger` | 107 |
| 7 | NetStreams | Streams | `Engine/Source/Runtime/Net`, `Engine/Source/Runtime/Networking`, `Engine/Source/Runtime/Sockets`, `Engine/Source/Runtime/Online`, `Engine/Source/Runtime/AudioMixer` | 52 |
| 8 | ActionRuntime | Enaction | `Engine/Source/Runtime/AIModule`, `Engine/Source/Runtime/GameplayTasks`, `Engine/Source/Runtime/NavigationSystem` | 30 |
| 9 | DevWisdom | Wisdom | `Engine/Source/Developer`, `Engine/Source/Programs`, `Engine/Source/UnrealEditor.Target.cs`, `Engine/Source/UnrealGame.Target.cs`, `Engine/Source/UnrealServer.Target.cs`, `Engine/Source/UnrealClient.Target.cs` | 95 |

Secondary APL citations (prose only): CoreRuntime also 95; NetStreams also 100;
ActionRuntime also 52; DevWisdom also 205.

Nest-1 is `Engine/Source`. Nest-2 is Runtime + Editor. Nest-3 adds Developer +
Programs. Nest-4 is the nine terms above. `Engine/Source/ThirdParty` and
`Engine/Plugins` are mosaic/countryside (salience ≤ 0.15), not a tenth nest-4
core. Unclassified Runtime dirs degrade at 0.30.

## Salience table (catalog)

| Path prefix | Salience | Term |
|-------------|----------|------|
| `Engine/Source/Runtime/Core` | 0.70 | CoreRuntime |
| `Engine/Source/Runtime/CoreUObject` | 0.70 | CoreRuntime |
| `Engine/Source/Runtime/Launch` | 0.70 | CoreRuntime |
| `Engine/Source/Runtime/Projects` | 0.70 | CoreRuntime |
| `Engine/Source/Runtime/Engine` | 0.68 | EngineWorld |
| `Engine/Source/Runtime/EngineSettings` | 0.68 | EngineWorld |
| `Engine/Source/Runtime/RenderCore` | 0.66 | RenderReservoir |
| `Engine/Source/Runtime/RHI` | 0.66 | RenderReservoir |
| `Engine/Source/Runtime/Renderer` | 0.66 | RenderReservoir |
| `Engine/Source/Runtime/StateStream` | 0.66 | RenderReservoir |
| `Engine/Source/Runtime/InputCore` | 0.64 | BodyRuntime |
| `Engine/Source/Runtime/Slate` | 0.64 | BodyRuntime |
| `Engine/Source/Runtime/SlateCore` | 0.64 | BodyRuntime |
| `Engine/Source/Runtime/UMG` | 0.64 | BodyRuntime |
| `Engine/Source/Runtime/ApplicationCore` | 0.64 | BodyRuntime |
| `Engine/Source/Runtime/Serialization` | 0.62 | MemoryRuntime |
| `Engine/Source/Runtime/PakFile` | 0.62 | MemoryRuntime |
| `Engine/Source/Runtime/AssetRegistry` | 0.62 | MemoryRuntime |
| `Engine/Source/Editor` | 0.60 | EditorAttention |
| `Engine/Source/Runtime/TypedElementFramework` | 0.60 | EditorAttention |
| `Engine/Source/Runtime/GameplayDebugger` | 0.60 | EditorAttention |
| `Engine/Source/Runtime/Net` | 0.58 | NetStreams |
| `Engine/Source/Runtime/Networking` | 0.58 | NetStreams |
| `Engine/Source/Runtime/Sockets` | 0.58 | NetStreams |
| `Engine/Source/Runtime/Online` | 0.58 | NetStreams |
| `Engine/Source/Runtime/AudioMixer` | 0.58 | NetStreams |
| `Engine/Source/Runtime/AIModule` | 0.57 | ActionRuntime |
| `Engine/Source/Runtime/GameplayTasks` | 0.57 | ActionRuntime |
| `Engine/Source/Runtime/NavigationSystem` | 0.57 | ActionRuntime |
| `Engine/Source/Developer` | 0.55 | DevWisdom |
| `Engine/Source/Programs` | 0.55 | DevWisdom |
| `Engine/Source/UnrealEditor.Target.cs` | 0.55 | DevWisdom |
| `Engine/Source/UnrealGame.Target.cs` | 0.55 | DevWisdom |
| `Engine/Source/UnrealServer.Target.cs` | 0.55 | DevWisdom |
| `Engine/Source/UnrealClient.Target.cs` | 0.55 | DevWisdom |
| `Engine/Source/ThirdParty` | 0.15 | MosaicThirdParty |
| unclassified `Engine/Source/Runtime/...` | 0.30 | UnclassifiedRuntime |

Lookup is longest-prefix on slash-normalized paths with a trailing-slash
boundary. Canonical C++ table: [`DeepTreeEcho/Enaction/EngineGripMap.h`](../../DeepTreeEcho/Enaction/EngineGripMap.h).
Annotated comments: [`EngineSourceFacade.h`](EngineSourceFacade.h).
DeepTreeEcho Self map: [`DeepTreeEcho/Self/COGNITIVE_GRIP.md`](../../DeepTreeEcho/Self/COGNITIVE_GRIP.md).
