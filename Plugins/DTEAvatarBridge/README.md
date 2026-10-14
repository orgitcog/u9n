# DTE Avatar Bridge (UE 5.3)

`DTEAvatarBridge` is an **isolated runtime plugin** for a Deep Tree Echo/GTAngel avatar. It provides presentation telemetry and bounded visual feedback without taking a dependency on any existing u9n cognition module, generated avatar asset, DNA file, or forwarding-header chain.

> **Scope:** this plugin outputs a suggested gait and animation/material values. It does **not** move a pawn, select gameplay actions, provide a rig, fabricate a `.uasset`/`.dna`, or assume that the supplied prototype has facial morph targets.

## Install

1. Keep the directory at `Plugins/DTEAvatarBridge` in a UE **5.3** project, or copy that directory unchanged into another UE 5.3 project's `Plugins/` directory.
2. Regenerate project files if the host IDE needs it, open the `.uproject`, then enable **DTE Avatar Bridge** under **Edit → Plugins**.
3. Restart the editor and compile the project/plugin from an actual UE 5.3 installation.
4. Add **DTE Avatar Telemetry Component** to the actor owning the parent skeletal mesh.
5. Optionally add **DTE Avatar Scene Feedback Component** to the actor and attach/position it as a forward-facing head or chest observation camera. It uses its own transient render target.

The plugin has only runtime module dependencies: `Core`, `CoreUObject`, and `Engine`.

## Cognition integration seam

This plugin deliberately does **not** include `DeepTreeEcho`, `UnrealEcho`, `ReservoirEcho`, or avatar controller headers. Existing u9n cognition should use the narrow Blueprint/C++ seam:

- Call **`Set Cognitive Telemetry(Speed, NormalizedThreat, NormalizedNovelty, NormalizedReward)`** on `UDTEAvatarTelemetryComponent` **on the game thread**.
- Bind **`OnTelemetryUpdated`** for presentation consumers or **`OnSuggestedGaitChanged`** for an animation state machine.
- If the cognition system provides an expected visual brightness, call **`Set Predicted Luminance`** on `UDTEAvatarSceneFeedbackComponent`; bind **`OnSceneFeedbackUpdated`** to consume the observed error.

Inputs are validated: non-finite values become `0`; the three normalized cognitive signals and every output expression are clamped to `[0, 1]`; speed is non-negative and capped at `MaximumSpeed` (default `600`). The explicit suggested gait rules are:

| Priority | Rule | Suggested gait |
|---:|---|---|
| 1 | Normalized speed `<= 0.03` | `Idle` |
| 2 | Threat `>= 0.80` | `Combat` |
| 3 | Threat `>= 0.55` | `Stealth` |
| 4 | Threat `>= 0.25` | `Cautious` |
| 5 | Otherwise | `Explore` |

The component publishes a **suggestion only**. A game or Animation Blueprint may choose to ignore it; no movement input, CharacterMovement value, AI action, or player decision is modified.

### Animation Blueprint wiring

1. Create an Animation Blueprint with parent class **`DTEAvatarAnimInstance`**.
2. Assign it to the parent `SkeletalMeshComponent` that uses the parent-supplied prototype rig.
3. Normally the instance finds `DTEAvatarTelemetryComponent` on its owning actor automatically. To use a different actor/component, call **`Set Telemetry Source`** during Blueprint initialization.
4. Consume the inherited `Telemetry` variable in the AnimGraph/state machine. Branch by `Telemetry.SuggestedGait`; use `Wonder`, `Awe`, `Alert`, and `WingSpread` for blend weights where the imported asset has matching controls.

`UDTEAvatarAnimInstance` updates its snapshot in `NativeUpdateAnimation` on the **game thread**. It intentionally does not implement a worker-thread animation update hook and performs no worker-thread `UObject` reads.

### Material parameter wiring

The plugin does not modify materials automatically. In an `OnTelemetryUpdated` Blueprint binding, create/cache a Dynamic Material Instance for the mesh and set these scalar parameters if the parent-created material exposes them:

| Material scalar parameter | Recommended source |
|---|---|
| `DTE_WingSpread` | `Telemetry.WingSpread` |
| `DTE_EmissiveStrength` | `Telemetry.EmissiveStrength` |
| `DTE_Expression_Wonder` | `Telemetry.Wonder` |
| `DTE_Expression_Awe` | `Telemetry.Awe` |
| `DTE_Expression_Alert` | `Telemetry.Alert` |

All recommended sources are bounded `[0,1]`. If a material expects a different physical emissive range, scale the scalar **in the material** or in Blueprint; do not make the telemetry API unbounded.

### Parent prototype rig expectations

This plugin contains no skeleton and does not rename bones. The parent asset/rig is expected to retain conventional humanoid anchors such as `pelvis`, a `spine` chain, `head`, `upperarm_l`, `lowerarm_l`, `hand_l`, and right-side equivalents (for example `upperarm_r`). Attach visual wing bones/controls in the parent control rig or AnimGraph; map `Telemetry.WingSpread` to those controls. The likely prototype has **no production facial morph targets**: until named morph targets or Control Rig controls are delivered, route the expression values to material effects, head/eye additive poses, or leave them unbound.

## Scene feedback capture

`UDTEAvatarSceneFeedbackComponent` derives from `USceneCaptureComponent2D` and is intentionally constrained:

| Property | Default | Hard runtime bound |
|---|---:|---:|
| `bFeedbackCaptureEnabled` | `false` | Must be explicitly set true |
| `FeedbackResolution` | `32 × 32` | 8–64 per side |
| `FeedbackCaptureRateHz` | `5 Hz` | 0.1–10 Hz |
| Target format | 8-bit RGBA (`PF_B8G8R8A8`) | transient, no mips |

It retains the prior luminance frame and computes:

- `NormalizedLuminance` — mean Rec. 709 luminance;
- `MotionEnergy` — mean absolute luminance difference from the prior capture;
- four region means — top-left, top-right, bottom-left, bottom-right;
- `EfferencePredictionError` — absolute error between current mean luminance and `SetPredictedLuminance`'s input.

**First-frame semantics:** a first successful capture has observed luminance and region means but reports `MotionEnergy = 0` and `EfferencePredictionError = 0`, even when a prediction was set. Prediction error begins only when a temporal observation pair exists.

### Performance and thread-safety caveats

`CaptureFeedbackNow` calls `CaptureScene()` followed by Engine's synchronous `ReadRenderTarget` path on a tiny render target. It can force a GPU/CPU synchronization point. It is therefore opt-in, disabled by default, capped at 64×64 and 10 Hz, and scheduled at 5 Hz by default. The same configured rate limit also applies to manual capture calls. Do not raise its cadence for continuous high-resolution vision; use an asynchronous render/readback design if that is required.

All component mutation, capture, render-target resource access, delegate broadcasts, and animation-instance source reads are guarded for the **game thread**. Do not call its Blueprint functions from a cognition worker thread; marshal telemetry to the game thread first.

## Portable math tests

The deterministic mapping implementation is a small UE-free C++ header at `Source/DTEAvatarBridge/Portable/DTEAvatarBridgeMath.h`. From this plugin directory, run:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I Source/DTEAvatarBridge/Portable Tests/PortableMathTests.cpp -o /tmp/dte_avatar_bridge_math_tests && /tmp/dte_avatar_bridge_math_tests
```

The tests cover non-finite/range handling, fixed deterministic mappings, first-frame prediction semantics, variable-`DeltaTime` exponential smoothing, and capture-limit validation.

## Validation boundary

The sandbox checkout contains UE 5.3 **source**, but no discovered executable `UnrealEditor`, `UnrealEditor-Cmd`, `UnrealBuildTool`, or `RunUAT.sh`. Consequently, native UHT/UBT compilation cannot be truthfully performed here. Compile in an installed UE 5.3 environment after copying/enabling the plugin. The portable standard-C++ test command above remains runnable without UE.

## Asset ownership

The parent task owns generated GLB/prototype rig and its asset documentation/delivery. This plugin intentionally supplies **no** `.uasset`, `.glb`, `.dna`, texture, animation, control rig, or fabricated facial-target placeholder.
