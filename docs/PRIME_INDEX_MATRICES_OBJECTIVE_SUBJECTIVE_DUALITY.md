# Prime-Index Coordinate Matrices and Objective/Subjective Duality

## Overview

This document formalizes the **prime-index coordinate system** for partition matrices and the **objective/subjective duality** that underlies the Deep Tree Echo cognitive architecture. The key insight is that integer partitions can be weighted using prime ordinals, creating a natural correspondence between combinatorial structures and computational ontologies.

## 1. Prime-Index Coordinate System

### 1.1 Prime Ordinal Mapping

The prime ordinal function maps natural numbers to primes:

```
π(0) = 2    (0th prime)
π(1) = 3    (1st prime)
π(2) = 5    (2nd prime)
π(3) = 7    (3rd prime)
π(4) = 11   (4th prime)
...
```

### 1.2 Part Tag Function

The part tag function maps partition parts to primes:

```
τ(k) = π(k-1)

τ(1) = π(0) = 2
τ(2) = π(1) = 3
τ(3) = π(2) = 5
τ(4) = π(3) = 7
τ(5) = π(4) = 11
```

### 1.3 Partition Weight Function

For a partition λ = (λ₁, λ₂, ..., λᵣ), the weight is:

```
W(λ) = ⊗_{t=1}^{r} τ(λₜ) = ∏_{t=1}^{r} τ(λₜ)
```

This maps partitions to products of primes, creating unique prime factorizations.

---

## 2. Explicit Matrices for n = 1, 2, 3

### 2.1 n = 1

**Partitions:** (1)

**Weights:**
- W(1) = τ(1) = 2

**State Matrix S₁:**
```
S₁ = [ 2 ]
```

**Refinement Matrix R₁:** (no splits possible)
```
R₁ = [ 0 ]
```

---

### 2.2 n = 2

**Partitions (basis order):** (2), (1,1)

**Weights:**
- W(2) = τ(2) = 3
- W(1,1) = τ(1) ⊗ τ(1) = 2 × 2 = 4

**State Matrix S₂:**
```
S₂ = | 3  0 |
     | 0  4 |
```

**Refinement Matrix R₂:** (split 2 → 1+1)
```
R₂ = | 0  1 |
     | 0  0 |
```

**Tagged Refinement R₂^(tagged):**
```
R₂^(tagged) = | 0  4 |    (edge label: τ(1)⊗τ(1) = 2⊗2 = 4)
              | 0  0 |
```

---

### 2.3 n = 3

**Partitions (basis order):** (3), (2,1), (1,1,1)

**Weights:**
- W(3) = τ(3) = 5
- W(2,1) = τ(2) ⊗ τ(1) = 3 × 2 = 6
- W(1,1,1) = τ(1) ⊗ τ(1) ⊗ τ(1) = 2 × 2 × 2 = 8

**State Matrix S₃:**
```
S₃ = | 5  0  0 |
     | 0  6  0 |
     | 0  0  8 |
```

**Refinement Matrix R₃:** (splits: 3→2+1, then 2+1→1+1+1)
```
R₃ = | 0  1  0 |
     | 0  0  1 |
     | 0  0  0 |
```

**Tagged Refinement R₃^(tagged):**
```
R₃^(tagged) = | 0  6  0 |    (3→(2,1): τ(2)⊗τ(1) = 3⊗2 = 6)
              | 0  0  4 |    ((2,1)→(1,1,1): τ(1)⊗τ(1) = 2⊗2 = 4)
              | 0  0  0 |
```

---

## 3. Extended Matrices for n = 4, 5

### 3.1 n = 4

**Partitions (basis order):** (4), (3,1), (2,2), (2,1,1), (1,1,1,1)

**Weights:**
- W(4) = τ(4) = 7
- W(3,1) = τ(3) ⊗ τ(1) = 5 × 2 = 10
- W(2,2) = τ(2) ⊗ τ(2) = 3 × 3 = 9
- W(2,1,1) = τ(2) ⊗ τ(1) ⊗ τ(1) = 3 × 2 × 2 = 12
- W(1,1,1,1) = τ(1)⁴ = 2⁴ = 16

**State Matrix S₄:**
```
S₄ = | 7   0   0   0   0  |
     | 0  10   0   0   0  |
     | 0   0   9   0   0  |
     | 0   0   0  12   0  |
     | 0   0   0   0  16  |
```

**Refinement Matrix R₄:**
```
        (4) (3,1) (2,2) (2,1,1) (1,1,1,1)
(4)      0    1     1      0        0
(3,1)    0    0     0      1        0
(2,2)    0    0     0      1        0
(2,1,1)  0    0     0      0        1
(1,1,1,1)0    0     0      0        0
```

**Tagged Refinement R₄^(tagged):**
```
        (4) (3,1) (2,2) (2,1,1) (1,1,1,1)
(4)      0   10    9      0        0       (4→3+1: 5×2=10, 4→2+2: 3×3=9)
(3,1)    0    0    0      4        0       (3,1→2,1,1: 2×2=4)
(2,2)    0    0    0      6        0       (2,2→2,1,1: 3×2=6)
(2,1,1)  0    0    0      0        4       (2,1,1→1,1,1,1: 2×2=4)
(1,1,1,1)0    0    0      0        0
```

---

### 3.2 n = 5

**Partitions (basis order):** (5), (4,1), (3,2), (3,1,1), (2,2,1), (2,1,1,1), (1,1,1,1,1)

**Weights:**
- W(5) = τ(5) = 11
- W(4,1) = τ(4) ⊗ τ(1) = 7 × 2 = 14
- W(3,2) = τ(3) ⊗ τ(2) = 5 × 3 = 15
- W(3,1,1) = τ(3) ⊗ τ(1)² = 5 × 4 = 20
- W(2,2,1) = τ(2)² ⊗ τ(1) = 9 × 2 = 18
- W(2,1,1,1) = τ(2) ⊗ τ(1)³ = 3 × 8 = 24
- W(1,1,1,1,1) = τ(1)⁵ = 2⁵ = 32

**State Matrix S₅:**
```
S₅ = diag(11, 14, 15, 20, 18, 24, 32)
```

---

## 4. Objective/Subjective Duality

### 4.1 The Two Complementary Forms

The partition matrices reveal a fundamental duality:

| Aspect | Objective | Subjective |
|--------|-----------|------------|
| **Composition** | ⊗ (branch/product) | ⋁ (nest/chain) |
| **Structure** | Explicit multiset | Implicit nesting |
| **Category** | Coproduct (⊕) | Product (×) |
| **Geometry** | Tree (outward) | Spine (inward) |
| **Representation** | Hypergraph | Nested tensor |
| **Process** | Enumeration | Representation |
| **Semantics** | Extensional | Intensional |
| **Neural** | Inference | Training |
| **Direction** | Forward expansion | Backward compression |

### 4.2 Full Matrix Decomposition

For each n, the full matrix decomposes as:

```
Mₙ = Oₙ ⊕ Sₙ
```

Where:
- **Oₙ** = Objective/product operator (⊗-side)
- **Sₙ** = Subjective/nest operator (⋁-side)

### 4.3 Example: n = 2

**Objective weights (⊗ side):**
- (1,1) → 2·2 = 4
- (2) → 0 (pure, belongs to subjective)

```
O₂ = | 4  0 |
     | 0  0 |
```

**Subjective weights (⋁ side):**
- (2) → p₂ = 3

```
S₂ = | 0  0 |
     | 0  3 |
```

**Full matrix:**
```
M₂ = O₂ + S₂ = | 4  0 |
               | 0  3 |
```

### 4.4 Example: n = 3

**Objective weights:**
- (1,1,1) → 2·2·2 = 8
- (2,1) → 3·2 = 6
- (3) → 0 (pure)

```
O₃ = | 8  0  0 |
     | 0  6  0 |
     | 0  0  0 |
```

**Subjective weights:**
Two subjective channels for 3:
- Base pure: p₃ = 5 ((())) 
- Concurrent nested: p₄ = 7 ((()()))

```
S₃ = | 5  0 |
     | 0  7 |
```

**Full matrix (block diagonal):**
```
M₃ = O₃ ⊕ S₃ = | 8  0  0  0  0 |
               | 0  6  0  0  0 |
               | 0  0  0  0  0 |
               | 0  0  0  5  0 |
               | 0  0  0  0  7 |
```

---

## 5. Categorical Interpretation

### 5.1 Objective = Coproduct-Centric

The objective orientation corresponds to:
- **Free symmetric monoidal coproduct structures**
- **Hypergraphs, trees, branching inference**
- **Polynomial functors, species, operads**

Mathematical homes:
- Free commutative monoid
- Extensive categories
- Lawvere theories of sums
- PROPs for branching

### 5.2 Subjective = Product-Centric

The subjective orientation corresponds to:
- **Cartesian/monoidal closed product structures**
- **Nested tensors, embeddings, internal composition**
- **Recursive types, endofunctor algebras**

Mathematical homes:
- Cartesian closed categories
- Monoidal closed categories
- Initial algebras / terminal coalgebras

### 5.3 The Adjunction

The two orientations are related by an adjunction:

```
Inference ⊣ Training
```

- Objective network generates **structures**
- Subjective network assigns **coordinates**
- One explores space
- The other learns a metric on that space

---

## 6. Transjective Membrane Architecture

### 6.1 Three-Layer Design

The Deep Tree Echo architecture implements a **double membrane** system:

```
┌─────────────────────────────────────────────────┐
│         OUTER MEMBRANE: Objective Gateway        │
│  Arena, Tools, APIs, Branching Inference         │
├─────────────────────────────────────────────────┤
│     INTERMEMBRANE SPACE: Transjective Buffer     │
│  Codecs, Provenance, CrossingPolicy, Queues      │
├─────────────────────────────────────────────────┤
│         INNER MEMBRANE: Subjective Core          │
│  SelfState, PrivateMemory, InnerModel, Values    │
└─────────────────────────────────────────────────┘
```

### 6.2 Module Mapping

**Subjective Core (Inner):**
- `SelfState`: Identity kernel (invariants, commitments)
- `PrivateMemory`: Sealed episodic + semantic store
- `InnerModel`: Low-power local model
- `ValuePolicy`: Intrinsic goals, guardrails
- `AutonomyLoop`: Offline-safe control

**Transjective Buffer (Intermembrane):**
- `MembraneBus`: Append-only event log
- `CodecSuite`: Graphize(), Tensorize(), Redact()
- `CrossingPolicy`: Capability checks, trust thresholds
- `CreditAssigner`: Update proposals with justification

**Objective Gateway (Outer):**
- `ArenaIO`: Event loop integration
- `ToolRouter`: Tool calls, provider selection
- `PlannerSearch`: Branching inference
- `PublicWorkspace`: Non-sensitive scratchpad
- `Accelerator`: High-energy model calls

---

## 7. OEIS A000081 Alignment

The term counts align with OEIS A000081 (unlabeled rooted trees):

| n | p(n) | A000081(n) | Terms |
|---|------|------------|-------|
| 1 | 1    | 1          | 1     |
| 2 | 2    | 1          | 2     |
| 3 | 3    | 2          | 4     |
| 4 | 5    | 4          | 9     |
| 5 | 7    | 9          | 20    |

The term count is **not** the partition number p(n), but the **rooted-tree count** produced by an operad of multisets of multisets. This is exactly what rooted trees encode: recursively nested multisets of children.

---

## 8. Integration with Deep Tree Echo

### 8.1 Cognitive Loop Mapping

The 12-step cognitive loop maps to the partition structure:

| Steps | Partition | Weight | Stream |
|-------|-----------|--------|--------|
| {1,5,9} | Triadic | 6 | Cerebral |
| {2,6,10} | Triadic | 6 | Somatic |
| {3,7,11} | Triadic | 6 | Autonomic |
| {4,8,12} | Triadic | 6 | Integration |

### 8.2 Prime Weights as Cognitive Signatures

The prime weights provide unique signatures for cognitive states:

- **2** = Minimal unit (singleton)
- **3** = Dyadic pair
- **5** = Triadic structure
- **7** = Tetradic structure
- **11** = Pentadic structure

### 8.3 Thread Pool Scheduling

The partition refinement DAG directly maps to thread pool scheduling:

- Coarse partitions → Inter-op parallelism
- Fine partitions → Intra-op parallelism
- Refinement edges → Task dependencies

---

## References

1. OEIS A000081: Number of unlabeled rooted trees with n nodes
2. OEIS A000041: Partition function p(n)
3. Robert Campbell: Science and the Cosmic Order
4. Sigurd Andersen: N-Grams and S-Grams
