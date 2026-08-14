/**
 * @file bench_main.cpp
 * @brief Performance benchmarks for Modern OpenCog
 */

#include <opencog/atomspace/atomspace.hpp>
#include <opencog/attention/attention_bank.hpp>
#include <opencog/pattern/matcher.hpp>
#include <opencog/pln/formulas.hpp>
#include <opencog/pln/inference.hpp>

#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <iomanip>

using namespace opencog;
using namespace std::chrono;

// ============================================================================
// Benchmark Utilities
// ============================================================================

template<typename Func>
double benchmark(const std::string& name, Func&& func, int iterations = 1) {
    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        func();
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    double per_op = static_cast<double>(duration.count()) / iterations;

    std::cout << std::left << std::setw(40) << name
              << std::right << std::setw(12) << std::fixed << std::setprecision(2)
              << per_op << " us";

    if (iterations > 1) {
        std::cout << " (" << iterations << " iterations, "
                  << duration.count() / 1000.0 << " ms total)";
    }

    std::cout << "\n";

    return per_op;
}

// ============================================================================
// AtomSpace Benchmarks
// ============================================================================

void bench_atomspace() {
    std::cout << "\n=== AtomSpace Benchmarks ===\n\n";

    // Node creation
    {
        AtomSpace space;
        benchmark("Create 10,000 nodes", [&]() {
            for (int i = 0; i < 10000; ++i) {
                space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i));
            }
        });
        std::cout << "  Final size: " << space.size() << " atoms\n\n";
    }

    // Link creation
    {
        AtomSpace space;
        std::vector<Handle> nodes;
        for (int i = 0; i < 1000; ++i) {
            nodes.push_back(space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i)));
        }

        benchmark("Create 10,000 links", [&]() {
            for (int i = 0; i < 10000; ++i) {
                int a = i % nodes.size();
                int b = (i + 1) % nodes.size();
                space.add_link(AtomType::INHERITANCE_LINK, {nodes[a], nodes[b]});
            }
        });
        std::cout << "  Final size: " << space.size() << " atoms\n\n";
    }

    // Node lookup
    {
        AtomSpace space;
        for (int i = 0; i < 10000; ++i) {
            space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i));
        }

        benchmark("Lookup 10,000 nodes", [&]() {
            for (int i = 0; i < 10000; ++i) {
                space.get_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i));
            }
        });
    }

    // Duplicate detection
    {
        AtomSpace space;
        space.add_node(AtomType::CONCEPT_NODE, "Existing");

        benchmark("Detect 10,000 duplicates", [&]() {
            for (int i = 0; i < 10000; ++i) {
                space.add_node(AtomType::CONCEPT_NODE, "Existing");
            }
        });
    }

    // Incoming set query
    {
        AtomSpace space;
        Handle hub = space.add_node(AtomType::CONCEPT_NODE, "Hub");
        for (int i = 0; i < 1000; ++i) {
            Handle spoke = space.add_node(AtomType::CONCEPT_NODE, "Spoke" + std::to_string(i));
            space.add_link(AtomType::INHERITANCE_LINK, {spoke, hub});
        }

        benchmark("Query incoming set (1000 links)", [&]() {
            auto incoming = space.get_incoming(hub);
        }, 1000);
    }

    // Type index query
    {
        AtomSpace space;
        for (int i = 0; i < 5000; ++i) {
            space.add_node(AtomType::CONCEPT_NODE, "Concept" + std::to_string(i));
        }
        for (int i = 0; i < 5000; ++i) {
            space.add_node(AtomType::PREDICATE_NODE, "Predicate" + std::to_string(i));
        }

        benchmark("Get atoms by type (5000 concepts)", [&]() {
            auto concepts = space.get_atoms_by_type(AtomType::CONCEPT_NODE);
        }, 100);
    }
}

// ============================================================================
// Attention Benchmarks
// ============================================================================

void bench_attention() {
    std::cout << "\n=== Attention Benchmarks ===\n\n";

    // Stimulate
    {
        AtomSpace space;
        AttentionBank bank(space);

        std::vector<Handle> atoms;
        for (int i = 0; i < 1000; ++i) {
            atoms.push_back(space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i)));
        }

        benchmark("Stimulate 10,000 times", [&]() {
            for (int i = 0; i < 10000; ++i) {
                bank.stimulate(atoms[i % atoms.size()].id(), 1.0f);
            }
        });
    }

    // Spread activation
    {
        AtomSpace space;
        AttentionBank bank(space);

        // Create connected graph
        std::vector<Handle> nodes;
        for (int i = 0; i < 100; ++i) {
            nodes.push_back(space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i)));
        }
        for (int i = 0; i < 100; ++i) {
            for (int j = i + 1; j < std::min(i + 5, 100); ++j) {
                space.add_link(AtomType::SIMILARITY_LINK, {nodes[i], nodes[j]});
            }
        }

        // Stimulate some nodes
        for (int i = 0; i < 10; ++i) {
            bank.stimulate(nodes[i].id(), 100.0f);
        }

        benchmark("Spread activation (100 nodes)", [&]() {
            for (const auto& node : nodes) {
                bank.spread_activation(node.id());
            }
        }, 100);
    }

    // Get attentional focus
    {
        AtomSpace space;
        ECANConfig config;
        config.af_boundary = 10.0f;
        AttentionBank bank(space, config);

        for (int i = 0; i < 1000; ++i) {
            Handle h = space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i));
            bank.stimulate(h.id(), static_cast<float>(i % 100));
        }

        benchmark("Get attentional focus (1000 atoms)", [&]() {
            auto af = bank.get_attentional_focus();
        }, 100);
    }
}

// ============================================================================
// Pattern Matching Benchmarks
// ============================================================================

void bench_pattern() {
    std::cout << "\n=== Pattern Matching Benchmarks ===\n\n";

    // Simple type match
    {
        AtomSpace space;
        for (int i = 0; i < 10000; ++i) {
            space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i));
        }

        PatternMatcher matcher(space);

        Pattern pattern;
        pattern.body = typed(AtomType::CONCEPT_NODE);

        benchmark("Match all concepts (10000)", [&]() {
            size_t count = matcher.count_matches(pattern);
        }, 10);
    }

    // Link pattern match
    {
        AtomSpace space;
        std::vector<Handle> nodes;
        for (int i = 0; i < 100; ++i) {
            nodes.push_back(space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i)));
        }
        for (int i = 0; i < 1000; ++i) {
            int a = i % nodes.size();
            int b = (i + 17) % nodes.size();  // Some variation
            space.add_link(AtomType::INHERITANCE_LINK, {nodes[a], nodes[b]});
        }

        PatternMatcher matcher(space);

        Pattern pattern;
        pattern.body = link(AtomType::INHERITANCE_LINK, {
            typed(AtomType::CONCEPT_NODE),
            typed(AtomType::CONCEPT_NODE)
        });

        benchmark("Match link patterns (1000 links)", [&]() {
            auto results = matcher.find_all(pattern, 1000);
        }, 10);
    }

    // Variable binding
    {
        AtomSpace space;
        Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
        for (int i = 0; i < 100; ++i) {
            Handle species = space.add_node(AtomType::CONCEPT_NODE, "Species" + std::to_string(i));
            space.add_link(AtomType::INHERITANCE_LINK, {species, animal});
        }

        PatternMatcher matcher(space);

        Pattern pattern;
        pattern.variables = {"X"};
        pattern.body = link(AtomType::INHERITANCE_LINK, {
            var("X", AtomType::CONCEPT_NODE),
            ground(animal.id())
        });

        benchmark("Match with variable binding (100 matches)", [&]() {
            auto results = matcher.find_all(pattern);
        }, 100);
    }
}

// ============================================================================
// PLN Benchmarks
// ============================================================================

void bench_pln() {
    std::cout << "\n=== PLN Formula Benchmarks ===\n\n";

    // Scalar operations
    {
        TruthValue ab{0.8f, 0.9f};
        TruthValue bc{0.7f, 0.85f};

        benchmark("Deduction (scalar) x 100,000", [&]() {
            for (int i = 0; i < 100000; ++i) {
                auto result = pln::deduction(ab, bc, 0.5f, 0.5f);
                (void)result;
            }
        });
    }

    {
        TruthValue tv1{0.7f, 0.8f};
        TruthValue tv2{0.6f, 0.9f};

        benchmark("Revision (scalar) x 100,000", [&]() {
            for (int i = 0; i < 100000; ++i) {
                auto result = pln::revision(tv1, tv2);
                (void)result;
            }
        });
    }

    {
        TruthValue a{0.8f, 0.9f};
        TruthValue b{0.7f, 0.8f};

        benchmark("Fuzzy AND (scalar) x 100,000", [&]() {
            for (int i = 0; i < 100000; ++i) {
                auto result = pln::fuzzy_and(a, b);
                (void)result;
            }
        });
    }

    // Batch operations
    {
        std::vector<TruthValue> tv1(1000, TruthValue{0.7f, 0.8f});
        std::vector<TruthValue> tv2(1000, TruthValue{0.6f, 0.9f});
        std::vector<TruthValue> out(1000);

        benchmark("Batch revision (1000 TVs) x 1,000", [&]() {
            pln::batch_revision(tv1, tv2, out);
        }, 1000);
    }
}

// ============================================================================
// Memory Layout Benchmarks
// ============================================================================

void bench_memory() {
    std::cout << "\n=== Memory Layout Benchmarks ===\n\n";

    std::cout << "Type sizes:\n";
    std::cout << "  AtomId:          " << sizeof(AtomId) << " bytes\n";
    std::cout << "  TruthValue:      " << sizeof(TruthValue) << " bytes\n";
    std::cout << "  AttentionValue:  " << sizeof(AttentionValue) << " bytes\n";
    std::cout << "  AtomHeader:      " << sizeof(AtomHeader) << " bytes\n";
    std::cout << "  Handle:          " << sizeof(Handle) << " bytes\n";
    std::cout << "\n";

    // Memory access pattern
    {
        AtomSpace space;
        std::vector<Handle> handles;
        for (int i = 0; i < 10000; ++i) {
            handles.push_back(space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i)));
        }

        // Sequential access
        benchmark("Sequential TV access (10,000)", [&]() {
            float sum = 0;
            for (const auto& h : handles) {
                sum += space.get_tv(h).strength;
            }
        }, 100);

        // Random access
        std::vector<size_t> indices(10000);
        for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
        std::mt19937 rng(42);
        std::shuffle(indices.begin(), indices.end(), rng);

        benchmark("Random TV access (10,000)", [&]() {
            float sum = 0;
            for (size_t idx : indices) {
                sum += space.get_tv(handles[idx]).strength;
            }
        }, 100);
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "Modern OpenCog Performance Benchmarks\n";
    std::cout << "============================================================\n";

    bench_memory();
    bench_atomspace();
    bench_attention();
    bench_pattern();
    bench_pln();

    std::cout << "\n============================================================\n";
    std::cout << "Benchmarks complete.\n";
    std::cout << "============================================================\n";

    return 0;
}
