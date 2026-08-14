#!/usr/bin/env python3
"""
Training script for MorphConvergenceNet.

Generates training data from FACS-coded expressions, trains the network
to converge from 19 morph targets to 118 MH Standards curves, and
validates LOD consistency.
"""

import json
import os
import sys
from typing import Dict, List, Tuple

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader

from morph_convergence_net import (
    MorphConvergenceNet, FACSConvergenceLoss,
    MORPH_TARGETS, FACS_AUS, MH_CURVES, MH_REGIONS,
    N_MORPH, N_FACS, N_MH, N_LOD,
    morph_dict_to_tensor, tensor_to_mh_dict, print_model_summary,
)


# ─────────────────────────────────────────────────────────────────────
# Ground Truth: FACS-coded expressions → MH Standards target values
# ─────────────────────────────────────────────────────────────────────

def build_training_data() -> List[Tuple[Dict[str, float], Dict[str, float]]]:
    """Build training pairs: (morph_targets, mh_curves_ground_truth).

    Each pair maps the repo's morph target values to the ground truth
    MH Standards curve values derived from FACS coding of reference frames.
    """
    data = []

    # --- JOY expressions ---
    data.append(({
        "mouth_smile": 0.8, "mouth_open": 0.5, "eye_squint": 0.5, "cheek_raise": 0.6,
    }, {
        "mouthCornerPullL": 0.7, "mouthCornerPullR": 0.7,
        "eyeCheekRaiseL": 0.5, "eyeCheekRaiseR": 0.5,
        "eyeSquintInnerL": 0.3, "eyeSquintInnerR": 0.3,
        "jawOpen": 0.3, "mouthUpperLipRaiseL": 0.1, "mouthUpperLipRaiseR": 0.1,
    }))

    data.append(({
        "mouth_smile": 0.9, "mouth_open": 0.7, "eye_squint": 0.7, "nose_scrunch": 0.3,
    }, {
        "mouthCornerPullL": 0.85, "mouthCornerPullR": 0.85,
        "eyeCheekRaiseL": 0.6, "eyeCheekRaiseR": 0.6,
        "eyeSquintInnerL": 0.5, "eyeSquintInnerR": 0.5,
        "jawOpen": 0.5, "noseWrinkleL": 0.15, "noseWrinkleR": 0.15,
        "mouthUpperLipRaiseL": 0.2, "mouthUpperLipRaiseR": 0.2,
    }))

    data.append(({
        "mouth_smile": 0.4, "eye_squint": 0.2, "cheek_raise": 0.3,
    }, {
        "mouthCornerPullL": 0.4, "mouthCornerPullR": 0.4,
        "eyeCheekRaiseL": 0.3, "eyeCheekRaiseR": 0.3,
        "eyeSquintInnerL": 0.15, "eyeSquintInnerR": 0.15,
    }))

    data.append(({
        "mouth_smile": 0.6, "mouth_open": 0.4, "eye_wide": 0.3, "brow_raise": 0.3,
    }, {
        "mouthCornerPullL": 0.55, "mouthCornerPullR": 0.55,
        "jawOpen": 0.25, "eyeWidenL": 0.2, "eyeWidenR": 0.2,
        "eyeUpperLidUpL": 0.15, "eyeUpperLidUpR": 0.15,
        "browRaiseInnerL": 0.2, "browRaiseInnerR": 0.2,
        "browRaiseOuterL": 0.15, "browRaiseOuterR": 0.15,
    }))

    data.append(({
        "eye_close": 0.9, "mouth_smile": 0.4, "cheek_raise": 0.3,
    }, {
        "eyeBlinkL": 0.85, "eyeBlinkR": 0.85,
        "mouthCornerPullL": 0.4, "mouthCornerPullR": 0.4,
        "eyeCheekRaiseL": 0.3, "eyeCheekRaiseR": 0.3,
    }))

    # --- WONDER expressions ---
    data.append(({
        "lip_part": 0.4, "eye_wide": 0.4, "brow_raise": 0.3,
    }, {
        "eyeWidenL": 0.3, "eyeWidenR": 0.3,
        "eyeUpperLidUpL": 0.25, "eyeUpperLidUpR": 0.25,
        "browRaiseInnerL": 0.2, "browRaiseInnerR": 0.2,
        "browRaiseOuterL": 0.2, "browRaiseOuterR": 0.2,
    }))

    data.append(({
        "lip_pucker": 0.2, "brow_furrow": 0.1,
    }, {
        "mouthLipsPurseUL": 0.15, "mouthLipsPurseUR": 0.15,
        "mouthLipsPurseDL": 0.1, "mouthLipsPurseDR": 0.1,
        "browDownL": 0.08, "browDownR": 0.08,
        "browLateralL": 0.05, "browLateralR": 0.05,
    }))

    # --- ANGER expressions ---
    data.append(({
        "brow_furrow": 0.4, "eye_squint": 0.3, "lip_press": 0.2,
    }, {
        "browDownL": 0.3, "browDownR": 0.3,
        "browLateralL": 0.2, "browLateralR": 0.2,
        "eyeSquintInnerL": 0.25, "eyeSquintInnerR": 0.25,
        "mouthPressDL": 0.15, "mouthPressDR": 0.15,
        "mouthLipsTightenUL": 0.1, "mouthLipsTightenUR": 0.1,
    }))

    data.append(({
        "brow_furrow": 0.8, "eye_wide": 0.5, "eye_squint": 0.8,
        "nose_scrunch": 0.6, "mouth_open": 0.6, "lip_press": 0.6,
    }, {
        "browDownL": 0.7, "browDownR": 0.7,
        "browLateralL": 0.5, "browLateralR": 0.5,
        "eyeUpperLidUpL": 0.4, "eyeUpperLidUpR": 0.4,
        "eyeSquintInnerL": 0.7, "eyeSquintInnerR": 0.7,
        "noseWrinkleL": 0.5, "noseWrinkleR": 0.5,
        "mouthUpperLipRaiseL": 0.5, "mouthUpperLipRaiseR": 0.5,
        "jawOpen": 0.4,
        "mouthPressDL": 0.5, "mouthPressDR": 0.5,
        "mouthLipsTightenUL": 0.4, "mouthLipsTightenUR": 0.4,
    }))

    # --- SADNESS expressions ---
    data.append(({
        "brow_raise": 0.3, "brow_furrow": 0.2, "mouth_frown": 0.2, "chin_raise": 0.2,
    }, {
        "browRaiseInnerL": 0.25, "browRaiseInnerR": 0.25,
        "browDownL": 0.15, "browDownR": 0.15,
        "mouthCornerDepressL": 0.2, "mouthCornerDepressR": 0.2,
        "jawChinRaiseDL": 0.15, "jawChinRaiseDR": 0.15,
    }))

    data.append(({
        "brow_raise": 0.6, "brow_furrow": 0.4, "cheek_raise": 0.3,
        "mouth_frown": 0.6, "chin_raise": 0.4,
    }, {
        "browRaiseInnerL": 0.5, "browRaiseInnerR": 0.5,
        "browDownL": 0.3, "browDownR": 0.3,
        "eyeCheekRaiseL": 0.25, "eyeCheekRaiseR": 0.25,
        "noseNasolabialDeepenL": 0.3, "noseNasolabialDeepenR": 0.3,
        "mouthCornerDepressL": 0.55, "mouthCornerDepressR": 0.55,
        "jawChinRaiseDL": 0.35, "jawChinRaiseDR": 0.35,
    }))

    # --- FEAR expressions ---
    data.append(({
        "brow_raise": 0.6, "brow_furrow": 0.3, "eye_wide": 0.6,
        "mouth_stretch": 0.4, "lip_part": 0.3,
    }, {
        "browRaiseInnerL": 0.5, "browRaiseInnerR": 0.5,
        "browRaiseOuterL": 0.3, "browRaiseOuterR": 0.3,
        "browDownL": 0.2, "browDownR": 0.2,
        "eyeWidenL": 0.5, "eyeWidenR": 0.5,
        "eyeUpperLidUpL": 0.4, "eyeUpperLidUpR": 0.4,
        "mouthStretchL": 0.35, "mouthStretchR": 0.35,
    }))

    # --- DISGUST expressions ---
    data.append(({
        "nose_scrunch": 0.6, "lip_raise": 0.6, "mouth_frown": 0.4,
        "chin_raise": 0.4, "mouth_open": 0.2,
    }, {
        "noseWrinkleL": 0.5, "noseWrinkleR": 0.5,
        "mouthUpperLipRaiseL": 0.55, "mouthUpperLipRaiseR": 0.55,
        "mouthCornerDepressL": 0.35, "mouthCornerDepressR": 0.35,
        "jawChinRaiseDL": 0.3, "jawChinRaiseDR": 0.3,
        "jawOpen": 0.15,
    }))

    # --- PUNK/ASYMMETRIC expressions ---
    data.append(({
        "mouth_smile_asym": 0.5, "eye_squint": 0.2,
    }, {
        "mouthCornerPullL": 0.5, "mouthCornerPullR": 0.2,
        "mouthDimpleL": 0.3, "mouthDimpleR": 0.1,
        "eyeSquintInnerL": 0.15, "eyeSquintInnerR": 0.15,
    }))

    # --- SPEAK expressions ---
    data.append(({
        "mouth_open": 0.6, "lip_part": 0.3,
    }, {
        "jawOpen": 0.4,
    }))

    data.append(({
        "lip_pucker": 0.5, "mouth_open": 0.3,
    }, {
        "mouthLipsPurseUL": 0.35, "mouthLipsPurseUR": 0.35,
        "mouthLipsPurseDL": 0.25, "mouthLipsPurseDR": 0.25,
        "mouthFunnelUL": 0.2, "mouthFunnelUR": 0.2,
        "mouthFunnelDL": 0.15, "mouthFunnelDR": 0.15,
        "jawOpen": 0.2,
    }))

    # --- NEUTRAL ---
    data.append(({}, {}))

    # --- Profile picture (Duchenne laugh) ---
    data.append(({
        "mouth_smile": 0.9, "mouth_open": 0.7, "eye_squint": 0.7,
        "cheek_raise": 0.6, "nose_scrunch": 0.3, "brow_raise": 0.2,
    }, {
        "mouthCornerPullL": 0.85, "mouthCornerPullR": 0.85,
        "mouthSharpCornerPullL": 0.3, "mouthSharpCornerPullR": 0.3,
        "eyeCheekRaiseL": 0.65, "eyeCheekRaiseR": 0.65,
        "eyeSquintInnerL": 0.45, "eyeSquintInnerR": 0.45,
        "eyeLidPressL": 0.2, "eyeLidPressR": 0.2,
        "jawOpen": 0.5,
        "noseWrinkleL": 0.1, "noseWrinkleR": 0.1,
        "mouthUpperLipRaiseL": 0.2, "mouthUpperLipRaiseR": 0.2,
        "browRaiseOuterL": 0.15, "browRaiseOuterR": 0.15,
        "noseNasolabialDeepenL": 0.3, "noseNasolabialDeepenR": 0.3,
    }))

    return data


def augment_data(
    data: List[Tuple[Dict[str, float], Dict[str, float]]],
    n_augmented: int = 500,
) -> List[Tuple[Dict[str, float], Dict[str, float]]]:
    """Augment training data by interpolating between known expressions
    and adding noise."""
    import random
    augmented = list(data)

    for _ in range(n_augmented):
        # Pick two random expressions and interpolate
        idx_a = random.randint(0, len(data) - 1)
        idx_b = random.randint(0, len(data) - 1)
        alpha = random.random()

        morph_a, mh_a = data[idx_a]
        morph_b, mh_b = data[idx_b]

        # Interpolate morph targets
        all_morph_keys = set(morph_a.keys()) | set(morph_b.keys())
        interp_morph = {}
        for k in all_morph_keys:
            va = morph_a.get(k, 0.0)
            vb = morph_b.get(k, 0.0)
            interp_morph[k] = va * alpha + vb * (1 - alpha)

        # Interpolate MH curves
        all_mh_keys = set(mh_a.keys()) | set(mh_b.keys())
        interp_mh = {}
        for k in all_mh_keys:
            va = mh_a.get(k, 0.0)
            vb = mh_b.get(k, 0.0)
            v = va * alpha + vb * (1 - alpha)
            # Add small noise
            v = max(0.0, min(1.0, v + random.gauss(0, 0.02)))
            interp_mh[k] = v

        augmented.append((interp_morph, interp_mh))

    return augmented


# ─────────────────────────────────────────────────────────────────────
# Dataset
# ─────────────────────────────────────────────────────────────────────

class MorphConvergenceDataset(Dataset):
    """Dataset of (morph_target_vector, mh_curve_vector) pairs."""

    def __init__(self, data: List[Tuple[Dict[str, float], Dict[str, float]]]):
        self.inputs = []
        self.targets = []
        mh_idx = {c: i for i, c in enumerate(MH_CURVES)}

        for morph_dict, mh_dict in data:
            # Build input tensor
            inp = torch.zeros(N_MORPH)
            for i, name in enumerate(MORPH_TARGETS):
                inp[i] = morph_dict.get(name, 0.0)
            self.inputs.append(inp)

            # Build target tensor
            tgt = torch.zeros(N_MH)
            for name, value in mh_dict.items():
                if name in mh_idx:
                    tgt[mh_idx[name]] = value
            self.targets.append(tgt)

    def __len__(self):
        return len(self.inputs)

    def __getitem__(self, idx):
        return self.inputs[idx], self.targets[idx]


# ─────────────────────────────────────────────────────────────────────
# Training Loop
# ─────────────────────────────────────────────────────────────────────

def train(
    model: MorphConvergenceNet,
    dataset: MorphConvergenceDataset,
    epochs: int = 200,
    lr: float = 0.005,
    batch_size: int = 16,
    log_interval: int = 20,
) -> Dict[str, List[float]]:
    """Train the MorphConvergenceNet."""

    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True)
    optimizer = optim.Adam(model.parameters(), lr=lr, weight_decay=1e-5)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs)
    criterion = FACSConvergenceLoss()

    history = {"epoch": [], "total": [], "facs": [], "lod": []}

    model.train()
    for epoch in range(1, epochs + 1):
        epoch_losses = {"total": 0, "facs": 0, "lod": 0}
        n_batches = 0

        for inputs, targets in loader:
            optimizer.zero_grad()

            # Forward
            predicted = model(inputs, lod_level=0)
            all_lods = model.forward_all_lods(inputs)
            intermediates = model.get_intermediate(inputs)

            # Compute loss
            losses = criterion(
                predicted, targets,
                all_lods=all_lods,
                corrected=intermediates["corrected_aus"],
                uncorrected=intermediates["bilateral_aus"],
            )

            # Backward
            losses["total"].backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
            optimizer.step()

            epoch_losses["total"] += losses["total"].item()
            epoch_losses["facs"] += losses["facs"].item()
            epoch_losses["lod"] += losses.get("lod_consistency", torch.tensor(0.0)).item()
            n_batches += 1

        scheduler.step()

        # Log
        avg_total = epoch_losses["total"] / n_batches
        avg_facs = epoch_losses["facs"] / n_batches
        avg_lod = epoch_losses["lod"] / n_batches

        history["epoch"].append(epoch)
        history["total"].append(avg_total)
        history["facs"].append(avg_facs)
        history["lod"].append(avg_lod)

        if epoch % log_interval == 0 or epoch == 1:
            print(f"Epoch {epoch:>4d}/{epochs} | "
                  f"Total: {avg_total:.6f} | "
                  f"FACS: {avg_facs:.6f} | "
                  f"LOD: {avg_lod:.6f} | "
                  f"LR: {scheduler.get_last_lr()[0]:.6f}")

    return history


# ─────────────────────────────────────────────────────────────────────
# Evaluation
# ─────────────────────────────────────────────────────────────────────

def evaluate(model: MorphConvergenceNet, data: List[Tuple[Dict, Dict]]):
    """Evaluate model accuracy on the original (non-augmented) data."""
    model.eval()
    mh_idx = {c: i for i, c in enumerate(MH_CURVES)}

    total_error = 0.0
    total_curves = 0
    per_expression_scores = []

    with torch.no_grad():
        for morph_dict, mh_dict in data:
            inp = morph_dict_to_tensor(morph_dict)
            pred = model(inp, lod_level=0).squeeze(0)

            # Compute per-curve error for ground truth curves
            expr_error = 0.0
            n_curves = 0
            for name, target_val in mh_dict.items():
                if name in mh_idx:
                    pred_val = pred[mh_idx[name]].item()
                    expr_error += abs(pred_val - target_val)
                    n_curves += 1

            if n_curves > 0:
                accuracy = max(0, 1.0 - (expr_error / n_curves)) * 100
            else:
                accuracy = 100.0  # neutral

            per_expression_scores.append(accuracy)
            total_error += expr_error
            total_curves += max(n_curves, 1)

    mean_accuracy = sum(per_expression_scores) / len(per_expression_scores)
    return mean_accuracy, per_expression_scores


# ─────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("=" * 60)
    print("MorphConvergenceNet Training")
    print("=" * 60)

    # Build model
    model = MorphConvergenceNet()
    print_model_summary(model)

    # Build training data
    print("\nBuilding training data...")
    raw_data = build_training_data()
    print(f"  Raw expressions: {len(raw_data)}")

    augmented_data = augment_data(raw_data, n_augmented=500)
    print(f"  Augmented total: {len(augmented_data)}")

    dataset = MorphConvergenceDataset(augmented_data)

    # Pre-training evaluation
    print("\n--- Pre-Training Evaluation ---")
    pre_accuracy, _ = evaluate(model, raw_data)
    print(f"  Mean accuracy: {pre_accuracy:.1f}%")

    # Train
    print("\n--- Training ---")
    history = train(model, dataset, epochs=300, lr=0.005, log_interval=30)

    # Post-training evaluation
    print("\n--- Post-Training Evaluation ---")
    post_accuracy, scores = evaluate(model, raw_data)
    print(f"  Mean accuracy: {post_accuracy:.1f}%")
    print(f"  Min accuracy:  {min(scores):.1f}%")
    print(f"  Max accuracy:  {max(scores):.1f}%")

    # Test profile picture
    print("\n--- Profile Picture Test (Duchenne Laugh) ---")
    profile_morphs = {
        "mouth_smile": 0.9, "mouth_open": 0.7, "eye_squint": 0.7,
        "cheek_raise": 0.6, "nose_scrunch": 0.3, "brow_raise": 0.2,
    }
    model.eval()
    with torch.no_grad():
        inp = morph_dict_to_tensor(profile_morphs)
        pred = model(inp, lod_level=0)
        mh_dict = tensor_to_mh_dict(pred)

    print(f"Active MH curves: {len(mh_dict)}")
    for name, value in sorted(mh_dict.items(), key=lambda x: -x[1])[:20]:
        print(f"  {name:35s}: {value:.4f}")

    # LOD comparison
    print("\n--- LOD Comparison ---")
    with torch.no_grad():
        all_lods = model.forward_all_lods(inp)
    for lod_i in range(N_LOD):
        lod_dict = tensor_to_mh_dict(all_lods[0, lod_i, :])
        print(f"  LOD {lod_i}: {len(lod_dict):>3d} active curves")

    # Save model
    save_path = "/home/ubuntu/morph-convergence/morph_convergence_trained.pt"
    torch.save({
        "model_state_dict": model.state_dict(),
        "history": history,
        "accuracy": post_accuracy,
    }, save_path)
    print(f"\nModel saved to {save_path}")

    # Save training history
    import json
    with open("/home/ubuntu/morph-convergence/training_history.json", "w") as f:
        json.dump(history, f, indent=2)
    print("Training history saved.")

    print(f"\n{'=' * 60}")
    print(f"CONVERGENCE: {pre_accuracy:.1f}% → {post_accuracy:.1f}%")
    print(f"{'=' * 60}")
