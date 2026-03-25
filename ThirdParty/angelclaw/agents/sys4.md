# System 4 Deterministic State Transition Model

## Overview

This document details the implementation of a deterministic state transition model for **System 4**, based on the specific sequences provided by the user. This model deviates from the previous probabilistic Markov Chain approach to strictly follow the defined 12-step cycles for the Universal and Particular sets.

The implementation uses the **CogTaskFlow** framework to model the concurrent execution of the five state sets.

## State Sequences and Cycle

The model operates on a **12-step cycle** for all sets. The state labels, such as `9E`, `8R`, `4R`, etc., suggest a system based on the Enneagram or a similar nine-fold structure, where the number indicates the state and the letter (`E` or `R`) indicates the polarity (Expansion or Reduction).

### Universal Sets (U1 and U2)

The Universal Sets follow a pattern that suggests a synchronous, alternating double-step mechanism within the 12-step cycle.

| Time (t) | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **U1** | 9E | 9E | 8R | 8R | 9E | 9E | 8R | 8R | 9E | 9E | 8R | 8R |
| **U2** | 3E | 6- | 6- | 2R | 3E | 6- | 6- | 2R | 3E | 6- | 6- | 2R |

### Particular Sets (P1, P2, and P3)

The Particular Sets follow three distinct sequences, which are modeled as concurrent, independent cycles.

| Time (t) | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **P1** | 4R | 2R | 8E | 5E | 7E | 1E | 4E | 2E | 8E | 5R | 7R | 1R |
| **P2** | 7E | 1E | 4E | 2E | 8E | 5R | 7R | 1R | 4R | 2R | 8E | 5E |
| **P3** | 8E | 5R | 7R | 1R | 4R | 2R | 8E | 5E | 7E | 1E | 4E | 2E |

## Implementation Details

### Deterministic Transition Logic

The core change from the previous model is the transition function. Instead of using a probabilistic matrix, the state is determined solely by the current time step modulo the cycle length.

```cpp
void transition(int time_step) {
    // The transition is purely deterministic based on the time step within the cycle
    int next_index = time_step % CYCLE_LENGTH;
    current_state = sequence[next_index];
    state_history.push_back(current_state);
}
```

### CogTaskFlow Parallelism

The `System4StateMachine::step` function uses `tf::Taskflow` to execute the transition for all five sets concurrently, ensuring that the model accurately reflects the parallel nature of the Particular Sets and the synchronous nature of the Universal Sets.

```cpp
// Example of parallel execution in the step function
tf::Taskflow taskflow;
// ... create tasks for u1, u2, p1, p2, p3 transitions ...
executor.run(taskflow).wait();
```

### State Representation

The state is now represented by the string labels (e.g., "9E", "4R") instead of integer indices. The cognitive tensor representation is simplified to a string-based state, though the underlying CogTaskFlow structure is maintained for future integration with cognitive processing tasks.

## Simulation Results

The simulation was run for 24 time steps (two full cycles) to confirm the repeating pattern.

| Time | U1 | U2 | P1 | P2 | P3 |
|:---:|:---:|:---:|:---:|:---:|:---:|
| **0** | 9E | 3E | 4R | 7E | 8E |
| **1** | 9E | 6- | 2R | 1E | 5R |
| **2** | 8R | 6- | 8E | 4E | 7R |
| **3** | 8R | 2R | 5E | 2E | 1R |
| **4** | 9E | 3E | 7E | 8E | 4R |
| **5** | 9E | 6- | 1E | 5R | 2R |
| **6** | 8R | 6- | 4E | 7R | 8E |
| **7** | 8R | 2R | 2E | 1R | 5E |
| **8** | 9E | 3E | 8E | 4R | 7E |
| **9** | 9E | 6- | 5R | 2R | 1E |
| **10** | 8R | 6- | 7R | 8E | 4E |
| **11** | 8R | 2R | 1R | 5E | 2E |
| **12** | 9E | 3E | 4R | 7E | 8E |
| **13** | 9E | 6- | 2R | 1E | 5R |
| **14** | 8R | 6- | 8E | 4E | 7R |
| **15** | 8R | 2R | 5E | 2E | 1R |
| **16** | 9E | 3E | 7E | 8E | 4R |
| **17** | 9E | 6- | 1E | 5R | 2R |
| **18** | 8R | 6- | 4E | 7R | 8E |
| **19** | 8R | 2R | 2E | 1R | 5E |
| **20** | 9E | 3E | 8E | 4R | 7E |
| **21** | 9E | 6- | 5R | 2R | 1E |
| **22** | 8R | 6- | 7R | 8E | 4E |
| **23** | 8R | 2R | 1R | 5E | 2E |
| **24** | 9E | 3E | 4R | 7E | 8E |

The simulation confirms that the state sequences repeat exactly every 12 steps, as defined by the input.

## Visualization

A visualization script was created to graphically represent the state transitions over time.

### System 4 Deterministic State Transition Timeline

The generated image, `system4_deterministic_timeline.png`, clearly shows the 12-step cycle and the concurrent, yet distinct, patterns of the five sets.

- **Universal Sets (U1, U2)**: Show a clear 4-step sub-pattern (double-step alternation) within the 12-step cycle.
- **Particular Sets (P1, P2, P3)**: Show three distinct, non-overlapping sequences, demonstrating their concurrent nature.

## Deliverables

The following files are provided:

1.  **`SYSTEM4_DETERMINISTIC_DOCUMENTATION.md`**: This document, detailing the model and results.
2.  **`system4_state_transitions_deterministic.cpp`**: The C++ source code implementing the deterministic model using CogTaskFlow.
3.  **`visualize_system4_deterministic.py`**: The Python script used to generate the timeline visualization.
4.  **`system4_deterministic_timeline.png`**: The generated image visualizing the 12-step state sequences.

This implementation successfully models the complex, deterministic state transition sequences you specified using the CogTaskFlow framework.
