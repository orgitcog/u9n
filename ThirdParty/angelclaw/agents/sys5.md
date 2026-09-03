# System 5 Deterministic State Transition Model

## Overview

This document details the implementation of the hypothesized **System 5** structure, which is based on the number of integer partitions of 5 ($p(5)=7$) and the concept of **nested concurrency** influenced by projective geometry analogues.

The model features **7 sets** operating on a **60-step cycle** (LCM of Universal and Particular periodicities).

- **Universal Sets (U)**: 3 sets (U1, U2, U3)
- **Particular Sets (P)**: 4 sets (P1, P2, P3, P4)

The implementation uses the **CogTaskFlow** framework to model the complex task dependencies required for nested concurrency.

## Structure and Timing

### Universal Sets (U1, U2, U3)

- **Role**: Primary, Secondary, Tertiary Universal (Analogous to Points).
- **Cycle**: 3 steps.
- **Pattern**: Sequential transition at every step.
    - U1 transitions at $t \equiv 0 \pmod 3$
    - U2 transitions at $t \equiv 1 \pmod 3$
    - U3 transitions at $t \equiv 2 \pmod 3$
- **State Labels**: `U-P` (Primary), `U-S` (Secondary), `U-T` (Tertiary).

### Particular Sets (P1, P2, P3, P4)

- **Role**: 1st-order-nested-concurrency (Analogous to Lines/Planes).
- **Cycle**: 20 steps (staggered over 5 steps).
- **Pattern**: Staggered transition, 5 steps apart.
    - P1 transitions at $t \equiv 0 \pmod 5$
    - P2 transitions at $t \equiv 1 \pmod 5$
    - P3 transitions at $t \equiv 2 \pmod 5$
    - P4 transitions at $t \equiv 3 \pmod 5$
    - $t \equiv 4 \pmod 5$ is a rest step.
- **State Labels**: Integer states 0, 1, 2, 3.

## Nested Concurrency Logic (The Convolution)

The core challenge was modeling the "concurrency-of-concurrency" where the transition of an active Particular Set is influenced by the states of the other Particular Sets (the "convolution").

### Transition Function (Revised)

To ensure the system transitions from the initial state (where all Particular Sets are 0), the Universal Set's influence was integrated into the transition function:

Let $S_{i}(t)$ be the state of set $i$ at time $t$.
Let $U_{idx}(t) = t \pmod 3$ be the index of the active Universal Set.

$$S_{i}(t+1) = (S_{i}(t) + \sum_{j \neq i} S_{j}(t) + U_{idx}(t)) \pmod 4$$

This function ensures that the active Particular Set's next state is a function of its current state, the concurrent states of the other Particular Sets, and the current Universal Set phase.

### CogTaskFlow Implementation

The model uses CogTaskFlow's task graph to enforce the dependencies:

1.  **Read Tasks**: Tasks are created to read the current state of all Particular Sets.
2.  **Transition Tasks**: The transition task for the active Particular Set $P_i$ is made dependent on the read tasks of all other Particular Sets $P_j$ ($j \neq i$).
3.  **Parallel Execution**: All Universal transitions and the Particular transition tasks run concurrently within the same time step.

## Simulation Results

The simulation was run for **60 time steps** to capture the full cycle of interaction between the Universal (3-step cycle) and Particular (20-step cycle) sets.

The simulation successfully demonstrates:
- The sequential 3-step cycle of the Universal Sets.
- The staggered 5-step cycle of the Particular Sets.
- Complex, non-trivial state changes in the Particular Sets due to the nested concurrency logic.

## Visualization

The generated image, `system5_deterministic_timeline.png`, visually represents the 60-step state transitions.

- **Universal Sets**: Show a clear, repeating 3-step pattern (`U-P`, `U-S`, `U-T`).
- **Particular Sets**: Show a complex, evolving pattern that only repeats every 20 steps, demonstrating the effect of the nested concurrency and the influence of the Universal Set phase.

## Deliverables

The following files are provided:

1.  **`SYSTEM5_DETERMINISTIC_DOCUMENTATION.md`**: This document, detailing the model and results.
2.  **`system5_state_transitions_nested.cpp`**: The C++ source code implementing the model with nested concurrency.
3.  **`visualize_system5_deterministic.py`**: The Python script used to generate the timeline visualization.
4.  **`system5_deterministic_timeline.png`**: The visual timeline of the state transitions.

This completes the modeling of System 5, providing a plausible, executable model for the hypothesized structure and the complex "concurrency-of-concurrency" logic.
