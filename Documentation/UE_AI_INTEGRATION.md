# UE AI Integration — Deep Tree Echo

## Overview

This document describes the Unreal Engine 5.3 integration layer for the Deep Tree
Echo cognitive architecture.  After completing this integration the project is a
coherent, buildable UE 5.3 project in which any Pawn can use
`ADeepTreeEchoAIController` as its AI brain.

---

## Module Layout

```
UnrealEngineCog/
├── Source/
│   ├── DeepTreeEcho/           ← Cognitive core module (UBT Build.cs)
│   │   └── DeepTreeEcho.Build.cs   all 42 DeepTreeEcho subdirs on include path
│   ├── UnrealEcho/             ← Avatar / animation module (UBT Build.cs)
│   │   └── UnrealEcho.Build.cs
│   ├── DeepTreeEchoAvatar/     ← NEW: AI-controller module (UBT Build.cs)
│   │   ├── DeepTreeEchoAvatar.Build.cs
│   │   ├── DeepTreeEchoAvatar.h
│   │   └── DeepTreeEchoAvatar.cpp
│   ├── UnrealEngineCog.Target.cs      ← NEW: Game target
│   └── UnrealEngineCogEditor.Target.cs ← NEW: Editor target
│
├── UnrealEcho/
│   └── DeepTreeEchoAvatar/
│       ├── Public/
│       │   ├── DeepTreeEchoAvatarComponent.h  (existing)
│       │   └── DeepTreeEchoAIController.h     ← NEW
│       └── Private/
│           ├── DeepTreeEchoAvatarComponent.cpp (existing)
│           └── DeepTreeEchoAIController.cpp    ← NEW
│
├── DeepTreeEcho/
│   ├── Core/
│   │   └── DeepTreeEchoCore.h   (UDeepTreeEchoCore UActorComponent)
│   └── UnrealBridge/
│       ├── CognitiveActionArbiter.h   ← NEW (pure C++/Eigen)
│       └── CognitiveActionArbiter.cpp ← NEW
│
└── Source/DeepTreeEcho/
    └── AIController/
        └── AIController.h  ← NEW stub for standalone compilation
```

---

## Opening the Project in Unreal Engine 5.3

1. Install **Unreal Engine 5.3** via the Epic Games Launcher.
2. Double-click `UnrealEngineCog.uproject` in the repo root.
3. UE will prompt to rebuild missing modules — click **Yes**.  UBT will compile
   `DeepTreeEcho`, `UnrealEcho`, and `DeepTreeEchoAvatar` automatically.
4. The project opens in the Unreal Editor.

> **Note**: The `Engine/` directory in this repository is a reference copy of
> engine stubs.  Do **not** modify files inside it.

### Prerequisites

| Requirement | Version |
|---|---|
| Unreal Engine | 5.3.x |
| Visual Studio  | 2022 (Windows) |
| .NET SDK        | ≥ 6.0 (for UBT) |
| CMake           | ≥ 3.14 (standalone build gate only) |

---

## Three Modules at a Glance

### `DeepTreeEcho`

*Runtime module — pure cognitive architecture.*

- 12-step cognitive cycle, 3 concurrent consciousness streams
- Echo State Networks (reservoir computing)
- 4E Embodied Cognition (Embodied, Embedded, Enacted, Extended)
- Hypergraph memory (declarative, procedural, episodic, intentional)
- Relevance realization, gestalt processing

**Include path**: all 42 subdirectories in `DeepTreeEcho/` (see `Build.cs`).

### `UnrealEcho`

*Runtime module — avatar & animation layer.*

- Live2D / Cubism 2D avatar integration
- MetaHuman 3D avatar support
- Neurochemical simulation mapped to animation parameters
- Character personality system

### `DeepTreeEchoAvatar`

*Runtime module — AI controller layer.*

- `ADeepTreeEchoAIController` wires UDeepTreeEchoCore to UE's AI framework
- Perception → cognition → navigation/focus pipeline
- Blueprint-exposable configuration and runtime state

---

## Attaching `ADeepTreeEchoAIController` to a Character

### Step 1 — Add UDeepTreeEchoCore to the Character

In the Character Blueprint's **Components** panel, click **+ Add** and search for
`DeepTreeEchoCore`.  Add the component and configure:

| Property | Recommended value |
|---|---|
| Enable Cognitive Processing | ✓ |
| Enable 4E Cognition | ✓ |
| Enable Relevance Realization | ✓ |
| Cycle Duration | 12.0 |

### Step 2 — Set the AI Controller class

In the Character Blueprint's **Class Defaults**, find **AI Controller Class** and
set it to `ADeepTreeEchoAIController`.

### Step 3 — Configure the controller

Open the `ADeepTreeEchoAIController` defaults (or use a child blueprint):

| Property | Default | Description |
|---|---|---|
| Cognitive Tick Rate | 10 Hz | How often the cognitive cycle is ticked |
| Movement Salience Threshold | 0.6 | Salience required to trigger movement |
| Focus Salience Threshold | 0.4 | Salience required to shift focus |
| Navigation Acceptance Radius | 100 cm | Arrival distance for MoveToActor |
| Debug Logging | false | Logs cycle step & action vector each tick |

### Step 4 — Place the character in the level

Drag the character into the level.  Press **Play** — the controller will:

1. Possess the pawn → find or warn about `UDeepTreeEchoCore` (initializing it
   only if its own `BeginPlay` has not already done so).
2. On each cognitive tick, gather perceived actors via
   `UAIPerceptionComponent`, build a salience vector (caching the perceived
   actors index-aligned), and call `CognitiveCore->ProcessSensoryInput(...)`.
3. Run `CognitiveActionArbiter` over the salience vector and current cognitive
   mode to obtain `[movement_urgency, focus_urgency, best_target_index]`.
4. Resolve the target index against the perceived-actor cache and call
   `MoveToActor` / `SetFocus` when the urgencies exceed the configured
   thresholds (movement stops and focus clears as soon as they are no longer
   warranted).  A manual `SetMoveTarget(...)` call pins both movement and
   gaze to that target — suspending autonomous commands — until cleared with
   `SetMoveTarget(nullptr)`.

---

## The `CognitiveActionArbiter` (pure C++)

`DeepTreeEcho/UnrealBridge/CognitiveActionArbiter.h` is a header-only Eigen
class with **no Unreal Engine dependencies**.  It can be compiled and tested in
the standalone CMake build (see [Quality Gate](#quality-gate)).

### Action Vector Layout

| Index | Name | Range | Meaning |
|---|---|---|---|
| 0 | `movement_urgency` | [0, 1] | ≥ threshold → call `MoveToActor` |
| 1 | `focus_urgency` | [0, 1] | ≥ threshold → call `SetFocus`; suppressed to 0 below `focus_threshold` |
| 2 | `best_target_index` | int or -1 | Index of peak-salience actor when movement **or** focus is warranted |

### Cognitive Mode Bias

| Mode | Movement Bias |
|---|---|
| Reactive | 1.0 (full response) |
| Deliberative | 0.8 |
| Integrative | 0.7 |
| Creative | 0.6 |
| Reflective | 0.5 (conservative) |

Example (pure C++):

```cpp
#include "UnrealBridge/CognitiveActionArbiter.h"

DeepTreeEcho::CognitiveActionArbiter arb;
std::vector<float> salience = {0.2f, 0.85f, 0.4f};
auto action = arb.compute(salience, DeepTreeEcho::CognitiveMode::Reactive);

bool move  = DeepTreeEcho::CognitiveActionArbiter::should_move(action);   // true
bool focus = DeepTreeEcho::CognitiveActionArbiter::should_focus(action);  // true
int  idx   = DeepTreeEcho::CognitiveActionArbiter::target_index(action);  // 1
```

---

## Quality Gate

Run the standalone CMake + GTest build before submitting changes.

### Linux / macOS (system g++ in PATH)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release -j
cd build
ctest --output-on-failure --parallel 2 -L unit -C Release
```

### Windows (LLVM clang++ + MSYS2 mingw64)

On Windows no C++ compiler is on `PATH` in a clean shell, so the build
requires a toolchain file that locates LLVM and the MSYS2 sysroot:

```powershell
cmake -S . -B build `
      -DCMAKE_BUILD_TYPE=Release `
      -DBUILD_TESTING=ON `
      -G Ninja `
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-windows-clang-mingw.cmake
cmake --build build --config Release -j
cd build
ctest --output-on-failure --parallel 2 -L unit -C Release
```

**Prerequisites** (both installed by WinGet):

| Tool | WinGet ID | Default install path |
|------|-----------|----------------------|
| LLVM / Clang 22+ | `LLVM.LLVM` | `C:\Program Files\LLVM` |
| MSYS2 | `MSYS2.MSYS2` | `C:\msys64` |
| Ninja | `Ninja-build.Ninja` | on PATH via WinGet links |

After installing MSYS2, open an MSYS2 MinGW64 shell and run:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-winpthreads-git
```

**Why the toolchain file is needed**:
MinGW's `g++.exe` invokes sub-tools (`cc1plus.exe`, `as.exe`, etc.) that
themselves depend on `libgcc_s_seh-1.dll`, `libgmp-10.dll`, and other MinGW
DLLs.  Without MinGW on `PATH`, those sub-tools fail even if the compiler
path is specified explicitly.  LLVM's `clang++.exe` has no such problem — it
only links against standard Windows system DLLs — so it can compile directly
from a clean shell when given a `--sysroot` pointing at the MSYS2 mingw64
tree.

**Why `BUILD_SHARED_LIBS=OFF` is forced on Windows/non-MSVC**:
`CMakeLists.txt` detects `WIN32 AND NOT MSVC` and forces
`BUILD_SHARED_LIBS=OFF` and static linking of `libstdc++`, `libgcc`, and
`libwinpthread`.  This makes every test executable fully self-contained
(only `KERNEL32.dll` and `msvcrt.dll` at runtime), preventing the
`STATUS_DLL_NOT_FOUND` (0xC0000135) error that occurred in prior builds.

All tests under `DeepTreeEcho/Testing/UnitTests/` are included automatically
by the root `CMakeLists.txt` glob.  Tests that require UE headers are excluded
by filename regex in that file.

The `CognitiveActionArbiterTests.cpp` file is a pure-C++/Eigen test that is
**always included** in the standalone build and covers the decision logic end
to end.

---

## Security Notes

- Build.cs files do **not** add `PreBuildSteps` or `PostBuildSteps`
  (SECURITY.md, issue #603).
- No secrets or credentials are present in any module file.

---

## Further Reading

- `DeepTreeEcho/Core/DeepTreeEchoCore.h` — full UCLASS API for the 12-step
  cognitive cycle, 4E cognition, relevance realization, memory, and body schema.
- `DeepTreeEcho/UnrealBridge/DeepTreeEchoUnrealBridge.h` — broader avatar ↔
  cognition bridge component (expression, gesture, narrative integration).
- `CLAUDE.md` — architecture spec: 12-step cycle, 3 streams, OEIS A000081
  nesting, UCLASS/USTRUCT conventions.
- `DEVELOPMENT_ROADMAP.md` — planned subsystem milestones.
