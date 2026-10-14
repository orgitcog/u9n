# Deep Tree Echo — Unreal Avatar Prototype

A textured **3D reconstruction of your cybernetic angel**, with an experimental humanoid/wing rig and a portable **Unreal Engine 5.3** presentation/visual-feedback plugin for `orgitcog/u9n`.

**This is an importable prototype, not a production-certified MetaHuman.** It includes real mesh, texture, skinning and animation data. It does not include fabricated `.uasset` files, MetaHuman DNA, facial morph targets, hair grooms, or motion capture extracted from the supplied videos. Native Unreal import and plugin compilation still need to be performed in an installed UE5.3 environment.

## What is in the kit

- `Assets/DTE_Avatar_Prototype.fbx`: custom skeletal mesh and a low-amplitude, two-second diagnostic idle/wing-pulse animation.
- `Assets/DTE_Avatar_Prototype.glb`: the same skinned prototype, with PBR materials and animation for inspection outside Unreal.
- `Assets/DTE_Avatar_Prototype.blend`: editable Blender scene with the 27-bone skeleton and packed textures.
- `Assets/Textures`: 4096-pixel PBR images; use the actual dimensions in `prototype-manifest.json` as authoritative.
- `Assets/Source/dte-avatar-source.glb`: untouched, unrigged image-to-3D reconstruction.
- `Plugins/DTEAvatarBridge`: runtime plugin, portable C++ tests and Blueprint integration instructions.
- `Scripts`: reproducible rig/export, validation and Unreal Editor import scripts.
- `References`: your original images and videos, plus the generated neutral A-pose reference. The original uploads are not published to the public repository.
- `Docs`: this guide, motion-reference notes, asset provenance and validation reports.

The reconstruction contains **49,795 vertices and 79,006 triangles**, normalized to **1.72 m** tall. This height is a chosen prototype scale, not a measurement from the reference images. There is one mesh and one PBR material; body, clothes, hair and wings are not separate artist-authored objects. The skeleton has **27 bones**, including humanoid anchors and two control bones per wing. Each vertex has at most three influences with normalized weights.

## Install in u9n / UE5.3

1. Extract the kit and copy `Plugins/DTEAvatarBridge` into your Unreal project's `Plugins` directory. The repository's `UnrealEngineCog.uproject` targets **5.3**.
2. Enable **DTE Avatar Bridge** in Edit → Plugins; restart and compile using your installed Unreal toolchain. The plugin has only Core, CoreUObject and Engine dependencies. It does not alter the existing `DeepTreeEcho`, `UnrealEcho` or `DeepTreeEchoAvatar` modules.
3. Import `Assets/DTE_Avatar_Prototype.fbx` into a new Content Browser folder with **Skeletal Mesh** and **Import Animations** enabled. Leave **Skeleton** unassigned on first import so UE creates the custom skeleton. Do not select Manny or a MetaHuman skeleton directly.
4. Use import scale **1.0**, scene/unit conversion, and **Import Normals**. Open the resulting Skeletal Mesh in Persona and verify sole-to-head height is approximately **172 cm**, the character faces the intended actor-forward direction, the mesh is not mirrored, and shoulders/knees deform acceptably. Adjust the mesh component's relative yaw if the imported facing differs; do not silently rotate the skeleton after retargeting.
5. The optional `Scripts/import_dte_avatar.py` automates first import. Enable Python Editor Script Plugin and Editor Scripting Utilities, set `DTE_AVATAR_BUNDLE` to the extracted kit root (or edit `BUNDLE`), then execute the script inside Unreal's Python console. It refuses to overwrite a non-empty destination. Its Python syntax is checked; Unreal execution is **not** tested here.

Epic documents an FBX **2020.2** importer. Blender's FBX writer uses a different file version, so the Blender round trip is not a substitute for the native Unreal import check. [1]

## Material reconstruction

FBX material translation may not recreate the full GLB PBR appearance. Rebuild the Unreal material from the included textures when necessary:

- **BaseColor.png:** sRGB enabled → Base Color.
- **ORM.png:** sRGB disabled; red = ambient occlusion, green = roughness, blue = metallic.
- **NormalGL.png:** normal-map compression. The source normal map is OpenGL-style; verify tangent orientation and use **Flip Green Channel** for Unreal's DirectX convention when needed.

The supplied images show luminous crystals, orange hubs and cyan cheek markings. Their appearance is present in the reconstruction's texture, but there is **no separate authored emissive mask**. Create a mask and an emissive material layer if you want dynamic glow; multiplying the whole base color by an emissive scalar would incorrectly illuminate skin and clothing too. Hair is mesh-based, not an Unreal Groom. There is no cloth simulation or physically articulated feather system.

## Runtime cognition → expression → observation

Add **DTE Avatar Telemetry Component** to the actor owning the skeletal mesh. Create an Animation Blueprint parented to **DTEAvatarAnimInstance**. On the game thread, feed the existing cognition system through:

```cpp
TelemetryComponent->SetCognitiveTelemetry(SpeedCmPerSecond, Threat01, Novelty01, Reward01);
```

The component validates the inputs and exposes smoothed **Wonder, Awe, Alert, WingSpread and EmissiveStrength** values and a **suggested** gait. It does not drive movement, make combat decisions, or add a new reservoir/LLM. Bind `OnTelemetryUpdated` or consume the inherited `Telemetry` variable in the Animation Blueprint.

For wings, use Transform (Modify) Bone / Control Rig controls on `wing_root_l` and `wing_root_r`, starting with small rotations. Connect `Telemetry.WingSpread` to the controls, not directly to an arbitrary whole-material shader. The diagnostic animation is a deformation check, not a physically accurate fold/deploy system.

For facial expression, the values are an integration seam only. This prototype has **no facial morph targets, eye bones or finger bones**. Author those controls before expecting wonder/awe facial poses or articulated hands. A future MetaHuman route should transfer the visual design to a separately authored MetaHuman-compatible face/body; the generated mesh is not a valid DNA asset.

Optional GTAngel-style observation:

1. Add **DTE Avatar Scene Feedback Component** and attach it as a forward-facing head/chest camera. Hide the owning actor in the capture if self-occlusion is unwanted; retain it if body visibility is part of the experiment.
2. Explicitly enable feedback; it is **disabled by default**. Default capture is **32×32 at 5 Hz**, bounded to 8–64 pixels per side and 0.1–10 Hz. Change runtime settings through `ConfigureFeedbackCapture`.
3. Bind `OnSceneFeedbackUpdated` to read mean luminance, motion energy and four quadrant means. If the cognition system predicts next-frame brightness, supply it via `SetPredictedLuminance` and consume the mean-brightness prediction error.
4. First capture reports zero motion/prediction error until a prior frame exists. Sampling uses synchronous readback and may stall the GPU/CPU even at low resolution; use asynchronous readback for a production vision pipeline.

This is **visual sensorimotor feedback**, not a complete proprioception system. Joint positions, contacts and body velocity should be supplied separately by the engine/cognition system. The bridge alone does not instantiate the full GTAngel endocrine, TRIZ niche-construction or Deep Tree Echo memory architecture.

## Animation references and retargeting

The reference reel was analyzed as visual direction. It progresses from idle/head scanning (~0–2 s) to forward walking (~3–6 s), a profile turn (~6–8 s), a forward arm gesture (~9–10 s) and a combat-ready pose with abrupt wing appearance (~10–12 s). These are approximate AI-assisted observations, not measured joint tracks. The abrupt wing appearance is not evidence of an actual unfolding mechanism.

The named cautious-walk, stealth, combat, wonder and awe videos are retained with duration/frame-rate metadata and hashes. They are **not** included as native skeletal animations. Only the authored `DTE_Idle_WingPulse_Diagnostic` clip is exported in this prototype.

For Manny/Quinn or licensed motion-library animation, create a target IK Rig and set **pelvis** as Retarget Root. Define Spine (`spine_01`–`spine_03`), Head (`neck_01`–`head`), Left/Right Arm (`upperarm_*`–`hand_*`) and Left/Right Leg (`thigh_*`–`ball_*`) chains. Match chain names and reference poses in the source and target rigs; then use an IK Retargeter. Keep custom wing controls outside the humanoid locomotion retarget. Different names/counts can be retargeted, but skin weights and foot contact still require inspection. [2]

## Rebuild and test

Requires Blender 4.0+ and the source GLB:

```bash
blender -b --python Scripts/build_prototype.py -- \
  --input Assets/Source/dte-avatar-source.glb --out RebuiltAssets
blender -b --python Scripts/validate_prototype.py -- --assets RebuiltAssets
```

Portable plugin tests (no Unreal installation required):

```bash
cd Plugins/DTEAvatarBridge
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -I Source/DTEAvatarBridge/Portable Tests/PortableMathTests.cpp \
  -o /tmp/dte_avatar_bridge_math_tests
/tmp/dte_avatar_bridge_math_tests
```

The skinning script uses anatomical position regions and inverse distance to bone segments. It is reproducible and structurally validated, but **not** artist-quality automatic rigging. It infers occluded surfaces from one generated A-pose view, not a calibrated multiview reconstruction. Preserve the Blender source for manual landmark, hair, skirt, shoulder and wing weight correction.

## Validation boundary

Portable C++ tests pass with strict warnings and AddressSanitizer/UndefinedBehaviorSanitizer. Plugin JSON and Python syntax are validated. The asset validation report records weight normalization, unweighted vertex count, GLB skin/animation data, and the independent Blender FBX re-import results.

**Still required in Unreal:** UHT/UBT build, actual asset import, animation playback and deformation review, unit/facing checks, material reconstruction, capture performance profiling and connection to live u9n cognition. The kit is not certified for production gameplay, facial animation, direct MetaHuman/Manny skeleton sharing or asset-store distribution.

## References

[1]: https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-skeletal-meshes-using-fbx-in-unreal-engine?application_version=5.3 "Epic UE5.3 — Importing Skeletal Meshes Using FBX"
[2]: https://dev.epicgames.com/documentation/en-us/unreal-engine/ik-rig-animation-retargeting-in-unreal-engine?application_version=5.3 "Epic UE5.3 — IK Rig Animation Retargeting"
