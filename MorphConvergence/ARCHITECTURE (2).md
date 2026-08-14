# MorphConvergenceNet Architecture

## Problem Statement

The DTE avatar uses 19 coarse morph targets. MetaHuman provides 251 fine-grained MH Standards control curves. The current system achieves 77.5% FACS accuracy. We need a differentiable mapping that converges to 100% while respecting Rig Logic's 8-level LOD hierarchy.

## Architecture: LOD-Aware MorphConvergenceNet

### Core Insight

The mapping from 19 morph targets to 251 MH Standards curves is **not arbitrary** — it follows FACS anatomy. Each coarse morph target decomposes into a known set of MH curves (e.g., `eye_squint` → `eyeCheekRaiseL/R` + `eyeSquintInnerL/R`). The nn learns the **intensity transfer functions** and **cross-AU interactions** (corrective expressions) that make this decomposition accurate.

### Network Topology

```
Input Layer (19 morph targets)
    │
    ├─── Anatomical Decomposition Layer (19 → 70 FACS AUs)
    │    [Sparse linear, FACS-constrained connectivity]
    │
    ├─── Bilateral Expansion Layer (70 → 140 L/R AUs)
    │    [Asymmetry module: learns L/R independence from context]
    │
    ├─── Corrective Expression Layer (140 → 140)
    │    [Residual block: learns multi-AU interaction corrections]
    │
    ├─── MH Standards Projection Layer (140 → 251 curves)
    │    [Sparse linear, anatomy-constrained]
    │
    └─── LOD Gate Layer (251 → 251 per LOD level)
         [8 binary masks, one per LOD, learned from Rig Logic joint groups]

Output: 251 MH Standards curve values × LOD level
```

### Layer Details

#### 1. Anatomical Decomposition (nn.Linear + Sigmoid)

Maps each of the 19 morph targets to its constituent FACS AUs. The connectivity is **sparse** — `mouth_smile` only connects to AU12-related neurons, not to AU4 (brow lowerer). The sparsity mask is derived from the FACS skill's AU table.

```
Input:  19-dim vector (morph target values, [0,1])
Output: 70-dim vector (FACS AU activations, [0,1])
Params: 19×70 weight matrix (sparse, ~200 non-zero entries)
```

#### 2. Bilateral Expansion (nn.Parallel + context attention)

Expands unilateral AU activations into bilateral L/R pairs. Uses a small attention mechanism to learn asymmetry from expression context (e.g., smirk = high L, low R).

```
Input:  70-dim AU vector
Output: 140-dim L/R AU vector
Params: 70×2 expansion weights + 70-dim asymmetry bias
```

#### 3. Corrective Expression Layer (nn.Sequential residual)

Learns the corrective expressions that Rig Logic applies when multiple AUs co-activate. This is the key innovation — instead of hand-authoring ~1000 corrective expressions, the nn learns them from data.

```
Input:  140-dim L/R AU vector
Hidden: 256-dim (captures multi-AU interactions)
Output: 140-dim corrected L/R AU vector (residual: input + correction)
Params: 140→256→140 with ReLU, skip connection
```

#### 4. MH Standards Projection (nn.Linear + Sigmoid)

Maps corrected AU activations to the 251 MetaHuman control curves. Again sparse — each AU maps to its known MH curves.

```
Input:  140-dim corrected AU vector
Output: 251-dim MH Standards curve values [0,1]
Params: 140×251 weight matrix (sparse, ~500 non-zero entries)
```

#### 5. LOD Gate Layer (element-wise mask)

8 binary masks (one per LOD level) that zero out curves not needed at that LOD. Derived from Rig Logic's joint group → LOD mapping. At LOD 0 (maximum detail), all 251 curves are active. At LOD 7 (minimum), only ~30 curves remain.

```
LOD 0: 251 active curves (full fidelity)
LOD 1: ~200 active curves
LOD 2: ~150 active curves
LOD 3: ~100 active curves
LOD 4: ~70 active curves
LOD 5: ~50 active curves
LOD 6: ~40 active curves
LOD 7: ~30 active curves (jaw, blink, smile only)
```

### Training Strategy

#### Loss Function: Composite FACS Loss

```
L_total = L_facs + λ_corr * L_corrective + λ_lod * L_lod_consistency

L_facs = MSE(predicted_mh_curves, ground_truth_mh_curves)
L_corrective = MSE(predicted_corrected, rig_logic_corrected)
L_lod_consistency = MSE(LOD_n_output[active_mask], LOD_0_output[active_mask])
```

The LOD consistency loss ensures that curves active at lower LODs produce the same values as at LOD 0 — the LOD system only removes curves, never changes their values.

#### Training Data Sources

1. **FACS-coded reference frames** (14 frames × 251 ground truth curves)
2. **Video analysis morph targets** (8 videos × ~10 frames × 19 morph values)
3. **Expression catalog** (42 expressions × 19 morph values + FACS AU ground truth)
4. **Synthetic augmentation**: Interpolate between known expressions, add noise
5. **FACS constraint regularization**: AU values must respect anatomical limits

### Parameter Count

| Layer | Parameters |
|-------|-----------|
| Anatomical Decomposition | ~200 (sparse) |
| Bilateral Expansion | ~210 |
| Corrective Expression | 140×256 + 256×140 = ~71,680 |
| MH Standards Projection | ~500 (sparse) |
| LOD Gates | 8 × 251 = 2,008 (binary, non-trainable) |
| **Total trainable** | **~72,590** |

This is a deliberately small network — it should converge quickly and run in real-time (<0.1ms per frame).
