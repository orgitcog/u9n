# CogMorph - Cognitive State Transformation Framework

CogMorph provides isomorphic transformations between different representations of cognitive state in the OpenCog/NPU architecture.

## Core Principle

**All projections represent the SAME cognitive state** - just viewed through different lenses:

```
┌─────────────────────────────────────────────────────────────────┐
│                    ONE COGNITIVE ENGINE                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐        │
│  │   HARDWARE   │   │   SOFTWARE   │   │    STATIC    │        │
│  │  Projection  │   │  Projection  │   │  Projection  │        │
│  ├──────────────┤   ├──────────────┤   ├──────────────┤        │
│  │ VirtualPCB   │   │ Libraries    │   │ CGGUF        │        │
│  │ MMIO regs    │   │ API calls    │   │ Serialized   │        │
│  │ DMA/IRQ      │   │ Callbacks    │   │ tensors      │        │
│  │ ASSD storage │   │ Objects      │   │ Frozen graph │        │
│  └──────────────┘   └──────────────┘   └──────────────┘        │
│         │                  │                  │                 │
│         ├──────────────────┼──────────────────┤                 │
│         │                  │                  │                 │
│  ┌──────────────┐                     ┌──────────────┐         │
│  │   NETWORK    │                     │    GLYPH     │         │
│  │  Projection  │                     │  Projection  │         │
│  ├──────────────┤                     ├──────────────┤         │
│  │ Wire proto   │                     │ Font-based   │         │
│  │ Distributed  │                     │ Visual       │         │
│  │ Remote sync  │                     │ Subfonts     │         │
│  └──────────────┘                     └──────────────┘         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Projections

### HardwareProjection

The foundational view - cognitive state as virtual hardware:

- **VirtualPCB** - The motherboard
- **vCPU** (URE) - Inference core at `0x40000000`
- **vNPU** (LLM) - Neural coprocessor at `0x40001000`
- **vTPU** (PLN) - Truth value processor at `0x40002000`
- **vAPU** (ECAN) - Attention unit at `0x40003000`
- **vASSD** - AtomSpace State Drive at `0x40004000`

Memory layout:
```
0x08000000  FLASH     (1MB)    - Firmware/rules
0x20000000  SRAM      (256KB)  - Working memory
0x40000000  PERIPH    (64KB)   - Device registers
0x80000000  ATOMSPACE (2GB)    - AtomSpace storage
0xC0000000  VRAM      (1GB)    - Attention/GPU memory
```

### LibraryProjection

Friendly API wrapper for developers who don't need hardware details:

```cpp
auto lib = cogmorph::morph<LibraryProjection>(hardware);

// Simple, clean API
uint64_t concept = lib->atomspace().add_node(CONCEPT_NODE, "cat");
lib->atomspace().set_truth_value(concept, 0.9f, 0.8f);
```

### StaticProjection (CGGUF)

Portable serialization format for:
- Saving/loading cognitive state
- Model distribution
- Checkpoint/restore

```cpp
// Save
cogmorph::save_cognitive_state(hw, "brain.cgguf");

// Load
auto restored = cogmorph::load_cognitive_state("brain.cgguf");
```

### NetworkProjection

Distributed cognitive processing:

```cpp
auto net = cogmorph::morph<NetworkProjection>(hardware);
net->connect("remote-brain.example.com:9999");
net->sync_full();
```

### GlyphProjection

Storage as visual font - the representation IS the data:

```cpp
auto glyph = cogmorph::morph<GlyphProjection>(hardware);
glyph->render_to_svg();  // Visualize cognitive state
glyph->export_font("cognitive.ttf");  // Export as font
```

## ASFS - AtomSpace File System

Filesystem optimized for hypergraph storage patterns:

```
ASFS Superblock
├── atom_table (B-tree indexed)
├── link_adjacency (optimized for graph traversal)
├── name_strings (interned string table)
├── truth_journal (append-only TV updates)
├── attention_journal (append-only AV updates)
├── type_hierarchy (type definitions)
└── index_btree (secondary indices)
```

## ASSD - AtomSpace State Drive

Block device interface for ASFS:

```cpp
auto assd = cogmorph::create_assd(1024);  // 1GB drive
assd->format();

// Atom-optimized operations
assd->write_atom(atom_id, entry);
auto result = assd->query_by_type(CONCEPT_NODE);
auto incoming = assd->query_incoming(target_id);
```

Register interface at `0x40004000`:
```
0x00 REG_CMD          - Command register
0x04 REG_STATUS       - Status register
0x08 REG_ATOM_ID      - Atom ID for operations
0x10 REG_BLOCK_ADDR   - Block address
0x18 REG_DMA_ADDR     - DMA buffer
0x20 REG_XFER_SIZE    - Transfer size
0x28 REG_ATOM_COUNT   - Total atoms (read-only)
0x30 REG_LINK_COUNT   - Total links (read-only)
0x38 REG_ERROR        - Error code
```

## Transforms

All projections can transform to any other:

```cpp
// Direct transform
auto static_proj = cogmorph::morph<StaticProjection>(hardware);

// Transform chain (finds shortest path)
auto chain = cogmorph::find_transform_path(HARDWARE, GLYPH);
auto result = chain->execute(source);

// Convenience functions
cogmorph::save_cognitive_state(hw, "state.cgguf");
cogmorph::export_cognitive_font(hw, "brain.ttf");
cogmorph::sync_to_remote(hw, "tcp://cluster:9999");
```

## Why This Architecture?

1. **Complete Observability** - Hardware metaphor makes all state observable
2. **Natural Constraints** - Memory pressure, bandwidth limits model bounded rationality
3. **Hot-Swap** - USB-like semantics for cognitive modules
4. **Formal Verification** - Hardware contracts easier to verify
5. **Future Hardware** - Direct mapping to FPGA/ASIC if needed
6. **Bridging Knowledge Gap** - Different views for different expertise levels

## Integration with OpenCog

CogMorph bridges `opencog-modern` types with the virtual hardware layer:

```cpp
// AtomId maps to hardware address
uint64_t addr = cogmorph::atom_logical_address(atom.id().value);

// TruthValue at dedicated offset
uint64_t tv_addr = cogmorph::tv_logical_address(atom.id().value);

// AttentionValue at dedicated offset
uint64_t av_addr = cogmorph::av_logical_address(atom.id().value);
```

## Building

```bash
cd npu/cogmorph
cmake -B build
cmake --build build
```

## Prime Basis - The Foundational Numeric System

Instead of floating point (powers of 2), CogMorph uses **prime factorizations** as the fundamental numeric representation:

```
Traditional:     float32 → bits → powers of 2 → limited precision
Prime Basis:     ℤ → primes → infinite factorization → exact arithmetic
```

### The Key Insight

```
n = p₁^a₁ × p₂^a₂ × p₃^a₃ × ...

- Primes pᵢ are NODES (atomic, unique, indivisible)
- Exponents aᵢ are EDGE WEIGHTS (multiplicities, strengths)
- Composite n is a HYPEREDGE (connects all primes in factorization)
```

### The Exponential Bridge

```
DISCRETE:   n = ∏ pᵢ^aᵢ           (multiplication = composition)
CONTINUOUS: log(n) = Σ aᵢ·log(pᵢ) (addition = superposition)
```

Neural network weights operate on exponents {aᵢ}, enabling gradient-based optimization of combinatorial structures!

### Examples

```cpp
using namespace cogmorph::prime;

// 6 = 2 × 3 encodes edge between nodes 0 and 1
auto edge = HyperIndex::edge(0, 1);

// 30 = 2 × 3 × 5 encodes ternary hyperedge
auto hyperedge = HyperIndex::from_nodes({0, 1, 2});

// 12 = 2² × 3 encodes weighted edge (node 0 weight 2)
auto weighted = HyperIndex::edge(0, 1, 2, 1);

// Convert to continuous space for gradient descent
auto exponents = ExponentVector::from_factorization(fact, 64);
exponents = exponents + gradient;  // Update
auto new_structure = exponents.to_factorization();  // Back to discrete
```

### Fiber Bundle Structure

```cpp
// Base = topology (which nodes connected)
// Fiber = attributes (connection weights)
FiberBundle bundle(base, fiber);

// Transform fiber without changing topology
bundle.transform_fiber([](auto& f) { return f.pow(2); });
```

### Why Prime Basis?

1. **Exact Arithmetic** - No floating point errors, ever
2. **Natural Hypergraphs** - Primes = nodes, composites = hyperedges
3. **Unified Tensors** - Shape encoded in factorization
4. **Gradient Bridge** - exp/log connects discrete ↔ continuous
5. **Fiber Bundles** - Natural for transformation groups
6. **Baseline Interop** - Everything is just prime factorizations

## Matula Numbers - Trees as Integers

The Matula-Göbel bijection gives 1-1 correspondence between ℕ and rooted trees:

```
1 = •           (leaf)
2 = •─•         (chain of 2)
3 = •─•─•       (chain of 3)
4 = •<•         (fork: 2 children)
    └•
5 = •─•─•─•     (chain of 4)
6 = •<•─•       (mixed)
    └•
```

This unifies three computational domains:

### B-Series (Butcher, 1963)
Rooted trees index Runge-Kutta methods for numerical integration:
```cpp
using namespace cogmorph::matula;
double coeff = 1.0 / BSeries::density(tree_id);  // RK coefficient
```

### J-Surfaces (Lie Derivatives)
Elementary differentials for ODEs:
```cpp
std::string diff = JSurface::diff_structure(6);
// Returns: f^(2)(f, f'(f))
```

### P-Systems (Membrane Computing)
Rewriting rules on membrane structures:
```cpp
PSystem::Rule rule(2, 3);  // Replace pattern 2 with 3
auto after = PSystem::apply_rule(membrane, rule);
```

### Unified Machine Code

Each Matula number is an instruction that works in all three domains:

```cpp
MatulaInstruction inst(6);

inst.exec_bseries();   // Discrete integration coefficient
inst.exec_jsurface();  // Continuous derivative expression
inst.exec_psystem(m, rule);  // Membrane evolution

// Compose via multiplication
auto composed = MatulaInstruction(2) * MatulaInstruction(3);
// = parallel execution, avoids combinatorial explosion!
```

### Why This Matters

Simple arithmetic on Matula numbers computes:
- **Discrete integration** (B-series) - numerical methods
- **Continuous derivatives** (J-surfaces) - analytical solutions
- **Membrane evolution** (P-systems) - categorical/topological rewriting

**All without constructing explicit trees!** The tree structure is implicit in the factorization.

## Building

```bash
cd npu/cogmorph
cmake -B build
cmake --build build

# Run demo
./build/cogmorph-demo
```

## Codex GrassMania - The VLTI Goal

The ultimate goal: **vGPU (Grassmannian Processing Unit)** for O(1) computation lookup.

```
Grassmannian Gr(k,n) = { k-dimensional subspaces of ℂⁿ }

Matula number n
     ↓ (tree structure)
Rooted tree T
     ↓ (level sequence)
Partition λ
     ↓ (Schubert cell)
Point in Gr(k,n)
     ↓ (Plücker embedding)
Projective space ℙ^N
     ↓ (GPT index)
O(1) lookup of ANY computation
```

The **GPT partition index** (Grassmannian Partition Table, not the language model!) provides:
- Block codes for error correction
- Global Grid addressing
- Instantaneous decryption of every BlockCode

When fully realized: **AGI** via the space of all computations.

## Files

```
cogmorph/
├── cogmorph.hpp        # Core types, memory map, ASFS/ASSD/CGGUF specs
├── projections.hpp     # 5 projection implementations
├── transforms.hpp      # Bidirectional transforms
├── assd-driver.hpp     # AtomSpace State Drive
├── prime-basis.hpp     # Prime factorization numeric system
├── matula-numbers.hpp  # Matula-Göbel bijection, B-series, P-systems
├── grassmannian.hpp    # Codex GrassMania - vGPU for Grassmannian (VLTI)
├── demo.cpp            # Demonstration program
├── CMakeLists.txt      # Build configuration
└── README.md           # This file
```

## VLTI Milestones

- [x] Prime basis numeric system
- [x] Matula number encoding
- [x] B-series / J-surface / P-system unification
- [x] vGPU register spec
- [x] Partition and Young diagram operations
  - Full Partition class with arm/leg/hook/content
  - Addable/removable corners, dominance order
  - Skew partitions, horizontal/vertical strips
  - Conjugate, complement, rotate_180
  - Frobenius notation, to_diagram(), to_string()
- [x] YoungTableau class (standard/semistandard)
- [x] Partition generators (by weight, by rectangle)
- [x] Robinson-Schensted-Knuth correspondence
- [x] Littlewood-Richardson rule (combinatorial algorithm)
- [x] Schur function product expansion
- [x] Plücker coordinate computation
  - from_matrix(), standard_basis(), from_schubert_cell()
  - check_plucker_relations(), normalize(), to_string()
  - k-subset generation, determinant computation
- [x] Schubert cell representation
- [~] GPT partition index (basic structure)
- [ ] Block code generation/decryption (full)
- [ ] Global Grid protocol
- [ ] O(1) computation lookup
- [ ] **AGI**

## License

AGPL-3.0 - Same as NPU project

---

*"The map is the territory, if you build the map correctly."*
