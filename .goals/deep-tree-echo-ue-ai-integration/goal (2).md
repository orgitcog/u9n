# Goal: Deep Tree Echo AI for Unreal Engine — Complete UE Integration Layer

## User Request

"deep tree echo ai for unreal engine" — make the Deep Tree Echo cognitive
architecture usable as AI inside the Unreal Engine project.

## Refined Goal

Complete the Unreal Engine integration layer so the Deep Tree Echo cognitive
architecture is a coherent, buildable UE 5.3 project and can act as the AI
brain of a Pawn. Today the `.uproject` declares a `DeepTreeEchoAvatar` module
that has no Build.cs or module implementation, the project has no Target.cs
files, `Source/DeepTreeEcho/DeepTreeEcho.Build.cs` omits ~23 of the 43
DeepTreeEcho subsystem directories (including all recently implemented tiers:
NanEcho, Persona, IonDevice, Embodied, LiveBridge, Level6/7/8, EchoML,
Echobeats, Membrane, etc.), and no AIController exists anywhere — nothing
connects `UDeepTreeEchoCore` perception→cognition→action to Unreal's AI
framework. Fix the module plumbing and add a Deep Tree Echo AI controller
layer whose pure decision logic is unit-testable in the standalone CMake
build.

## Acceptance Criteria

- [ ] Criterion 1 — Module consistency: every module named in
      `UnrealEngineCog.uproject` (`DeepTreeEcho`, `UnrealEcho`,
      `DeepTreeEchoAvatar`) has a `Source/<Name>/<Name>.Build.cs` and a module
      implementation pair (`<Name>.h`/`<Name>.cpp` with
      `IMPLEMENT_MODULE(F<Name>Module, <Name>)`). The `DeepTreeEchoAvatar`
      Build.cs must expose `UnrealEcho/DeepTreeEchoAvatar/Public` and compile
      `UnrealEcho/DeepTreeEchoAvatar/Private`, and depend on `DeepTreeEcho`
      and `UnrealEcho`.
- [ ] Criterion 2 — Project targets: `Source/UnrealEngineCog.Target.cs`
      (Game) and `Source/UnrealEngineCogEditor.Target.cs` (Editor) exist,
      follow UE 5.3 conventions (`DefaultBuildSettings =
      BuildSettingsVersion.V4`, `IncludeOrderVersion =
      EngineIncludeOrderVersion.Unreal5_3`), and list all three modules in
      `ExtraModuleNames`.
- [ ] Criterion 3 — Complete include coverage:
      `Source/DeepTreeEcho/DeepTreeEcho.Build.cs` includes every
      `DeepTreeEcho/` subdirectory that contains headers (all 42 non-Testing
      subdirectories: 4ECognition, ActiveInference, Attention, Avatar,
      Blueprint, Cognitive, CognitiveShell, Core, Cosmos, Echobeats, EchoML,
      Embodied, Emotion, Entelechy, Evolution, GameTraining, Goals,
      Integration, Introspection, IonDevice, Language, Learning, Level6,
      Level7, Level8, LiveBridge, Membrane, Memory, Metamodel, NanEcho,
      Neural, Persona, Planning, Reservoir, Sensorimotor, Sensory, Social,
      Sys6, System5, Taskflow, UnrealBridge, Wisdom).
- [ ] Criterion 4 — AI controller layer exists: a new
      `ADeepTreeEchoAIController` (subclass of `AAIController`) in
      `UnrealEcho/DeepTreeEchoAvatar/` (Public/Private pair, UCLASS with
      GENERATED_BODY, Blueprint-exposed properties/functions) that on
      possession finds-or-creates the pawn's `UDeepTreeEchoCore` component,
      feeds perception into the cognitive cycle each tick, and translates
      cognitive outputs into movement/focus actions. UE framework types not
      yet stubbed (e.g. `AIController.h`) must be added to the existing stub
      framework under `Source/DeepTreeEcho/` following its established
      patterns so the file set stays standalone-parseable.
- [ ] Criterion 5 — Testable decision core: the perception→action mapping
      logic is factored into a pure C++ (Eigen-only, no UE types) class
      `DeepTreeEcho/UnrealBridge/` or similar (e.g.
      `CognitiveActionArbiter`), with a `.h/.cpp` pair, covered by a new
      GTest unit test file in `DeepTreeEcho/Testing/UnitTests/` that is
      picked up by the root CMake glob and passes.
- [ ] Criterion 6 — Quality gate green: `cmake -S . -B build
      -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON` configures, `cmake
      --build build --config Release` compiles, and `ctest --output-on-failure
      -L unit -C Release` passes in the build directory (100% of unit-labeled
      tests, including the new ones).
- [ ] Criterion 7 — Documentation: a `Documentation/UE_AI_INTEGRATION.md`
      (or update to an existing integration doc) explaining the module layout,
      how to open the project in UE 5.3, and how to attach
      `ADeepTreeEchoAIController` to a character.

## Scope Boundaries

**In scope:**
- `Source/` module/target plumbing (Build.cs, Target.cs, module .h/.cpp)
- New AI controller code in `UnrealEcho/DeepTreeEchoAvatar/`
- New pure-logic decision class + unit tests under `DeepTreeEcho/`
- Minimal additions to the UE stub headers in `Source/DeepTreeEcho/`
  (e.g. an `AIController.h` stub) following existing stub patterns
- Documentation for the UE integration

**Out of scope:**
- Running or packaging inside an actual Unreal Editor / UBT build
  (UE 5.3 is not installed in this environment; verification is by the
  CMake+ctest gate and structural review)
- Modifying vendored engine code (`Engine/`), samples (`Samples/`),
  plugins (`Plugins/`), or `ThirdParty/`
- MetaHuman-DNA-Calibration, Live2D/Cubism, videosrc assets
- ReservoirEcho library internals
- Behavior Tree / Blackboard asset authoring (C++ layer only)
- Modifying existing DeepTreeEcho subsystem headers (API contract),
  except where a compile error in touched code demands a fix

**Security constraint (from SECURITY.md / issue #603):**
- Build.cs files must NOT add PreBuildSteps or PostBuildSteps.

## Applicable Project Conventions

**Quality gate command (matches CI job `unit-tests-deeptreeecho`):**
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release -j
cd build && ctest --output-on-failure --parallel 2 -L unit -C Release
```
CMake 4.3.4 is available locally. GTest is fetched automatically if absent.

**Commit convention:**
- Conventional commits: `type(scope): [B/I] description` (≤72 chars)
- Prior art: `feat(deep-tree-echo): [B] implement tiers 7-9 subsystems`
- Assisted-by trailer required: `Assisted-by: Claude:Sonnet-4.6` (Builder)
  / `Assisted-by: Claude:Haiku-4.5` (Inspector)
- Also include: `Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>`

**Guidelines:**
- CLAUDE.md at repo root — architecture spec (12-step cycle, 3 streams,
  4E cognition, UCLASS/USTRUCT conventions)
- SECURITY.md — no high-privileged build callbacks in Build.cs
- No AGENTS.md or CONSTITUTION.md present

**Rules:**
- C++17; Eigen for pure math; UE 5.3 UCLASS/USTRUCT patterns for UE code
- Each .cpp includes its own header first
- New unit tests must be pure C++ (no UE types) or they will break the
  root CMakeLists glob build — UE-dependent tests are excluded by regex
  there, so name any UE-dependent test to match existing exclusions or
  keep tests UE-free (preferred)

## Reference Implementations

- `Source/UnrealEcho/UnrealEcho.{h,cpp,Build.cs}` — module pattern to copy
- `Source/DeepTreeEcho/DeepTreeEcho.{h,cpp,Build.cs}` — module + stub host
- `Source/DeepTreeEcho/CoreMinimal.h`, `Source/DeepTreeEcho/GameFramework/*.h`,
  `Source/DeepTreeEcho/BehaviorTree/BlackboardComponent.h`,
  `Source/DeepTreeEcho/Perception/AIPerceptionComponent.h` — stub patterns
- `DeepTreeEcho/Core/DeepTreeEchoCore.h` — the cognitive core UActorComponent
  (UCLASS, BlueprintCallable API: cycle, 4E, relevance, memory, body schema)
- `UnrealEcho/DeepTreeEchoAvatar/Public/DeepTreeEchoAvatarComponent.h` —
  existing avatar-side integration component
- `DeepTreeEcho/Testing/UnitTests/CoreTests.cpp` — GTest unit test pattern
