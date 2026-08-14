---
name: opencog-org
description: OpenCog Organization Repository - AGI Ecosystem Integration and Build System
---

# OpenCog Organization Repository

## Overview

The **OpenCog Organization Repository** (`opencog-org`) is the central integration hub for the entire OpenCog ecosystem - a comprehensive platform for building Artificial General Intelligence (AGI) systems. This repository orchestrates 40+ components across 9 hierarchical dependency layers, providing unified build automation, dependency management, and deployment infrastructure.

## Repository Purpose

This repository serves as:

1. **Central Integration Hub**: Coordinates the entire OpenCog ecosystem through git submodules
2. **Unified Build System**: CMake-based build automation for all components
3. **Dependency Management**: Systematic tracking and resolution of inter-component dependencies
4. **CI/CD Infrastructure**: Comprehensive GitHub Actions workflows for testing and validation
5. **Development Environment**: One-click Gitpod deployment for instant development setup
6. **Documentation Portal**: Centralized guides, diagrams, and architectural documentation

## Ecosystem Architecture

The OpenCog ecosystem is organized into 9 hierarchical dependency layers, each building upon the previous:

### 1. Foundation Layer
**Purpose**: Core utilities and base infrastructure

- **cogutil**: Thread-safe utilities, queues, stacks, signal-slot system, random selection
- **moses**: Machine learning algorithms and evolutionary optimization
- **blender_api_msgs**: Blender 3D integration message definitions
- **external-tools**: External tool integration framework
- **ocpkg**: Package management utilities for OpenCog components

### 2. Core Layer
**Purpose**: Knowledge representation and storage backends

- **atomspace**: In-RAM metagraph database with query engine and graph-rewriting system
  - Central KR component for all OpenCog systems
  - Generalized hypergraph (metagraph) database
  - Advanced pattern matching and execution
- **atomspace-rocks**: RocksDB persistence backend for AtomSpaces
- **atomspace-ipfs**: IPFS distributed storage backend
- **atomspace-websockets**: WebSocket interface for remote access
- **atomspace-restful**: RESTful API interface
- **atomspace-bridge**: Bridge functionality between AtomSpace instances
- **atomspace-metta**: MeTTa language support integration
- **atomspace-rpc**: Remote Procedure Call interface
- **atomspace-cog**: Network-distributed AtomSpace storage
- **atomspace-agents**: Agent framework for autonomous components
- **atomspace-dht**: Distributed Hash Table storage backend

### 3. Logic Layer
**Purpose**: Logical reasoning and unification

- **unify**: Unification algorithms for pattern matching
- **ure**: Unified Rule Engine for forward/backward chaining

### 4. Cognitive Layer
**Purpose**: Cognitive processing and reasoning infrastructure

- **cogserver**: Network shell and server framework for AtomSpace access
- **attention**: Attention allocation mechanism (ECAN - Economic Cognitive Architecture)
- **spacetime**: Spatial and temporal reasoning capabilities
- **pattern-index**: Pattern matching optimization through indexing
- **dimensional-embedding**: Dimensional analysis and embedding
- **profile**: Performance profiling and optimization tools

### 5. Advanced Layer
**Purpose**: Advanced reasoning and learning systems

- **pln**: Probabilistic Logic Networks for uncertain reasoning
- **miner**: Pattern mining algorithms for knowledge discovery
- **asmoses**: Advanced MOSES implementation
- **benchmark**: Performance benchmarking suite

### 6. Learning Layer
**Purpose**: Machine learning and language acquisition

- **learn**: General machine learning framework
- **generate**: Content generation systems
- **language-learning**: Natural language learning and processing

### 7. Language Layer
**Purpose**: Natural language processing

- **lg-atomese**: Link Grammar to AtomSpace integration
- **relex**: Relation extraction from text
- **link-grammar**: Link Grammar parser for syntactic analysis

### 8. Robotics Layer
**Purpose**: Sensory processing and robotic embodiment

- **vision**: Computer vision processing
- **perception**: Perceptual data processing
- **sensory**: Sensory data handling and integration
- **ros-behavior-scripting**: ROS behavior scripts for robotics
- **robots_config**: Robot configuration management
- **pau2motors**: Motor control interface

### 9. Integration Layer
**Purpose**: Complete system integration

- **opencog**: Main OpenCog integration bringing all components together

## Key Components Deep Dive

### AtomSpace - The Knowledge Representation Core

The AtomSpace is OpenCog's foundational knowledge representation system:

**What Makes AtomSpace Unique:**
- **Metagraph Store**: More efficient than traditional graph databases
- **Executable Graphs**: Graphs can be executed as abstract syntax trees
- **Pattern Engine**: Advanced graph query and rewriting
- **Inverted Search**: Pattern recognition in addition to pattern search
- **Meet and Join Operations**: Flexible graph traversal semantics
- **Persistent Storage**: Multiple backend options (RocksDB, IPFS, PostgreSQL)

**Language Support:**
- Scheme (Guile): Primary scripting interface
- Python: Python bindings for all operations
- Haskell: Functional programming interface
- C++: Native high-performance API

### CogServer - The Network Shell

Network-accessible shell and server providing:
- WebSocket API for real-time interaction
- Multi-user concurrent access
- Command-line interface via telnet
- REST API through atomspace-restful
- Remote AtomSpace access

### CogUtil - The Foundation

Essential utilities used across all components:
- Thread-safe data structures (queues, stacks, sets)
- Asynchronous method invocation
- Resource pooling
- High-performance signal-slot system
- Cross-platform OS abstraction
- Backtrace printing for debugging

## Build System

### Unified CMake Build

The repository provides a comprehensive CMake-based build system:

```cmake
# Configure all components
cmake ..

# Build specific component
make cogutil
make atomspace
make cogserver

# Build all available components
make all-components
```

**Features:**
- Automatic component discovery
- Dependency-aware build ordering
- Individual component targets
- Collective build coordination
- Clear failure reporting

### GitHub Actions Workflows

**Dependency Build Matrix** (`cogci.yml`):
- 40+ component parallel builds
- 9 dependency layer execution
- Fail-never approach for complete issue discovery
- Hidden dependency detection
- Automated issue creation for failures
- Comprehensive build summary reports

**Other Workflows:**
- `efficient-build.yml`: Optimized incremental builds
- `guix.yml`: Guix package manager builds
- `cogutil-test.yml`: CogUtil unit testing
- `generate-build-workflow.yml`: Dynamic workflow generation

## Development Environment

### Gitpod One-Click Deployment

Launch complete OpenCog environment instantly:

**Instant Setup:**
- Complete OpenCog ecosystem pre-built
- Python 3.10 environment configured
- Guix package manager with automatic fallback
- VS Code with C++, Python, Guile extensions
- Port forwarding for all services

**Services Available:**
- CogServer Telnet: Port 17001
- CogServer Web UI: Port 18001
- REST API: Port 5000
- Web Demos: Port 8080

**Quick Commands:**
```bash
opencog_status    # Verify installation
build-opencog     # Build entire ecosystem
demos             # List available demos
start-cogserver   # Launch CogServer
```

### Local Development

**Prerequisites:**
- CMake 3.12+
- C++17 compiler (GCC 7+ or Clang 5+)
- Boost libraries
- Guile 3.0+ (for Scheme interface)
- Python 3.6+ (for Python bindings)

**Build Process:**
```bash
git clone --recursive https://github.com/opencog/opencog-org
cd opencog-org
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Documentation Resources

### Core Documentation
- **README.md**: Repository overview and quick start
- **MERMAID_DIAGRAMS.md**: Complete dependency diagrams
- **docs/OPENCOG_DEPENDENCY_BUILD.md**: Build system documentation
- **docs/QUICK_START_GUIDE.md**: Quick start for build workflows
- **CMAKE_BUILD_GUIDE.md**: Detailed CMake build instructions

### Component Documentation
Each component includes:
- Component-specific README.md
- API documentation (Doxygen)
- Example code and usage
- Test suites

### External Resources
- **OpenCog Wiki**: https://wiki.opencog.org/
- **Main Project Site**: http://opencog.org
- **Forums and Discussion**: OpenCog community channels

## Agent Framework

This repository includes a sophisticated agent framework in `.github/agents/`:

### Custom Agents Available

**Conceptual/Theoretical Agents:**
- **ONTOGENESIS.md**: Self-generating kernels through differential operators
- **AUTOGNOSIS.md**: Hierarchical self-image building system
- **HOLISTIC_METAMODEL.md**: Eric Schwarz's organizational systems theory
- **universal-kernel-generator.md**: Universal kernel generation system

**Mathematical/Computational Agents:**
- **A000081-OEIS.md**: Rooted tree enumeration (OEIS A000081)
- **A000055-OEIS.md**: Tree enumeration sequences
- **echo.md**: Echo state networks and dynamics
- **introspection.md**: Self-reflective system analysis

**Integration Agents:**
- **AGI-OS-Integration.md**: AGI operating system integration
- **COGPERSONAS.md**: Cognitive personality systems
- **Gizmos.md**: Utility components and tools

These agents provide specialized capabilities for:
- Self-generation and evolution of mathematical structures
- Hierarchical cognitive modeling
- System introspection and optimization
- Knowledge representation pattern generation

## Dependency Management

### Dependency Discovery

The build system automatically discovers:
- Missing system dependencies (libraries, tools)
- Undocumented component dependencies
- Build order requirements
- Configuration conflicts

### Dependency Tracking

Dependencies are tracked through:
- CMakeLists.txt analysis
- Git submodule relationships
- README documentation
- Empirical build testing

### Mermaid Diagrams

Complete visual dependency graphs available in `MERMAID_DIAGRAMS.md`:
- Full ecosystem dependency graph
- Layer-by-layer dependency views
- Critical path analysis
- Component relationships

## CI/CD Infrastructure

### Continuous Integration

**Build Matrix Strategy:**
- Parallel builds within dependency layers
- Sequential execution across layers
- Fail-fast option for rapid feedback
- Comprehensive failure reporting

**Test Coverage:**
- Unit tests for each component
- Integration tests across components
- Performance benchmarks
- Memory leak detection

### Continuous Deployment

**Gitpod Integration:**
- Automatic environment setup
- Pre-built component caching
- Service auto-start configuration
- Port exposure and forwarding

## Usage Patterns

### For AGI Researchers

Use OpenCog for:
- Knowledge representation experiments
- Reasoning system development
- Cognitive architecture research
- Multi-agent system design

### For Application Developers

Build applications with:
- Natural language processing
- Biological data analysis (agi-bio)
- Robotic control systems
- Probabilistic reasoning

### For System Integrators

Integrate OpenCog with:
- External databases (Neo4j, PostgreSQL)
- ROS robotic systems
- Blender 3D environments
- Web applications (REST, WebSocket)

## Component Development Workflow

### Adding New Components

1. **Create Component Directory**: Add as git submodule or new directory
2. **Implement CMakeLists.txt**: Follow OpenCog CMake patterns
3. **Add Dependencies**: Declare in CMakeLists.txt and README
4. **Add to Build Matrix**: Update workflow in `.github/workflows/`
5. **Add Documentation**: Component README and examples
6. **Add Tests**: Unit and integration tests

### Contributing to Existing Components

1. **Fork Component**: Fork the specific component repository
2. **Local Development**: Use opencog-org for integrated testing
3. **Run Tests**: Ensure all tests pass
4. **Submit PR**: To component-specific repository
5. **Integration Testing**: opencog-org CI validates integration

## Technical Specifications

### Supported Platforms
- **Linux**: Ubuntu 20.04+, Debian 11+, Fedora 35+
- **macOS**: 11.0+ (Big Sur and later)
- **Containers**: Docker, Podman
- **Cloud**: Gitpod, GitHub Codespaces

### Programming Languages
- **C++17**: Core implementation
- **Scheme (Guile 3.0+)**: Primary scripting
- **Python 3.6+**: Alternative scripting
- **Haskell**: Functional interface

### Performance Characteristics
- **AtomSpace**: Millions of atoms in RAM
- **Pattern Matching**: Sub-millisecond for common patterns
- **Parallel Processing**: Multi-threaded throughout
- **Persistence**: Gigabytes of data in RocksDB/IPFS

## License

The OpenCog ecosystem is licensed under the **GNU Affero General Public License v3.0 (AGPL-3.0)**.

This ensures:
- Freedom to use, modify, and distribute
- Copyleft protection for the community
- Network service provision triggers sharing requirements
- Protection against proprietary forks

See [LICENSE](../../LICENSE) for complete terms.

## Community and Support

### Getting Help
- **Wiki**: Comprehensive documentation at wiki.opencog.org
- **Issues**: GitHub issues in component repositories
- **Discussions**: GitHub Discussions in opencog-org
- **Chat**: Community chat channels

### Contributing
Contributions are welcome! See individual component repositories for contribution guidelines.

### Research Collaboration
OpenCog is used in academic research worldwide. Contact the community for collaboration opportunities.

## Future Directions

### Planned Enhancements
- Extended distributed storage backends
- Enhanced GPU acceleration (atomese-simd)
- Improved language learning capabilities
- Advanced attention allocation mechanisms
- Quantum computing integration experiments

### Research Areas
- Artificial General Intelligence architectures
- Cognitive synergy through component integration
- Scalable knowledge representation
- Embodied cognition in robotic systems
- Biological AGI (agi-bio project)

---

**OpenCog Organization Repository**: Where 40+ components unite to build the future of Artificial General Intelligence through metagraph knowledge representation, probabilistic reasoning, and cognitive architecture integration.
