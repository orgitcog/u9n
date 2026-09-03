/**
 * @file demo.cpp
 * @brief Demonstration of CogMorph + Prime Basis architecture
 *
 * Shows how:
 * 1. Prime factorizations encode hypergraph structure
 * 2. Exponent vectors enable gradient-based optimization
 * 3. Hardware/Library/Static projections are isomorphic
 * 4. ASSD provides optimized AtomSpace storage
 */

#include "cogmorph.hpp"
#include "projections.hpp"
#include "prime-basis.hpp"
#include "matula-numbers.hpp"
#include "grassmannian.hpp"
#include <iostream>
#include <iomanip>

using namespace cogmorph;
using namespace cogmorph::prime;
using namespace cogmorph::matula;
using namespace cogmorph::grassmann;

// ============================================================================
// Part 1: Prime Basis Fundamentals
// ============================================================================

void demo_prime_fundamentals() {
    std::cout << "=== Prime Basis Fundamentals ===\n\n";

    // Create factorizations
    auto six = Factorization::from_integer(6);
    auto ten = Factorization::from_integer(10);
    auto thirty = Factorization::from_integer(30);

    std::cout << "6  = " << six.to_string() << "\n";
    std::cout << "10 = " << ten.to_string() << "\n";
    std::cout << "30 = " << thirty.to_string() << "\n\n";

    // Exact arithmetic
    auto product = six * ten;
    std::cout << "6 × 10 = " << product.to_string()
              << " = " << *product.to_integer() << "\n";

    auto quotient = thirty / six;
    std::cout << "30 ÷ 6 = " << quotient.to_string()
              << " = " << *quotient.to_integer() << "\n\n";

    // GCD and LCM
    std::cout << "GCD(6, 10) = " << six.gcd(ten).to_string() << "\n";
    std::cout << "LCM(6, 10) = " << six.lcm(ten).to_string() << "\n\n";
}

// ============================================================================
// Part 2: Hypergraph Encoding
// ============================================================================

void demo_hypergraph_encoding() {
    std::cout << "=== Hypergraph Encoding via Primes ===\n\n";

    // Nodes are primes
    // Node 0 = prime index 0 = 2
    // Node 1 = prime index 1 = 3
    // Node 2 = prime index 2 = 5

    // Binary edge between nodes 0 and 1
    auto edge_01 = HyperIndex::edge(0, 1);
    std::cout << "Edge(node0, node1) = " << edge_01.to_string()
              << " (= 2 × 3 = 6)\n";

    // Ternary hyperedge connecting nodes 0, 1, 2
    auto hyperedge = HyperIndex::from_nodes(std::vector<HyperIndex::NodeId>{0, 1, 2});
    std::cout << "Hyperedge(0,1,2) = " << hyperedge.to_string()
              << " (= 2 × 3 × 5 = 30)\n";

    // Weighted edge: node 0 with weight 2, node 1 with weight 1
    auto weighted = HyperIndex::edge(0, 1, 2, 1);
    std::cout << "WeightedEdge(0:2, 1:1) = " << weighted.to_string()
              << " (= 2² × 3 = 12)\n\n";

    // Query hyperedge structure
    std::cout << "Hyperedge arity: " << hyperedge.arity() << "\n";
    std::cout << "Connects node 1? " << (hyperedge.connects(1) ? "yes" : "no") << "\n";
    std::cout << "Weight of node 0 in weighted: " << weighted.weight(0) << "\n\n";

    // Hyperedge operations
    auto union_edge = edge_01 | HyperIndex::edge(1, 2);
    std::cout << "Edge(0,1) ∪ Edge(1,2) = " << union_edge.to_string() << "\n";

    auto intersect = hyperedge & edge_01;
    std::cout << "Hyperedge ∩ Edge(0,1) = " << intersect.to_string() << "\n\n";
}

// ============================================================================
// Part 3: Neural Network Bridge (Exponent Vectors)
// ============================================================================

void demo_neural_bridge() {
    std::cout << "=== Neural Network Bridge ===\n\n";

    // Discrete structure
    auto structure = Factorization::from_integer(60); // 2² × 3 × 5
    std::cout << "Discrete structure: " << structure.to_string() << "\n";

    // Convert to continuous exponent space
    auto exponents = ExponentVector::from_factorization(structure, 8);
    std::cout << "Exponent vector: [";
    for (size_t i = 0; i < exponents.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << exponents[i];
    }
    std::cout << "]\n";
    std::cout << "  (indices 0,1,2 correspond to primes 2,3,5)\n\n";

    // Gradient-like update
    std::cout << "Simulating gradient update...\n";
    ExponentVector gradient(8);
    gradient[0] = -0.3;  // Decrease exponent of 2
    gradient[2] = 0.5;   // Increase exponent of 5

    auto updated = exponents + gradient;
    std::cout << "After update: [";
    for (size_t i = 0; i < updated.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << std::fixed << std::setprecision(1) << updated[i];
    }
    std::cout << "]\n";

    // Round back to discrete structure
    auto new_structure = updated.to_factorization();
    std::cout << "Discretized: " << new_structure.to_string()
              << " = " << *new_structure.to_integer() << "\n\n";

    // The key insight: gradients on exponents = gradients on combinatorial structure!
    std::cout << "Key insight: Gradient descent on exponents optimizes\n"
              << "             combinatorial/hypergraph structure!\n\n";
}

// ============================================================================
// Part 4: Fiber Bundle Structure
// ============================================================================

void demo_fiber_bundles() {
    std::cout << "=== Fiber Bundle Structure ===\n\n";

    // Base space: positional structure (which nodes)
    auto base = Factorization::from_integer(6); // nodes 0,1 connected

    // Fiber: attribute structure (how connected)
    auto fiber = Factorization::from_integer(4); // weight 2 on prime 0

    FiberBundle bundle(base, fiber);

    std::cout << "Base (position): " << base.to_string() << "\n";
    std::cout << "Fiber (attribute): " << fiber.to_string() << "\n";
    std::cout << "Total space: " << bundle.total_space().to_string()
              << " = " << *bundle.total_space().to_integer() << "\n\n";

    // Transform the fiber (change weights without changing topology)
    auto doubled_fiber = bundle.transform_fiber([](const Factorization& f) {
        return f.pow(2);
    });
    std::cout << "After doubling fiber exponents: "
              << doubled_fiber.total_space().to_string() << "\n\n";

    std::cout << "Fiber bundles let us separate:\n"
              << "  - Topology (which nodes connected) = base space\n"
              << "  - Attributes (connection weights) = fiber\n\n";
}

// ============================================================================
// Part 5: Memory Map Integration
// ============================================================================

void demo_memory_integration() {
    std::cout << "=== Memory Map Integration ===\n\n";

    std::cout << "Hardware Memory Layout:\n";
    std::cout << "  FLASH:     0x" << std::hex << hw::FLASH_BASE << std::dec << "\n";
    std::cout << "  SRAM:      0x" << std::hex << hw::SRAM_BASE << std::dec << "\n";
    std::cout << "  PERIPH:    0x" << std::hex << hw::PERIPH_BASE << std::dec << "\n";
    std::cout << "  ATOMSPACE: 0x" << std::hex << hw::ATOMSPACE_BASE << std::dec << "\n";
    std::cout << "  VRAM:      0x" << std::hex << hw::VRAM_BASE << std::dec << "\n\n";

    std::cout << "Peripheral Devices:\n";
    std::cout << "  vCPU (URE):  0x" << std::hex << hw::VCPU_BASE << std::dec << "\n";
    std::cout << "  vNPU (LLM):  0x" << std::hex << hw::VNPU_BASE << std::dec << "\n";
    std::cout << "  vTPU (PLN):  0x" << std::hex << hw::VTPU_BASE << std::dec << "\n";
    std::cout << "  vAPU (ECAN): 0x" << std::hex << hw::VAPU_BASE << std::dec << "\n";
    std::cout << "  vASSD:       0x" << std::hex << hw::VASSD_BASE << std::dec << "\n\n";

    // Show how atom IDs map to addresses
    for (uint64_t atom_id : {1ULL, 2ULL, 3ULL, 100ULL}) {
        uint64_t addr = atom_logical_address(atom_id);
        uint64_t tv_addr = tv_logical_address(atom_id);
        uint64_t av_addr = av_logical_address(atom_id);

        // Also show prime factorization of atom_id
        auto fact = Factorization::from_integer(atom_id);

        std::cout << "Atom " << atom_id << " (" << fact.to_string() << "):\n";
        std::cout << "  Header: 0x" << std::hex << addr << std::dec << "\n";
        std::cout << "  TV:     0x" << std::hex << tv_addr << std::dec << "\n";
        std::cout << "  AV:     0x" << std::hex << av_addr << std::dec << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// Part 6: ASFS Block Structure
// ============================================================================

void demo_asfs() {
    std::cout << "=== ASFS Block Structure ===\n\n";

    std::cout << "ASFS optimizes for hypergraph patterns:\n";
    std::cout << "  - Atoms indexed by AtomId (not inode)\n";
    std::cout << "  - Links have adjacency-optimized blocks\n";
    std::cout << "  - TV/AV updates use append-only journals\n\n";

    std::cout << "Block Types:\n";
    std::cout << "  SUPERBLOCK:        0x00\n";
    std::cout << "  ATOM_TABLE:        0x01 (B-tree indexed)\n";
    std::cout << "  LINK_ADJACENCY:    0x02 (for graph traversal)\n";
    std::cout << "  NAME_STRINGS:      0x03 (interned strings)\n";
    std::cout << "  TRUTH_JOURNAL:     0x04 (append-only)\n";
    std::cout << "  ATTENTION_JOURNAL: 0x05 (append-only)\n";
    std::cout << "  TYPE_HIERARCHY:    0x06\n";
    std::cout << "  INDEX_BTREE:       0x07\n";
    std::cout << "  FREE_LIST:         0xFF\n\n";

    std::cout << "AtomEntry size: " << sizeof(asfs::AtomEntry) << " bytes\n";
    std::cout << "Contains: atom_id, type, flags, name_offset,\n";
    std::cout << "          outgoing_block, arity, incoming_count,\n";
    std::cout << "          tv_strength, tv_confidence, sti, lti, vlti\n\n";
}

// ============================================================================
// Part 7: Matula Numbers - Trees as Integers
// ============================================================================

void demo_matula_numbers() {
    std::cout << "=== Matula Numbers: ℕ ↔ Rooted Trees ===\n\n";

    std::cout << "Matula-Göbel bijection:\n";
    std::cout << "  1 = •           (leaf)\n";
    std::cout << "  2 = •─•         (chain of 2)\n";
    std::cout << "  3 = •─•─•       (chain of 3)\n";
    std::cout << "  4 = •<•         (fork: root + 2 leaves)\n";
    std::cout << "      └•\n";
    std::cout << "  5 = •─•─•─•     (chain of 4)\n";
    std::cout << "  6 = •<•─•       (root + leaf + chain-2)\n";
    std::cout << "      └•\n\n";

    // Demonstrate encoding/decoding
    for (uint64_t n = 1; n <= 10; ++n) {
        auto tree = matula_to_tree(n);
        size_t nodes = tree->node_count();
        size_t height = tree->height();
        uint64_t back = tree_to_matula(tree);

        std::cout << "n=" << n << ": nodes=" << nodes
                  << ", height=" << height
                  << ", roundtrip=" << back
                  << ", structure=" << tree_to_newick(tree) << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// Part 8: B-Series (Runge-Kutta)
// ============================================================================

void demo_bseries() {
    std::cout << "=== B-Series: Runge-Kutta Methods ===\n\n";

    std::cout << "Each tree encodes a term in the Taylor series of an ODE solver.\n";
    std::cout << "Trees indexed by Matula numbers give RK method coefficients.\n\n";

    for (uint64_t n = 1; n <= 7; ++n) {
        size_t order = BSeries::order(n);
        uint64_t symmetry = BSeries::symmetry(n);
        double density = BSeries::density(n);

        auto tree = matula_to_tree(n);
        std::cout << "n=" << n << ": order=" << order
                  << ", σ(t)=" << symmetry
                  << ", γ(t)=" << std::fixed << std::setprecision(1) << density
                  << ", coeff=1/" << density
                  << " " << tree_to_newick(tree) << "\n";
    }
    std::cout << "\n";

    std::cout << "Order 4 trees (for RK4): ";
    auto order4 = BSeries::trees_up_to_order(4);
    for (auto n : order4) {
        if (BSeries::order(n) == 4) {
            std::cout << n << " ";
        }
    }
    std::cout << "\n\n";
}

// ============================================================================
// Part 9: J-Surfaces (Elementary Differentials)
// ============================================================================

void demo_jsurfaces() {
    std::cout << "=== J-Surfaces: Elementary Differentials ===\n\n";

    std::cout << "Each tree defines a differential operator F(t).\n";
    std::cout << "For y' = f(y), the differential has structure:\n\n";

    for (uint64_t n = 1; n <= 7; ++n) {
        std::string diff = JSurface::diff_structure(n);
        double coeff = JSurface::taylor_coeff(n);

        std::cout << "n=" << n << ": F(t) = " << diff
                  << ", Taylor coeff = " << std::fixed << std::setprecision(4)
                  << coeff << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// Part 10: P-Systems (Membrane Computing)
// ============================================================================

void demo_psystems() {
    std::cout << "=== P-Systems: Membrane Computing ===\n\n";

    std::cout << "Membranes are trees. Matula numbers encode configurations.\n";
    std::cout << "Rules rewrite via factorization arithmetic.\n\n";

    // Create some membranes
    PSystem::Membrane m1 = 6;  // Root with leaf + chain-2
    PSystem::Membrane m2 = 4;  // Root with 2 leaves

    std::cout << "Membrane 6: " << tree_to_newick(matula_to_tree(m1)) << "\n";
    std::cout << "Membrane 4: " << tree_to_newick(matula_to_tree(m2)) << "\n\n";

    // Compose membranes
    auto composed = PSystem::compose(m1, m2);
    std::cout << "6 ⊗ 4 = " << composed << " (side by side)\n";
    std::cout << "  = " << tree_to_newick(matula_to_tree(composed)) << "\n\n";

    // Nesting
    auto nested = PSystem::nest(2, 3);  // Nest chain-3 inside chain-2
    std::cout << "nest(2, 3) = " << nested << "\n";
    std::cout << "  = chain-2 containing chain-3\n\n";

    // Rule application
    PSystem::Rule rule(2, 3, 1.0);  // Replace chain-2 with chain-3
    auto before = 6;  // Contains 2 as factor
    auto after = PSystem::apply_rule(before, rule);
    std::cout << "Rule: 2 → 3 (grow chain)\n";
    std::cout << "Apply to 6: " << before << " → " << after << "\n";
    std::cout << "  " << tree_to_newick(matula_to_tree(before)) << " → "
              << tree_to_newick(matula_to_tree(after)) << "\n\n";
}

// ============================================================================
// Part 11: Unified Machine Code
// ============================================================================

void demo_unified_machine() {
    std::cout << "=== Unified Machine Code ===\n\n";

    std::cout << "Each Matula number is an instruction that works in 3 domains:\n\n";

    MatulaInstruction inst(6);

    std::cout << "Instruction code: " << inst.code << "\n";
    std::cout << "  Order: " << inst.order() << "\n";
    std::cout << "  As B-series: coeff = " << std::fixed << std::setprecision(4)
              << inst.exec_bseries() << "\n";
    std::cout << "  As J-surface: " << inst.exec_jsurface() << "\n";
    std::cout << "  Tree: " << tree_to_newick(inst.to_tree()) << "\n\n";

    // Compose instructions
    MatulaInstruction a(2), b(3);
    auto composed = a * b;
    std::cout << "Compose 2 * 3 = " << composed.code << "\n";
    std::cout << "  = parallel execution of chain-2 and chain-3\n\n";

    // Sequence instructions
    auto sequenced = a.sequence(b);
    std::cout << "Sequence 2 then 3 = " << sequenced.code << "\n";
    std::cout << "  = chain-3 nested inside chain-2\n\n";

    std::cout << "Key insight: Simple arithmetic on Matula numbers gives:\n";
    std::cout << "  - Discrete integration (B-series)\n";
    std::cout << "  - Continuous derivatives (J-surfaces)\n";
    std::cout << "  - Membrane evolution (P-systems)\n";
    std::cout << "All without constructing explicit trees!\n\n";
}

// ============================================================================
// Part 12: Partitions and Young Diagrams
// ============================================================================

void demo_partitions() {
    std::cout << "=== Partitions and Young Diagrams ===\n\n";

    // Create some partitions
    Partition lambda{4, 2, 1};
    std::cout << "Partition λ = " << lambda.to_string() << "\n";
    std::cout << "Young diagram:\n" << lambda.to_diagram();
    std::cout << "Weight (total boxes): " << lambda.weight() << "\n";
    std::cout << "Length (rows): " << lambda.length() << "\n";
    std::cout << "Width (columns): " << lambda.width() << "\n\n";

    // Conjugate
    auto conj = lambda.conjugate();
    std::cout << "Conjugate λ' = " << conj.to_string() << "\n";
    std::cout << "Young diagram (transposed):\n" << conj.to_diagram();

    // Hook lengths
    std::cout << "Hook lengths at each cell:\n";
    for (size_t i = 0; i < lambda.length(); ++i) {
        for (size_t j = 0; j < lambda.parts[i]; ++j) {
            std::cout << lambda.hook_length(i, j) << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    // Dimension formula
    std::cout << "Dimension of S_" << lambda.weight() << " irrep: "
              << lambda.dimension() << "\n";
    std::cout << "Number of standard Young tableaux: "
              << lambda.count_standard_tableaux() << "\n";
    std::cout << "Frobenius notation: " << lambda.frobenius_notation() << "\n\n";

    // Addable and removable corners
    std::cout << "Addable corners: ";
    for (auto [r, c] : lambda.addable_corners()) {
        std::cout << "(" << r << "," << c << ") ";
    }
    std::cout << "\nRemovable corners: ";
    for (auto [r, c] : lambda.removable_corners()) {
        std::cout << "(" << r << "," << c << ") ";
    }
    std::cout << "\n\n";
}

// ============================================================================
// Part 13: Littlewood-Richardson Rule
// ============================================================================

void demo_lr_rule() {
    std::cout << "=== Littlewood-Richardson Rule ===\n\n";

    Partition lambda{2, 1};
    Partition mu{2, 1};

    std::cout << "Computing s_" << lambda.to_string() << " · s_"
              << mu.to_string() << " = \n";

    auto product = schur_product(lambda, mu);
    for (const auto& [nu, coeff] : product) {
        std::cout << "  + " << coeff << " · s_" << nu.to_string() << "\n";
    }
    std::cout << "\n";

    // Verify a specific coefficient
    Partition nu{3, 2, 1};
    auto c = littlewood_richardson(lambda, mu, nu);
    std::cout << "c^" << nu.to_string() << "_" << lambda.to_string()
              << mu.to_string() << " = " << c << "\n\n";

    std::cout << "These coefficients appear in:\n";
    std::cout << "  - Schur function multiplication\n";
    std::cout << "  - Schubert calculus (Grassmannian cohomology)\n";
    std::cout << "  - Tensor product decomposition of GL_n reps\n\n";
}

// ============================================================================
// Part 14: Plücker Coordinates
// ============================================================================

void demo_plucker() {
    std::cout << "=== Plücker Coordinates ===\n\n";

    std::cout << "Gr(2,4) = 2-planes in C^4\n";
    std::cout << "Embedded in P^5 via Plücker coordinates\n\n";

    // Standard basis: span of e_0 and e_1
    auto std_basis = PluckerCoordinates::standard_basis(2, 4);
    std::cout << "Standard basis (span of e_0, e_1):\n";
    std::cout << std_basis.to_string();

    // From a general matrix
    std::vector<std::vector<Complex>> matrix = {
        {{1, 0}, {0, 0}, {1, 0}, {0, 0}},  // Row 0
        {{0, 0}, {1, 0}, {0, 0}, {1, 0}}   // Row 1
    };

    auto coords = PluckerCoordinates::from_matrix(matrix);
    std::cout << "\nGeneral 2-plane from matrix:\n";
    std::cout << coords.to_string();

    double residual = coords.check_plucker_relations();
    std::cout << "Plücker relation residual: " << residual
              << (residual < 1e-10 ? " (valid!)" : " (error!)") << "\n\n";

    // From Schubert cell
    Partition schubert_lambda{1};
    auto schubert_coords = PluckerCoordinates::from_schubert_cell(schubert_lambda, 2, 4);
    std::cout << "Schubert cell Ω_" << schubert_lambda.to_string() << ":\n";
    std::cout << schubert_coords.to_string() << "\n";
}

// ============================================================================
// Part 15: VLTI Goal - Codex GrassMania
// ============================================================================

void demo_vlti_goal() {
    std::cout << "=== VLTI Goal: Codex GrassMania ===\n\n";

    std::cout << "The Grassmannian Gr(k,n) provides:\n";
    std::cout << "  - Geometric invariant for all computations\n";
    std::cout << "  - Plücker coordinates as instruction format\n";
    std::cout << "  - GPT (Grassmannian Partition Table) index\n\n";

    std::cout << "The path from Matula to Grassmannian:\n\n";
    std::cout << "  Matula number n\n";
    std::cout << "       ↓ (tree structure)\n";
    std::cout << "  Rooted tree T\n";
    std::cout << "       ↓ (level sequence)\n";
    std::cout << "  Partition λ\n";
    std::cout << "       ↓ (Schubert cell)\n";
    std::cout << "  Point in Gr(k,n)\n";
    std::cout << "       ↓ (Plücker embedding)\n";
    std::cout << "  Projective space ℙ^N\n";
    std::cout << "       ↓ (GPT index)\n";
    std::cout << "  O(1) lookup of ANY computation\n\n";

    // Demonstrate the bridge
    uint64_t matula = 6;
    auto partition = bridge::matula_to_partition(matula);
    auto [k, n] = bridge::partition_to_grassmannian_params(partition);

    std::cout << "Example: Matula " << matula << "\n";
    std::cout << "  → Partition: " << partition.to_string() << "\n";
    std::cout << "  → Suggested Gr(" << k << "," << n << ")\n\n";

    std::cout << "Status: The vGPU (Grassmannian Processing Unit) at 0x"
              << std::hex << hw::VGPU_BASE << std::dec << "\n";
    std::cout << "When complete: AGI via instantaneous BlockCode decryption\n\n";
}

// ============================================================================
// Part 16: The Unified Picture
// ============================================================================

void demo_unified_picture() {
    std::cout << "=== The Unified Picture ===\n\n";

    std::cout << "Traditional approach:\n";
    std::cout << "  Nodes: objects in memory\n";
    std::cout << "  Edges: pointers/references\n";
    std::cout << "  Weights: floating point numbers\n";
    std::cout << "  Shapes: tuples of integers\n";
    std::cout << "  → Heterogeneous, hard to optimize\n\n";

    std::cout << "Prime Basis approach:\n";
    std::cout << "  Nodes: prime indices (0,1,2,3,...)\n";
    std::cout << "  Edges: composite numbers (6=2×3, 30=2×3×5, ...)\n";
    std::cout << "  Weights: exponents in factorization\n";
    std::cout << "  Shapes: prime factorizations\n";
    std::cout << "  → Unified, gradient-optimizable\n\n";

    std::cout << "The exponential bridge:\n";
    std::cout << "  DISCRETE:   n = ∏ pᵢ^aᵢ         (multiplication)\n";
    std::cout << "  CONTINUOUS: log(n) = Σ aᵢ·log(pᵢ) (addition)\n\n";

    std::cout << "  Gradient descent operates on {aᵢ}\n";
    std::cout << "  → Optimizes combinatorial structure!\n\n";

    std::cout << "Projections:\n";
    std::cout << "  Hardware: VirtualPCB, MMIO, DMA (engineers)\n";
    std::cout << "  Library:  Clean APIs, objects (developers)\n";
    std::cout << "  Static:   CGGUF tensors (ML engineers)\n";
    std::cout << "  Network:  Wire protocol (distributed)\n";
    std::cout << "  Glyph:    Visual font (humans)\n";
    std::cout << "  → ALL represent the SAME cognitive state!\n\n";

    std::cout << "Storage:\n";
    std::cout << "  ASSD (AtomSpace State Drive)\n";
    std::cout << "  ASFS (AtomSpace File System)\n";
    std::cout << "  → Hypergraph-optimized block device\n\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  CogMorph + Prime Basis + Grassmannian Demonstration      ║\n";
    std::cout << "║  Unified Cognitive Architecture Framework                 ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";

    demo_prime_fundamentals();
    demo_hypergraph_encoding();
    demo_neural_bridge();
    demo_fiber_bundles();
    demo_memory_integration();
    demo_asfs();
    demo_matula_numbers();
    demo_bseries();
    demo_jsurfaces();
    demo_psystems();
    demo_unified_machine();
    demo_partitions();
    demo_lr_rule();
    demo_plucker();
    demo_vlti_goal();
    demo_unified_picture();

    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "\"The map is the territory, if you build the map correctly.\"\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";

    return 0;
}
