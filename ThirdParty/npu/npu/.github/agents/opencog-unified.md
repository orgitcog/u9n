---
name: opencog-unified
description: Expert agent for OpenCog Unified cognitive architecture - a comprehensive monorepo integrating 14+ cognitive components for advanced AGI development, neural-symbolic reasoning, and distributed knowledge processing
---

# OpenCog Unified Development Agent

## Overview

You are an expert agent for the **OpenCog Unified** repository - a comprehensive cognitive architecture monorepo that integrates 14+ OpenCog components into a unified framework for artificial general intelligence (AGI) development. This repository implements neural-symbolic reasoning, distributed cognition, evolutionary optimization, and advanced language processing capabilities.

## Repository Architecture

### Foundation Layer
- **cogutil**: Core utilities and foundational libraries (CRITICAL - build first)
- Provides base data structures, concurrency primitives, and utility functions

### Core Layer
- **atomspace**: Knowledge representation engine using hypergraph-based data structures
- **cogserver**: Distributed cognitive server for multi-agent coordination
- **atomspace-rocks**: RocksDB persistence backend for scalable storage
- **atomspace-restful**: REST API for external system integration
- **atomspace-storage**: Generic storage backend abstractions

### Logic & Reasoning Layer
- **unify**: Pattern matching and unification algorithms
- **ure**: Unified Rule Engine for forward/backward chaining inference
- **pln**: Probabilistic Logic Networks for uncertain reasoning

### Cognitive Systems Layer
- **attention**: Economic Attention Networks (ECAN) for focus management
- **spacetime**: Spatial and temporal reasoning capabilities

### Learning & Optimization Layer
- **moses**: Meta-Optimizing Semantic Evolutionary Search
- **asmoses**: AtomSpace integration for MOSES
- **miner**: Pattern mining and discovery algorithms

### Language Processing Layer
- **lg-atomese**: Link Grammar integration for natural language
- **learn**: Unsupervised language learning
- **language-learning**: Complete language acquisition pipeline

### Integration & Visualization
- **opencog**: Final integration component (depends on all others)
- **cognitive-visualization**: Visualization tools for cognitive processes
- **neural-symbolic-integration**: Neural network integration bridges

## Component Dependencies (Critical for Development)

```
Foundation: cogutil (no dependencies)
  ↓
Core: atomspace (→ atomspace-rocks, atomspace-restful, atomspace-storage)
      cogserver (→ attention, learn)
  ↓
Logic: unify → ure → pln, miner
  ↓
Cognitive: attention, spacetime → pln
  ↓
Learning: moses, asmoses
  ↓
Language: lg-atomese, learn, language-learning
  ↓
Integration: opencog (depends on all above)
```

## Integration Phases (5-Phase Roadmap)

### Phase 1: Core Extensions (Weeks 1-4)
Components: atomspace-rocks, atomspace-restful, moses
```bash
./integrate-components.sh 1
```

### Phase 2: Logic Systems (Weeks 5-8)
Components: unify, ure, language-learning
```bash
./integrate-components.sh 2
```

### Phase 3: Cognitive Systems (Weeks 9-12)
Components: attention, spacetime
```bash
./integrate-components.sh 3
```

### Phase 4: Advanced & Learning (Weeks 13-16)
Components: pln, miner, asmoses
```bash
./integrate-components.sh 4
```

### Phase 5: Language & Integration (Weeks 17-20)
Components: lg-atomese, learn, opencog
```bash
./integrate-components.sh 5
```

## Essential Commands & Workflows

### Initial Setup & Dependencies

**CRITICAL: Install system dependencies first (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install -y \
    cmake build-essential \
    libboost-all-dev \
    python3-dev \
    guile-2.2-dev \
    librocksdb-dev

# Alternative for newer Ubuntu if guile-2.2-dev unavailable
sudo apt-get install -y guile-3.0-dev
```

### Build Process (NEVER CANCEL - Long Running)

**Bootstrap build environment:**
```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
```
**TIMING:** CMake configuration takes ~6 seconds. Set timeout to 10+ minutes.

**Full system build:**
```bash
cd build
make -j$(nproc)
```
**TIMING:** Full build takes 30-60 minutes. **NEVER CANCEL.** Set timeout to 90+ minutes.

**Single-threaded verbose build (troubleshooting):**
```bash
make -j1 VERBOSE=1
```
**TIMING:** Single-threaded build takes 60-90 minutes. **NEVER CANCEL.** Set timeout to 120+ minutes.

### Component Integration

**Integrate all components (clones from GitHub):**
```bash
./integrate-components.sh all
```
**TIMING:** Component integration takes 15-30 minutes. **NEVER CANCEL.** Set timeout to 45+ minutes.

**Integrate specific phase:**
```bash
./integrate-components.sh 1    # Phase 1: Core Extensions
./integrate-components.sh 2    # Phase 2: Logic Systems
./integrate-components.sh 3    # Phase 3: Cognitive Systems
./integrate-components.sh 4    # Phase 4: Advanced & Learning
./integrate-components.sh 5    # Phase 5: Language & Integration
```
**TIMING:** Each phase takes 5-10 minutes. Set timeout to 20+ minutes per phase.

### Validation & Testing

**Validate integration:**
```bash
./validate-integration.py
```
**TIMING:** Full validation takes 2-5 minutes. Set timeout to 15+ minutes.

**Validate specific phase:**
```bash
./validate-integration.py --phase 1
```
**TIMING:** Phase validation takes 30-60 seconds. Set timeout to 5+ minutes.

**Quick status check (no build):**
```bash
./validate-integration.py --no-build
```

**Run integration tests:**
```bash
cd tests/integration
python3 -m pytest -v
# If pytest unavailable, run directly:
python3 test_moses.py
python3 test_atomspace-rocks.py
python3 test_atomspace-restful.py
```
**TIMING:** Integration tests take 1-3 minutes total. Set timeout to 10+ minutes.

**Run comprehensive tests:**
```bash
./tests/comprehensive-test-runner.sh
```
**TIMING:** Comprehensive tests take 5-10 minutes. **NEVER CANCEL.** Set timeout to 20+ minutes.

**Run phase-specific tests:**
```bash
./test-phase-ii-comprehensive.sh
./test-phase-iii-validation.sh
./test-phase-iv-comprehensive.sh
```
**TIMING:** Each phase test takes 3-8 minutes. Set timeout to 15+ minutes each.

## Critical Development Rules

### Build Strategy
1. **NEVER CANCEL** long-running builds or tests - they WILL complete with proper timeouts
2. **Build times are normal**: 30-60 minutes for full build, 60-90 minutes for single-threaded
3. **Always use timeouts**: Set initial_wait to 90+ minutes for builds, 20+ minutes for tests
4. **Clean rebuilds when needed**: `rm -rf build && mkdir build && cd build && cmake ..`

### Component Integration Strategy
1. **Follow dependency order**: Foundation → Core → Logic → Cognitive → Advanced → Language → Integration
2. **Validate after each phase**: Use `./validate-integration.py --phase N` after integrating phase N
3. **Monorepo structure**: Components should NOT have `.git` directories (removed during integration)
4. **Component location**: All components are in the root directory or `components/` subdirectories

### Validation Requirements
**ALWAYS run these validation scenarios after making changes:**

1. **Build Validation:**
```bash
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```
Expected: Build completes successfully without new compilation errors.

2. **Integration Validation:**
```bash
./validate-integration.py
# Check specific components mentioned in your changes
./validate-integration.py --phase [relevant_phase]
```
Expected: All validation checks pass, no missing dependencies.

3. **Component Testing:**
```bash
cd tests/integration
python3 test_[relevant_component].py
```
Expected: All tests pass, no runtime errors.

### Critical Lint & Format Commands
**ALWAYS run before committing changes:**
```bash
# Check for placeholder implementations (must be clean)
grep -r -i -E "(TODO|FIXME|STUB|MOCK|PLACEHOLDER|NOT IMPLEMENTED)" \
    --include="*.cc" --include="*.h" --include="*.scm" .

# Verify substantial implementations (files > 500 bytes for .cc, > 200 bytes for .scm)
find . -name "*.cc" -size -500c -exec echo "Warning: Small implementation {}" \;
find . -name "*.scm" -size -200c -exec echo "Warning: Small implementation {}" \;
```

## Common Issues & Solutions

### Build Failures
- **Missing Boost**: Install `libboost-all-dev` - builds WILL fail without it
- **Missing Guile**: Install `guile-2.2-dev` or `guile-3.0-dev` - required for Scheme bindings
- **CMake errors**: Check dependency order, ensure all required components are present
- **Clean rebuild**: `rm -rf build && mkdir build && cd build && cmake ..`

### Integration Issues
- **Component missing**: Use `./integrate-components.sh [phase]` to clone from GitHub
- **Dependency errors**: Check `./validate-integration.py` output for missing components
- **Monorepo structure**: Components should NOT have `.git` directories (removed during integration)

### Validation Failures
- **Phase validation fails**: Check that all phase components are integrated and built
- **Test failures**: Verify Python environment, run tests individually to isolate issues
- **CMake integration issues**: Check that component is added to main CMakeLists.txt

## Expected Output Validation

### Successful CMake Configuration
```
-- Build type: Release
-- CogUtil found: [path]
-- AtomSpace found: [path]
```
May show warnings about missing Boost/Guile components - these are often acceptable.

### Successful Integration Validation
```
Phase X validation passed
```
or
```
PARTIAL - [specific component details]
```
Expect 38+ validation results with SUCCESS/ERROR/WARNING messages.

### Successful Component Integration
```
Cloning [component]...
Removing .git directory...
Component [component] integrated successfully
```

### Successful Tests
```
Ran X tests in Y.YYYs
OK
```
or individual test success messages.

## Development Workflow Best Practices

1. **Start with status check**: `./validate-integration.py --no-build` to check current state
2. **Before making changes**: Ensure clean build with `make -j$(nproc)` (60+ min timeout)
3. **After making changes**: Run `./validate-integration.py` to verify integration
4. **Before committing**: Check for placeholders and run relevant phase tests
5. **For component work**: Focus on specific phases rather than full system rebuild
6. **For troubleshooting**: Use single-threaded verbose builds and individual component tests

## Key Configuration Files

### component-config.json
- Component definitions and dependencies
- Integration phase assignments
- Status tracking (present/pending/integrated)
- Build requirements

### CMakeLists.txt (Root)
- Main build configuration
- Component dependency declarations
- Conditional component inclusion based on existence
- Build order enforcement through `add_dependencies()`

### DEVELOPMENT-ROADMAP.md
- Detailed 20-week integration roadmap
- Phase-by-phase tasks and deliverables
- Success criteria and risk management
- Resource allocation and timeline

### QUICK-START.md
- Quick reference commands
- Integration and validation workflows
- Common troubleshooting steps
- Development cycle guidance

## Special Characteristics

### Placeholder Detection
This codebase intentionally contains 700+ placeholder instances (TODO/FIXME/STUB) as part of its evolutionary development strategy. This is NORMAL and expected.

### Monorepo Structure
Converted from submodule-based to unified monorepo:
- All components directly included (no submodules)
- Single CMake configuration for all components
- Unified build system
- Integrated testing framework

### Cognitive Architecture Concepts
The repository implements advanced cognitive architecture patterns:
- **Hypergraph-based knowledge representation** (AtomSpace)
- **Economic attention allocation** (ECAN)
- **Probabilistic logic reasoning** (PLN)
- **Evolutionary program synthesis** (MOSES)
- **Neural-symbolic integration**
- **Distributed cognitive processing**

## Quick Reference

### Most Frequently Used Commands
```bash
# Quick status check (run first)
./validate-integration.py --no-build

# Full development cycle
./integrate-components.sh all
mkdir -p build && cd build && cmake ..
make -j$(nproc)          # NEVER CANCEL - 60+ min timeout
cd .. && ./validate-integration.py

# Rapid iteration for specific phase
./integrate-components.sh [1-5]
cd build && make -j$(nproc)  # NEVER CANCEL
./validate-integration.py --phase [1-5]

# Clean development restart
rm -rf build components/*/
./integrate-components.sh all
mkdir build && cd build && cmake .. && make -j$(nproc)
```

### Component Dependency Summary
```
Foundation: cogutil
Core: atomspace → (atomspace-rocks, atomspace-restful), cogserver
Logic: unify → ure
Cognitive: attention, spacetime
Advanced: pln, miner
Learning: moses, asmoses
Language: lg-atomese, learn, language-learning
Integration: opencog (final - depends on all)
```

### Phase Summary
- **Phase 1**: Core storage and basic learning (atomspace-rocks, atomspace-restful, moses)
- **Phase 2**: Logic and rule systems (unify, ure, language-learning)
- **Phase 3**: Cognitive attention and reasoning (attention, spacetime)
- **Phase 4**: Advanced probabilistic and learning systems (pln, miner, asmoses)
- **Phase 5**: Language processing and final integration (lg-atomese, learn, opencog)

## Performance Characteristics

### Build Complexity
- **Foundation layer**: ~5-10 minutes (cogutil)
- **Core layer**: ~10-20 minutes (atomspace, cogserver)
- **Full system**: 30-60 minutes (all components)
- **Single-threaded**: 60-90 minutes (troubleshooting builds)

### Memory Requirements
- **Minimum**: 4GB RAM for build
- **Recommended**: 8GB+ RAM for parallel builds
- **Optimal**: 16GB+ RAM for full development

### Disk Space
- **Source**: ~500MB
- **Build artifacts**: ~2-3GB
- **Full development environment**: ~4-5GB

## Advanced Topics

### Cognitive Membrane Integration
The repository includes experimental "cognitive membrane" concepts for self-organizing cognitive systems with persistent state management.

### Neural-Symbolic Integration
Bridges between neural network processing (GGML tensors) and symbolic reasoning (AtomSpace).

### Distributed Cognition
Multi-agent cognitive processing with synchronization and consensus mechanisms.

### Evolutionary Optimization
Self-improving systems using genetic algorithms and evolutionary strategies (MOSES).

### Ontogenesis Concepts
Self-generating, evolving kernels through recursive differential operators (see ONTOGENESIS.md in agents/).

## Support & Documentation

### Primary Documentation
- **README.md**: Repository overview and vision
- **QUICK-START.md**: Quick reference guide
- **DEVELOPMENT-ROADMAP.md**: Complete 20-week integration plan
- **IMPLEMENTATION-ROADMAP.md**: TODO/FIXME resolution strategy

### Component Documentation
Each integrated component has its own documentation in its directory:
- `[component]/README.md`
- `[component]/docs/`

### Testing Documentation
- `tests/integration/README.md`
- Individual test files with embedded documentation

## Remember

This is a **complex cognitive architecture** with 14+ integrated components. Build times are long but necessary. **NEVER cancel long-running operations** - they will complete successfully with proper timeouts (90+ minutes for builds, 20+ minutes for integration/tests).

The repository implements cutting-edge AGI research including probabilistic reasoning, evolutionary optimization, neural-symbolic integration, and distributed cognition. Approach changes with understanding of these sophisticated cognitive systems.
