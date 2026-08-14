# System 3 Deterministic State Transition Model

## Overview

This document details the implementation of a deterministic state transition model for **System 3**, based on the specific sequences provided by the user. This model is built on a **4-step cycle** and features one Universal Set (U1) and two Particular Sets (P1 and P2).

The implementation uses the **CogTaskFlow** framework to model the concurrent execution of the three state sets.

## State Sequences and Cycle

The model operates on a **4-step cycle** for all sets. The state labels, such as `4E`, `3R`, `2E`, and `1R`, continue the pattern of a number indicating the state and a letter (`E` for Expansion or `R` for Reduction) indicating the polarity.

### Universal Set (U1)

The Universal Set follows a simple alternating pattern:

| Time (t) | 0 | 1 | 2 | 3 |
|:---:|:---:|:---:|:---:|:---:|
| **U1** | 4E | 3R | 4E | 3R |

### Particular Sets (P1 and P2)

The Particular Sets follow two distinct, staggered sequences:

| Time (t) | 0 | 1 | 2 | 3 |
|:---:|:---:|:---:|:---:|:---:|
| **P1** | 2E | 1E | 2E | 1R |
| **P2** | 1R | 2E | 1E | 2E |

## Implementation Details

### Deterministic Transition Logic

The core logic is a deterministic transition based on the time step modulo the cycle length, ensuring the exact sequence is followed at every step.

### CogTaskFlow Parallelism

The `System3StateMachine::step` function uses `tf::Taskflow` to execute the transition for all three sets concurrently, accurately reflecting the parallel nature of the system.

## Simulation Results

The simulation was run for 12 time steps (three full cycles) to confirm the repeating pattern.

| Time | U1 | P1 | P2 |
|:---:|:---:|:---:|:---:|
| **0** | 4E | 2E | 1R |
| **1** | 3R | 1E | 2E |
| **2** | 4E | 2E | 1E |
| **3** | 3R | 1R | 2E |
| **4** | 4E | 2E | 1R |
| **5** | 3R | 1E | 2E |
| **6** | 4E | 2E | 1E |
| **7** | 3R | 1R | 2E |
| **8** | 4E | 2E | 1R |
| **9** | 3R | 1E | 2E |
| **10** | 4E | 2E | 1E |
| **11** | 3R | 1R | 2E |
| **12** | 4E | 2E | 1R |

The simulation confirms that the state sequences repeat exactly every 4 steps, as defined by the input.

## Visualization

The generated image, `system3_deterministic_timeline.png`, visually represents the 4-step cycle and the concurrent patterns of the three sets.

- **Universal Set (U1)**: Shows the alternating `4E` and `3R` states.
- **Particular Sets (P1, P2)**: Show the distinct, staggered sequences, demonstrating their concurrent nature.

## Deliverables

The following files are provided:

1.  **`SYSTEM3_DETERMINISTIC_DOCUMENTATION.md`**: This document, detailing the model and results.
2.  **`system3_state_transitions_deterministic.cpp`**: The C++ source code implementing the deterministic model using CogTaskFlow.
3.  **`visualize_system3_deterministic.py`**: The Python script used to generate the timeline visualization.
4.  **`system3_deterministic_timeline.png`**: The generated image visualizing the 4-step state sequences.

This implementation successfully models the complex, deterministic state transition sequences you specified for System 3 using the CogTaskFlow framework.
