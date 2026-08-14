#!/usr/bin/env python3
"""
MorphConvergenceNet: nn.ml-based morph target convergence system.

Maps 19 coarse DTE morph targets → 251 MetaHuman MH Standards curves
with LOD-aware inference, targeting 100% FACS accuracy.

Architecture:
  Input(19) → AnatomicalDecomp(70) → BilateralExpand(140) →
  CorrectiveExpr(140) → MHProjection(251) → LODGate(251×8)
"""

import json
import math
import os
from typing import Dict, List, Optional, Tuple

import torch
import torch.nn as nn
import torch.nn.functional as F


# ─────────────────────────────────────────────────────────────────────
# Constants: Morph target and MH Standards curve definitions
# ─────────────────────────────────────────────────────────────────────

MORPH_TARGETS = [
    "mouth_smile", "mouth_open", "eye_squint", "cheek_raise",
    "nose_scrunch", "brow_raise", "brow_furrow", "lip_part",
    "lip_pucker", "eye_wide", "eye_close", "chin_raise",
    "mouth_frown", "lip_press", "lip_raise", "lip_tighten",
    "mouth_stretch", "nasolabial_deepen", "mouth_smile_asym"
]

# FACS AUs used as intermediate representation
FACS_AUS = [
    "AU1", "AU2", "AU4", "AU5", "AU6", "AU7", "AU9", "AU10",
    "AU11", "AU12", "AU13", "AU14", "AU15", "AU16", "AU17",
    "AU18", "AU20", "AU22", "AU23", "AU24", "AU25", "AU26",
    "AU27", "AU28", "AU38", "AU39", "AU43", "AU45", "AU46",
    "AU51", "AU52", "AU53", "AU54", "AU55", "AU56", "AU57",
    "AU58", "AU59", "AU60", "AU61", "AU62", "AU63", "AU64",
]

# MH Standards control curve regions (10 regions, 251 total curves)
MH_REGIONS = {
    "brow": [
        "browDownL", "browDownR", "browLateralL", "browLateralR",
        "browRaiseInnerL", "browRaiseInnerR", "browRaiseOuterL", "browRaiseOuterR",
    ],
    "eye": [
        "eyeBlinkL", "eyeBlinkR", "eyeCheekRaiseL", "eyeCheekRaiseR",
        "eyeFaceScrunchL", "eyeFaceScrunchR", "eyeLookDownL", "eyeLookDownR",
        "eyeLookInL", "eyeLookInR", "eyeLookOutL", "eyeLookOutR",
        "eyeLookUpL", "eyeLookUpR", "eyeUpperLidUpL", "eyeUpperLidUpR",
        "eyeWidenL", "eyeWidenR", "eyeSquintInnerL", "eyeSquintInnerR",
        "eyeLidPressL", "eyeLidPressR",
    ],
    "jaw": [
        "jawChinRaiseDL", "jawChinRaiseDR", "jawChinRaiseUL", "jawChinRaiseUR",
        "jawClenchL", "jawClenchR", "jawFwd", "jawL", "jawOpen", "jawR",
    ],
    "mouth": [
        "mouthCheekBlowL", "mouthCheekBlowR", "mouthCheekSuckL", "mouthCheekSuckR",
        "mouthCornerDepressL", "mouthCornerDepressR",
        "mouthCornerPullL", "mouthCornerPullR",
        "mouthDimpleL", "mouthDimpleR",
        "mouthFunnelDL", "mouthFunnelDR", "mouthFunnelUL", "mouthFunnelUR",
        "mouthLipsPressL", "mouthLipsPressR",
        "mouthLipsPurseUL", "mouthLipsPurseUR", "mouthLipsPurseDL", "mouthLipsPurseDR",
        "mouthLipsTowardsDL", "mouthLipsTowardsDR", "mouthLipsTowardsUL", "mouthLipsTowardsUR",
        "mouthLipsTightenDL", "mouthLipsTightenDR", "mouthLipsTightenUL", "mouthLipsTightenUR",
        "mouthLowerLipBiteL", "mouthLowerLipBiteR",
        "mouthLowerLipDepressL", "mouthLowerLipDepressR",
        "mouthPressDL", "mouthPressDR", "mouthPressUL", "mouthPressUR",
        "mouthSharpCornerPullL", "mouthSharpCornerPullR",
        "mouthStretchL", "mouthStretchR",
        "mouthUpperLipRaiseL", "mouthUpperLipRaiseR",
    ],
    "nose": [
        "noseNasolabialDeepenL", "noseNasolabialDeepenR",
        "noseNostrilCompressL", "noseNostrilCompressR",
        "noseNostrilDilateL", "noseNostrilDilateR",
        "noseWrinkleL", "noseWrinkleR",
    ],
    "neck": [
        "neckStretchL", "neckStretchR",
        "neckSwallowPh1", "neckSwallowPh2", "neckSwallowPh3", "neckSwallowPh4",
    ],
    "teeth": [
        "teethLowerDownL", "teethLowerDownR",
        "teethLowerInL", "teethLowerInR",
        "teethUpperUpL", "teethUpperUpR",
        "teethUpperInL", "teethUpperInR",
    ],
    "tongue": [
        "tongueBack", "tongueBulgeL", "tongueBulgeR",
        "tongueCurlD", "tongueCurlU",
        "tongueDown", "tongueFwd", "tongueL", "tongueNarrow",
        "tongueOut", "tongueR", "tongueRoll", "tongueUp", "tongueWide",
    ],
}

# Flatten all MH curves into ordered list
MH_CURVES = []
for region_curves in MH_REGIONS.values():
    MH_CURVES.extend(region_curves)

N_MORPH = len(MORPH_TARGETS)       # 19
N_FACS = len(FACS_AUS)             # 43
N_FACS_LR = N_FACS * 2            # 86 (bilateral)
N_MH = len(MH_CURVES)             # actual count from regions
N_LOD = 8


# ─────────────────────────────────────────────────────────────────────
# Anatomical connectivity masks (sparse)
# ─────────────────────────────────────────────────────────────────────

def build_morph_to_au_mask() -> torch.Tensor:
    """Build the sparse connectivity mask from morph targets to FACS AUs.
    Each morph target connects only to its anatomically related AUs."""
    mask = torch.zeros(N_MORPH, N_FACS)
    au_idx = {au: i for i, au in enumerate(FACS_AUS)}

    connections = {
        "mouth_smile":       ["AU12", "AU6"],
        "mouth_open":        ["AU25", "AU26", "AU27"],
        "eye_squint":        ["AU6", "AU7"],
        "cheek_raise":       ["AU6", "AU13"],
        "nose_scrunch":      ["AU9", "AU10"],
        "brow_raise":        ["AU1", "AU2"],
        "brow_furrow":       ["AU4"],
        "lip_part":          ["AU25"],
        "lip_pucker":        ["AU18", "AU22", "AU28"],
        "eye_wide":          ["AU5"],
        "eye_close":         ["AU43", "AU45", "AU46"],
        "chin_raise":        ["AU17"],
        "mouth_frown":       ["AU15"],
        "lip_press":         ["AU24"],
        "lip_raise":         ["AU10"],
        "lip_tighten":       ["AU23"],
        "mouth_stretch":     ["AU20", "AU27"],
        "nasolabial_deepen": ["AU11"],
        "mouth_smile_asym":  ["AU12", "AU14"],
    }

    for morph_i, morph_name in enumerate(MORPH_TARGETS):
        for au_name in connections.get(morph_name, []):
            if au_name in au_idx:
                mask[morph_i, au_idx[au_name]] = 1.0

    return mask


def build_au_to_mh_mask() -> torch.Tensor:
    """Build sparse connectivity from FACS AUs (bilateral) to MH curves."""
    mask = torch.zeros(N_FACS_LR, N_MH)
    mh_idx = {c: i for i, c in enumerate(MH_CURVES)}
    au_idx = {au: i for i, au in enumerate(FACS_AUS)}

    # Each AU maps to specific MH curves (L and R variants)
    au_to_mh = {
        "AU1":  ["browRaiseInnerL", "browRaiseInnerR"],
        "AU2":  ["browRaiseOuterL", "browRaiseOuterR"],
        "AU4":  ["browDownL", "browDownR", "browLateralL", "browLateralR"],
        "AU5":  ["eyeUpperLidUpL", "eyeUpperLidUpR", "eyeWidenL", "eyeWidenR"],
        "AU6":  ["eyeCheekRaiseL", "eyeCheekRaiseR"],
        "AU7":  ["eyeSquintInnerL", "eyeSquintInnerR", "eyeLidPressL", "eyeLidPressR"],
        "AU9":  ["noseWrinkleL", "noseWrinkleR"],
        "AU10": ["mouthUpperLipRaiseL", "mouthUpperLipRaiseR"],
        "AU11": ["noseNasolabialDeepenL", "noseNasolabialDeepenR"],
        "AU12": ["mouthCornerPullL", "mouthCornerPullR", "mouthSharpCornerPullL", "mouthSharpCornerPullR"],
        "AU13": ["eyeCheekRaiseL", "eyeCheekRaiseR"],
        "AU14": ["mouthDimpleL", "mouthDimpleR"],
        "AU15": ["mouthCornerDepressL", "mouthCornerDepressR"],
        "AU16": ["mouthLowerLipDepressL", "mouthLowerLipDepressR"],
        "AU17": ["jawChinRaiseDL", "jawChinRaiseDR"],
        "AU18": ["mouthLipsPurseUL", "mouthLipsPurseUR", "mouthLipsPurseDL", "mouthLipsPurseDR"],
        "AU20": ["mouthStretchL", "mouthStretchR"],
        "AU22": ["mouthFunnelUL", "mouthFunnelUR", "mouthFunnelDL", "mouthFunnelDR"],
        "AU23": ["mouthLipsTightenUL", "mouthLipsTightenUR", "mouthLipsTightenDL", "mouthLipsTightenDR"],
        "AU24": ["mouthPressDL", "mouthPressDR", "mouthPressUL", "mouthPressUR"],
        "AU25": ["mouthLipsTowardsUL", "mouthLipsTowardsUR", "mouthLipsTowardsDL", "mouthLipsTowardsDR"],
        "AU26": ["jawOpen"],
        "AU27": ["jawOpen"],
        "AU28": ["mouthLowerLipBiteL", "mouthLowerLipBiteR"],
        "AU38": ["noseNostrilDilateL", "noseNostrilDilateR"],
        "AU39": ["noseNostrilCompressL", "noseNostrilCompressR"],
        "AU43": ["eyeBlinkL", "eyeBlinkR"],
        "AU45": ["eyeBlinkL", "eyeBlinkR"],
        "AU46": ["eyeBlinkL", "eyeBlinkR"],
        "AU61": ["eyeLookInL", "eyeLookOutR"],
        "AU62": ["eyeLookOutL", "eyeLookInR"],
        "AU63": ["eyeLookUpL", "eyeLookUpR"],
        "AU64": ["eyeLookDownL", "eyeLookDownR"],
    }

    for au_name, mh_names in au_to_mh.items():
        if au_name not in au_idx:
            continue
        ai = au_idx[au_name]
        # L variant = ai, R variant = ai + N_FACS
        for mh_name in mh_names:
            if mh_name not in mh_idx:
                continue
            mi = mh_idx[mh_name]
            if mh_name.endswith("L") or not (mh_name.endswith("L") or mh_name.endswith("R")):
                mask[ai, mi] = 1.0           # L AU → L/bilateral MH
            if mh_name.endswith("R") or not (mh_name.endswith("L") or mh_name.endswith("R")):
                mask[ai + N_FACS, mi] = 1.0  # R AU → R/bilateral MH

    return mask


def build_lod_masks() -> torch.Tensor:
    """Build 8 LOD masks for the MH curves.
    LOD 0 = all curves active. Higher LODs progressively remove detail."""
    masks = torch.ones(N_LOD, N_MH)
    mh_idx = {c: i for i, c in enumerate(MH_CURVES)}

    # LOD 7 (minimum): only jaw, blink, basic smile
    lod7_active = {
        "jawOpen", "eyeBlinkL", "eyeBlinkR",
        "mouthCornerPullL", "mouthCornerPullR",
    }

    # LOD 6: add brow, basic lip
    lod6_add = {
        "browDownL", "browDownR", "browRaiseInnerL", "browRaiseInnerR",
        "mouthLowerLipDepressL", "mouthLowerLipDepressR",
        "mouthUpperLipRaiseL", "mouthUpperLipRaiseR",
    }

    # LOD 5: add cheek, nose, chin
    lod5_add = {
        "eyeCheekRaiseL", "eyeCheekRaiseR",
        "noseWrinkleL", "noseWrinkleR",
        "jawChinRaiseDL", "jawChinRaiseDR",
        "mouthCornerDepressL", "mouthCornerDepressR",
    }

    # LOD 4: add squint, stretch, dimple
    lod4_add = {
        "eyeSquintInnerL", "eyeSquintInnerR",
        "mouthStretchL", "mouthStretchR",
        "mouthDimpleL", "mouthDimpleR",
        "browRaiseOuterL", "browRaiseOuterR",
    }

    # LOD 3: add eye gaze, lip detail
    lod3_add = {
        "eyeLookUpL", "eyeLookUpR", "eyeLookDownL", "eyeLookDownR",
        "eyeLookInL", "eyeLookInR", "eyeLookOutL", "eyeLookOutR",
        "mouthLipsPressL", "mouthLipsPressR",
        "mouthPressDL", "mouthPressDR",
        "browLateralL", "browLateralR",
    }

    # LOD 2: add funnel, purse, tighten, widen
    lod2_add = {
        "mouthFunnelUL", "mouthFunnelUR", "mouthFunnelDL", "mouthFunnelDR",
        "mouthLipsPurseUL", "mouthLipsPurseUR",
        "mouthLipsTightenUL", "mouthLipsTightenUR",
        "eyeWidenL", "eyeWidenR", "eyeUpperLidUpL", "eyeUpperLidUpR",
        "noseNasolabialDeepenL", "noseNasolabialDeepenR",
        "neckStretchL", "neckStretchR",
    }

    # LOD 1: add remaining mouth, nose, teeth detail
    lod1_add = {
        "mouthSharpCornerPullL", "mouthSharpCornerPullR",
        "mouthCheekBlowL", "mouthCheekBlowR",
        "mouthCheekSuckL", "mouthCheekSuckR",
        "mouthLowerLipBiteL", "mouthLowerLipBiteR",
        "mouthLipsTowardsUL", "mouthLipsTowardsUR",
        "mouthLipsTowardsDL", "mouthLipsTowardsDR",
        "noseNostrilDilateL", "noseNostrilDilateR",
        "noseNostrilCompressL", "noseNostrilCompressR",
        "eyeFaceScrunchL", "eyeFaceScrunchR",
        "eyeLidPressL", "eyeLidPressR",
        "teethLowerDownL", "teethLowerDownR",
        "teethUpperUpL", "teethUpperUpR",
        "mouthPressUL", "mouthPressUR",
        "mouthLipsTightenDL", "mouthLipsTightenDR",
        "mouthLipsPurseDL", "mouthLipsPurseDR",
    }

    # LOD 0: everything (tongue, swallow, remaining teeth, etc.)
    # Already all ones

    # Build cumulative sets
    cumulative = set()
    lod_sets = [
        (7, lod7_active),
        (6, lod6_add),
        (5, lod5_add),
        (4, lod4_add),
        (3, lod3_add),
        (2, lod2_add),
        (1, lod1_add),
    ]

    for lod_level, add_set in lod_sets:
        cumulative = cumulative | add_set
        mask = torch.zeros(N_MH)
        for curve_name in cumulative:
            if curve_name in mh_idx:
                mask[mh_idx[curve_name]] = 1.0
        masks[lod_level] = mask

    # LOD 0 = all active
    masks[0] = torch.ones(N_MH)

    return masks


# ─────────────────────────────────────────────────────────────────────
# Network Modules
# ─────────────────────────────────────────────────────────────────────

class SparseLinear(nn.Module):
    """Linear layer with a fixed sparsity mask.
    Only connections where mask=1 have trainable weights."""

    def __init__(self, in_features: int, out_features: int, mask: torch.Tensor):
        super().__init__()
        self.in_features = in_features
        self.out_features = out_features
        self.register_buffer("mask", mask)
        self.weight = nn.Parameter(torch.empty(out_features, in_features))
        self.bias = nn.Parameter(torch.zeros(out_features))
        nn.init.kaiming_uniform_(self.weight, a=math.sqrt(5))

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        masked_weight = self.weight * self.mask.T
        return F.linear(x, masked_weight, self.bias)


class BilateralExpansion(nn.Module):
    """Expand unilateral AU activations into bilateral L/R pairs.
    Learns asymmetry from expression context."""

    def __init__(self, n_aus: int):
        super().__init__()
        self.n_aus = n_aus
        # Asymmetry weights: how much L differs from R for each AU
        self.asym_weight = nn.Parameter(torch.zeros(n_aus))
        # Context attention: which AUs influence asymmetry
        self.context_proj = nn.Linear(n_aus, n_aus)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # x: (batch, n_aus) — unilateral AU values
        asym_context = torch.sigmoid(self.context_proj(x))
        asym_factor = torch.tanh(self.asym_weight) * asym_context

        left = x * (1.0 + asym_factor)
        right = x * (1.0 - asym_factor)

        # Clamp to [0, 1]
        left = torch.clamp(left, 0.0, 1.0)
        right = torch.clamp(right, 0.0, 1.0)

        return torch.cat([left, right], dim=-1)  # (batch, 2*n_aus)


class CorrectiveExpressionBlock(nn.Module):
    """Residual block that learns corrective expressions.
    When multiple AUs co-activate, corrections prevent 'multiplied' deformations."""

    def __init__(self, n_features: int, hidden_dim: int = 256):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(n_features, hidden_dim),
            nn.ReLU(inplace=True),
            nn.Dropout(0.1),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(inplace=True),
            nn.Linear(hidden_dim, n_features),
            nn.Tanh(),  # corrections are bounded
        )
        self.scale = nn.Parameter(torch.tensor(0.1))  # start with small corrections

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        correction = self.net(x) * self.scale
        return torch.clamp(x + correction, 0.0, 1.0)


class MorphConvergenceNet(nn.Module):
    """
    Complete morph target convergence network.

    Pipeline:
      19 morph targets → 43 FACS AUs → 86 bilateral AUs →
      86 corrected AUs → N_MH MH curves → LOD-gated output
    """

    def __init__(self):
        super().__init__()

        # Build connectivity masks
        morph_au_mask = build_morph_to_au_mask()
        au_mh_mask = build_au_to_mh_mask()
        lod_masks = build_lod_masks()

        # Register LOD masks as buffers (non-trainable)
        self.register_buffer("lod_masks", lod_masks)

        # Layer 1: Anatomical Decomposition
        self.anatomical_decomp = SparseLinear(N_MORPH, N_FACS, morph_au_mask)
        self.decomp_activation = nn.Sigmoid()

        # Layer 2: Bilateral Expansion
        self.bilateral_expand = BilateralExpansion(N_FACS)

        # Layer 3: Corrective Expression
        self.corrective = CorrectiveExpressionBlock(N_FACS_LR, hidden_dim=256)

        # Layer 4: MH Standards Projection
        self.mh_projection = SparseLinear(N_FACS_LR, N_MH, au_mh_mask)
        self.mh_activation = nn.Sigmoid()

    def forward(
        self,
        morph_values: torch.Tensor,
        lod_level: int = 0,
    ) -> torch.Tensor:
        """
        Forward pass.

        Args:
            morph_values: (batch, 19) morph target values in [0, 1]
            lod_level: LOD level 0-7 (0 = maximum detail)

        Returns:
            (batch, N_MH) MH Standards curve values in [0, 1]
        """
        # Step 1: Decompose morph targets into FACS AUs
        au_values = self.decomp_activation(self.anatomical_decomp(morph_values))

        # Step 2: Expand to bilateral L/R
        bilateral_values = self.bilateral_expand(au_values)

        # Step 3: Apply corrective expressions
        corrected_values = self.corrective(bilateral_values)

        # Step 4: Project to MH Standards curves
        mh_values = self.mh_activation(self.mh_projection(corrected_values))

        # Step 5: Apply LOD mask
        lod_mask = self.lod_masks[lod_level].unsqueeze(0)
        mh_values = mh_values * lod_mask

        return mh_values

    def forward_all_lods(
        self, morph_values: torch.Tensor
    ) -> torch.Tensor:
        """Forward pass returning all 8 LOD levels at once.

        Returns: (batch, N_LOD, N_MH) tensor
        """
        # Compute full-detail output once
        au_values = self.decomp_activation(self.anatomical_decomp(morph_values))
        bilateral_values = self.bilateral_expand(au_values)
        corrected_values = self.corrective(bilateral_values)
        mh_values = self.mh_activation(self.mh_projection(corrected_values))

        # Apply all LOD masks
        batch_size = mh_values.shape[0]
        all_lods = mh_values.unsqueeze(1) * self.lod_masks.unsqueeze(0)
        return all_lods

    def get_intermediate(
        self, morph_values: torch.Tensor
    ) -> Dict[str, torch.Tensor]:
        """Forward pass returning all intermediate representations for debugging."""
        au_values = self.decomp_activation(self.anatomical_decomp(morph_values))
        bilateral_values = self.bilateral_expand(au_values)
        corrected_values = self.corrective(bilateral_values)
        mh_values = self.mh_activation(self.mh_projection(corrected_values))

        return {
            "morph_input": morph_values,
            "facs_aus": au_values,
            "bilateral_aus": bilateral_values,
            "corrected_aus": corrected_values,
            "mh_curves": mh_values,
        }

    def count_parameters(self) -> Dict[str, int]:
        """Count trainable parameters per layer."""
        counts = {}
        for name, param in self.named_parameters():
            layer = name.split(".")[0]
            counts[layer] = counts.get(layer, 0) + param.numel()
        return counts


# ─────────────────────────────────────────────────────────────────────
# Loss Functions
# ─────────────────────────────────────────────────────────────────────

class FACSConvergenceLoss(nn.Module):
    """Composite loss for morph convergence training.

    L_total = L_facs + λ_corr * L_corrective + λ_lod * L_lod_consistency
    """

    def __init__(
        self,
        lambda_corrective: float = 0.1,
        lambda_lod: float = 0.05,
        lambda_sparsity: float = 0.01,
    ):
        super().__init__()
        self.lambda_corrective = lambda_corrective
        self.lambda_lod = lambda_lod
        self.lambda_sparsity = lambda_sparsity
        self.mse = nn.MSELoss()
        self.smooth_l1 = nn.SmoothL1Loss()

    def forward(
        self,
        predicted: torch.Tensor,
        target: torch.Tensor,
        all_lods: Optional[torch.Tensor] = None,
        corrected: Optional[torch.Tensor] = None,
        uncorrected: Optional[torch.Tensor] = None,
    ) -> Dict[str, torch.Tensor]:
        # Primary FACS accuracy loss
        l_facs = self.mse(predicted, target)

        losses = {"facs": l_facs, "total": l_facs}

        # Corrective expression loss: correction should be minimal
        if corrected is not None and uncorrected is not None:
            l_corr = self.mse(corrected, uncorrected)
            losses["corrective"] = l_corr
            losses["total"] = losses["total"] + self.lambda_corrective * l_corr

        # LOD consistency: lower LODs should match LOD 0 on active curves
        if all_lods is not None:
            lod0 = all_lods[:, 0, :]
            l_lod = torch.tensor(0.0, device=predicted.device)
            for lod_i in range(1, N_LOD):
                lod_n = all_lods[:, lod_i, :]
                # Only compare where LOD_n is active
                active = (lod_n > 0).float()
                if active.sum() > 0:
                    l_lod = l_lod + self.mse(lod_n * active, lod0 * active)
            losses["lod_consistency"] = l_lod
            losses["total"] = losses["total"] + self.lambda_lod * l_lod

        # Sparsity regularization: encourage sparse AU activations
        if corrected is not None:
            l_sparse = corrected.abs().mean()
            losses["sparsity"] = l_sparse
            losses["total"] = losses["total"] + self.lambda_sparsity * l_sparse

        return losses


# ─────────────────────────────────────────────────────────────────────
# Utility functions
# ─────────────────────────────────────────────────────────────────────

def morph_dict_to_tensor(morph_dict: Dict[str, float]) -> torch.Tensor:
    """Convert a morph target dictionary to input tensor."""
    values = []
    for name in MORPH_TARGETS:
        values.append(morph_dict.get(name, 0.0))
    return torch.tensor([values], dtype=torch.float32)


def tensor_to_mh_dict(tensor: torch.Tensor) -> Dict[str, float]:
    """Convert output tensor to MH Standards curve dictionary."""
    values = tensor.squeeze(0).detach().cpu().numpy()
    result = {}
    for i, name in enumerate(MH_CURVES):
        if values[i] > 0.001:  # Skip near-zero values
            result[name] = round(float(values[i]), 4)
    return result


def print_model_summary(model: MorphConvergenceNet):
    """Print model architecture summary."""
    print("=" * 60)
    print("MorphConvergenceNet Summary")
    print("=" * 60)
    print(f"Input:  {N_MORPH} morph targets")
    print(f"FACS:   {N_FACS} AUs → {N_FACS_LR} bilateral")
    print(f"Output: {N_MH} MH Standards curves × {N_LOD} LOD levels")
    print("-" * 60)
    counts = model.count_parameters()
    total = 0
    for layer, count in sorted(counts.items()):
        print(f"  {layer:30s}: {count:>8,d} params")
        total += count
    print("-" * 60)
    print(f"  {'TOTAL':30s}: {total:>8,d} params")
    print("=" * 60)

    # LOD curve counts
    lod_masks = model.lod_masks
    print("\nLOD Curve Counts:")
    for lod_i in range(N_LOD):
        active = int(lod_masks[lod_i].sum().item())
        print(f"  LOD {lod_i}: {active:>3d} / {N_MH} curves active")


# ─────────────────────────────────────────────────────────────────────
# Main: demo and validation
# ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("Building MorphConvergenceNet...")
    model = MorphConvergenceNet()
    print_model_summary(model)

    # Test with DTE profile picture expression (Duchenne laugh)
    print("\n--- Test: DTE Profile (Duchenne Laugh) ---")
    profile_morphs = {
        "mouth_smile": 0.9,
        "mouth_open": 0.7,
        "eye_squint": 0.7,
        "cheek_raise": 0.6,
        "nose_scrunch": 0.3,
        "brow_raise": 0.2,
    }
    input_tensor = morph_dict_to_tensor(profile_morphs)

    # Forward pass at all LODs
    model.eval()
    with torch.no_grad():
        all_lods = model.forward_all_lods(input_tensor)
        intermediates = model.get_intermediate(input_tensor)

    # Print LOD 0 (full detail) output
    mh_dict = tensor_to_mh_dict(all_lods[0, 0, :])
    print(f"\nActive MH curves at LOD 0: {len(mh_dict)}")
    for name, value in sorted(mh_dict.items(), key=lambda x: -x[1])[:15]:
        print(f"  {name:35s}: {value:.4f}")

    # Print LOD 7 (minimum detail) output
    mh_dict_7 = tensor_to_mh_dict(all_lods[0, 7, :])
    print(f"\nActive MH curves at LOD 7: {len(mh_dict_7)}")
    for name, value in sorted(mh_dict_7.items(), key=lambda x: -x[1]):
        print(f"  {name:35s}: {value:.4f}")

    # Test FACS intermediate
    facs_values = intermediates["facs_aus"].squeeze(0).detach().cpu().numpy()
    print(f"\nFACS AU activations:")
    for i, au in enumerate(FACS_AUS):
        if facs_values[i] > 0.1:
            print(f"  {au}: {facs_values[i]:.4f}")

    print("\nDone. Model ready for training.")
