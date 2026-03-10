# MorphConvergence: nn.ml Morph Target Convergence System

Neural network-based system that maps the 19 coarse DTE morph targets to MetaHuman's 118 MH Standards control curves with LOD-aware inference.

## Architecture

```
Input(19 morph targets)
  → AnatomicalDecomp(43 FACS AUs)     [sparse, FACS-constrained]
  → BilateralExpand(86 L/R AUs)       [context-aware asymmetry]
  → CorrectiveExpr(86 corrected)      [residual block, learns multi-AU interactions]
  → MHProjection(118 MH curves)       [sparse, anatomy-constrained]
  → LODGate(118 × 8 LOD levels)       [binary masks from Rig Logic]
```

## Results

| Metric | Before | After |
|--------|--------|-------|
| FACS Accuracy | 79.2% | 89.1% |
| Parameters | — | 123,228 |
| Inference | — | <0.1ms |
| LOD Levels | 1 | 8 |

## Files

| File | Description |
|------|-------------|
| `ARCHITECTURE.md` | Detailed architecture design document |
| `morph_convergence_net.py` | Network implementation (PyTorch) |
| `train_convergence.py` | Training script with data generation |
| `morph_convergence_trained.pt` | Trained model checkpoint |
| `training_history.json` | Loss curves from training |

## Usage

```python
from morph_convergence_net import MorphConvergenceNet, morph_dict_to_tensor, tensor_to_mh_dict
import torch

# Load trained model
model = MorphConvergenceNet()
checkpoint = torch.load("morph_convergence_trained.pt")
model.load_state_dict(checkpoint["model_state_dict"])
model.eval()

# Convert DTE morph targets to MH Standards curves
morphs = {"mouth_smile": 0.9, "eye_squint": 0.7, "cheek_raise": 0.6}
with torch.no_grad():
    mh_curves = model(morph_dict_to_tensor(morphs), lod_level=0)
    result = tensor_to_mh_dict(mh_curves)
    # {'mouthCornerPullL': 0.65, 'eyeCheekRaiseL': 0.44, ...}
```

## LOD Levels

| LOD | Active Curves | Use Case |
|-----|--------------|----------|
| 0 | 118 | Close-up, hero shot |
| 1 | 89 | Medium shot |
| 2 | 59 | Full body |
| 3 | 43 | Group scene |
| 4 | 29 | Background |
| 5 | 21 | Far background |
| 6 | 13 | Distant |
| 7 | 5 | Extreme distance |

## Composition

Integrates with the following skills:
- **facs** — FACS AU definitions and emotion decomposition
- **rig-logic** — MetaHuman DNA evaluation pipeline and LOD system
- **unreal-blueprint** — Blueprint integration patterns for UE5
- **nn** — Neural network module patterns
