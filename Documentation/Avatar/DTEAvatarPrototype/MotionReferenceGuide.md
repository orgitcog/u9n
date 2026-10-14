# Motion references → Unreal animation direction

The supplied files are retained intact and hashed in `reference-manifest.json`. Only `dte_multi_pose_reel.mp4` was analyzed for its action sequence; the other named clips are catalogued by their supplied names and inspected container metadata, not represented as fully reviewed motion capture.

## Observed reel sequence

An AI-assisted analysis of the 12.04-second reel reports a neutral idle with head scanning at approximately 0–2 seconds, forward walking at 3–6 seconds, a profile turn at 6–8 seconds, a forward arm gesture at 9–10 seconds, and a combat-ready stance with abrupt wing appearance at 10–12 seconds. Facial markings and backpack/wing structure show temporal inconsistency. These approximate observations should be checked against the reel by the animator; they are not joint-angle measurements.

## Implementation mapping

Use separate locomotion and upper-body/expression layers. Idle, Explore, Cautious and Stealth can share a locomotion Blend Space when suitable authored or licensed animations exist. Combat is an animation-state suggestion, not an AI command; overlay defensive upper-body poses and player-selected actions. Wonder/Awe/Alert values are available from the telemetry component, but the prototype has no facial morph targets. Keep those values unbound until facial controls are authored, or use restrained head/material effects.

The wings in the reel appear abruptly rather than undergoing a continuous articulated deployment. Do not pretend to recover a fold/unfold mechanism from these frames. The prototype's wing bones support a small procedural pulse only; a production deploy system requires modeled hinges, explicit pivots and authored motion.

## What actually exports

`DTE_Idle_WingPulse_Diagnostic` is a newly authored two-second, low-amplitude idle/wing pulse exported in FBX and GLB. It tests skinning and playback. No cautious-walk, stealth, dodge/counter, wonder or awe animation has been extracted from the uploaded videos.

The five named 1080p clips are 1080×1920 at 24 fps, each approximately 8.08 seconds according to ffprobe. The original reel is 720×1280 at 24 fps. Additional UUID-named clips remain preserved without inventing semantic labels.

A future mocap pass needs consistent body landmarks and depth/occlusion correction, followed by manual foot-contact, hand, face and wing cleanup. The monocular AI-generated references alone do not guarantee reliable joint trajectories or mesh correspondence.
