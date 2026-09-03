# System 2 Deterministic State Transition Model

## Overview

This document details the implementation of a deterministic state transition model for **System 2**, based on the specific sequences provided by the user. This model is the most concise of the series, featuring a **2-step cycle** with one Universal Set (U1) and one Particular Set (P1).

The implementation uses the **CogTaskFlow** framework to model the concurrent execution of the two state sets.

## State Sequences and Cycle

The model operates on a **2-step cycle** for both sets. The state labels, `2E`, `1E`, and `1R`, continue the pattern of a number indicating the state and a letter (`E` for Expansion or `R` for Reduction) indicating the polarity.

### Universal Set (U1)

The Universal Set remains in a constant state throughout the cycle:

| Time (t) | 0 | 1 |
|:---:|:---:|:---:|
| **U1** | 2E | 2E |

### Particular Set (P1)

The Particular Set alternates between two states:

| Time (t) | 0 | 1 |
|:---:|:---:|:---:|
| **P1** | 1E | 1R |

## Implementation Details

### Deterministic Transition Logic

The core logic is a deterministic transition based on the time step modulo the cycle length, ensuring the exact sequence is followed at every step.

### CogTaskFlow Parallelism

The `System2StateMachine::step` function uses `tf::Taskflow` to execute the transition for both sets concurrently, accurately reflecting the parallel nature of the system.

## Simulation Results

The simulation was run for 12 time steps (six full cycles) to confirm the repeating pattern.

| Time | U1 | P1 |
|:---:|:---:|:---:|
| **0** | 2E | 1E |
| **1** | 2E | 1R |
| **2** | 2E | 1E |
| **3** | 2E | 1R |
| **4** | 2E | 1E |
| **5** | 2E | 1R |
| **6** | 2E | 1E |
| **7** | 2E | 1R |
| **8** | 2E | 1E |
| **9** | 2E | 1R |
| **10** | 2E | 1E |
| **11** | 2E | 1R |
| **12** | 2E | 1E |

The simulation confirms that the state sequences repeat exactly every 2 steps, as defined by the input.

## Visualization

The generated image, `system2_deterministic_timeline.png`, visually represents the 2-step cycle and the concurrent patterns of the two sets.

- **Universal Set (U1)**: Shows the constant `2E` state.
- **Particular Set (P1)**: Shows the clear alternation between `1E` and `1R`.

## Deliverables

The following files are provided:

1.  **`SYSTEM2_DETERMINISTIC_DOCUMENTATION.md`**: This document, detailing the model and results.
2.  **`system2_state_transitions_deterministic.cpp`**: The C++ source code implementing the model.
3.  **`visualize_system2_deterministic.py`**: The Python script used to generate the timeline visualization.
4.  **`system2_deterministic_timeline.png`**: The generated image visualizing the 2-step state sequences.

This implementation successfully models the deterministic state transition sequences you specified for System 2 using the CogTaskFlow framework.
