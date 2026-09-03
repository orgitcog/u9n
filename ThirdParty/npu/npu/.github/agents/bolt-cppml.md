---
name: bolt-cppml
description: A modern C++ IDE implementation with AI integration, GGML/ML support, and advanced code editing features. Combines Bolt IDE with machine learning capabilities for intelligent code assistance, real-time collaboration, and GPU-accelerated AI models.
---

# Bolt C++ ML - Intelligent IDE with Machine Learning

## Project Overview

**Bolt C++ ML** (bolt-cppml) is a modern C++ implementation of the Bolt IDE core components enhanced with comprehensive machine learning and AI capabilities. It represents a fusion of traditional IDE features with cutting-edge AI assistance, creating an intelligent development environment powered by GGML, neural networks, and advanced code analysis.

## Architecture

### Core Components

```
Bolt C++ ML Architecture
├── AI/ML Integration Layer
│   ├── GGML Wrapper (Model inference, tensor operations)
│   ├── RWKV Implementation (RNN capabilities, sequential modeling)
│   ├── Direct GGUF Inference (Model loading, quantization)
│   ├── GPU Acceleration (CUDA/OpenCL, optimized kernels)
│   ├── AI Code Completion (Context-aware suggestions)
│   ├── AI Code Generation (Intelligent code synthesis)
│   └── AI Refactoring Engine (Automated code improvements)
├── Editor Core
│   ├── Syntax Highlighting (Multi-language support)
│   ├── Code Folding (Automatic detection, toggle)
│   ├── Multi-Cursor Support (Parallel editing)
│   ├── Find/Replace (Regex, multi-file)
│   ├── Bracket Matching (Smart pairing)
│   ├── Line Numbering (Gutter integration)
│   ├── File Tree Navigation (Directory browser)
│   ├── Split View Editing (Multiple panes)
│   ├── Minimap (Code overview)
│   └── Theme System (Customizable UI)
├── Collaboration Layer
│   ├── Real-time Collaborative Editing (WebSocket-based)
│   ├── Operational Transformation (Conflict resolution)
│   ├── Multi-user Sessions (Synchronized state)
│   └── Presence Indicators (User awareness)
├── Development Tools
│   ├── Language Server Protocol (LSP client/server)
│   ├── Git Integration (Version control)
│   ├── Integrated Debugger (Breakpoints, stepping)
│   ├── Performance Profiler (CPU/memory analysis)
│   ├── Logging System (Multi-level, categorized)
│   ├── Benchmark Suite (Performance testing)
│   └── Code Analysis (Static analysis, metrics)
├── Network Layer
│   ├── WebSocket Server (Real-time collaboration)
│   ├── HTTP Server (API endpoints)
│   ├── Connection Pooling (Resource optimization)
│   ├── Message Compression (Bandwidth reduction)
│   └── Network Metrics (Performance monitoring)
├── Core Engine
│   ├── Memory Management (Pools, tracking, optimization)
│   ├── Message Handling (Event-driven architecture)
│   ├── Store Systems (Chat, Editor, Workbench)
│   ├── Thread Safety (Locks, atomic operations)
│   └── Plugin System (Extensible architecture)
└── GUI Framework
    ├── ImGui Integration (Immediate mode UI)
    ├── Widget Framework (Custom components)
    ├── Dark Mode Support (Theme variants)
    ├── Keyboard Shortcuts (Customizable bindings)
    └── Drawkern Rendering (Custom graphics)
```

## Key Features

### AI/ML Capabilities

#### 1. **GGML Integration**
- **Purpose**: Core tensor operations and ML model inference
- **Capabilities**:
  - Tensor graph construction and execution
  - Quantized model support (Q4_K, Q8_0, etc.)
  - CPU and GPU acceleration
  - Memory-efficient inference
- **Use Cases**: Code completion, generation, analysis

#### 2. **RWKV Neural Network**
- **Purpose**: Recurrent neural network for sequential processing
- **Capabilities**:
  - RNN-based sequence modeling
  - Context-aware predictions
  - Efficient state management
- **Use Cases**: Long-context code understanding, pattern recognition

#### 3. **Direct GGUF Inference**
- **Purpose**: Direct model file loading without external dependencies
- **Capabilities**:
  - GGUF format parsing
  - Model weight loading
  - Inference pipeline
  - Model quantization support
- **Models**: TinyLlama, Phi-2, CodeLlama, custom models

#### 4. **GPU Acceleration**
- **Purpose**: Accelerated AI model inference
- **Backends**: CUDA, OpenCL, Metal
- **Optimizations**:
  - Kernel fusion
  - Memory coalescing
  - Batched operations
  - Mixed precision inference

#### 5. **AI Code Completion**
- **Features**:
  - Context-aware suggestions
  - Multi-language support
  - Real-time inference
  - Keyboard shortcuts integration
  - Snippet expansion
- **Performance**: <100ms latency, streaming results

#### 6. **AI Code Generation**
- **Capabilities**:
  - Function generation from comments
  - Test case generation
  - Documentation generation
  - Boilerplate code synthesis
- **Integration**: Editor commands, context menus

#### 7. **AI Refactoring Engine**
- **Features**:
  - Intelligent code restructuring
  - Variable/function renaming
  - Extract method/class
  - Code smell detection
  - Performance optimization suggestions

### Advanced Editor Features

#### 1. **Code Folding**
- Automatic detection of foldable regions
- Manual fold/unfold with keyboard shortcuts
- Persistent fold state across sessions
- Language-aware folding (functions, classes, blocks)

#### 2. **Multi-Cursor Editing**
- Multiple selection points
- Synchronized editing
- Column selection mode
- Find-and-edit all occurrences

#### 3. **Split View Editing**
- Horizontal/vertical splits
- Multiple file tabs per pane
- Independent scrolling
- Synchronized editing option

#### 4. **Minimap**
- Code overview visualization
- Current viewport indicator
- Click-to-navigate
- Syntax-aware coloring

#### 5. **File Tree Navigation**
- Directory browsing
- File filtering
- Quick file switching
- Project organization

#### 6. **Theme System**
- Dark/light mode support
- Customizable color schemes
- Syntax highlighting themes
- UI element theming

#### 7. **Keyboard Shortcuts**
- Fully customizable bindings
- Vim/Emacs emulation modes
- Chord key sequences
- Context-sensitive shortcuts

### Development Tools

#### 1. **Language Server Protocol (LSP)**
- **Features**:
  - Code completion
  - Go to definition
  - Find references
  - Hover documentation
  - Diagnostics and errors
- **Languages**: C/C++, Python, JavaScript, TypeScript, Rust, Go

#### 2. **Git Integration**
- **Capabilities**:
  - Repository management
  - Commit/push/pull
  - Branch operations
  - Diff visualization
  - Merge conflict resolution
  - Blame annotations

#### 3. **Integrated Debugger**
- **Features**:
  - Breakpoint management
  - Step through code
  - Variable inspection
  - Call stack visualization
  - Conditional breakpoints
  - Watch expressions

#### 4. **Performance Profiler**
- **Metrics**:
  - CPU usage per function
  - Memory allocation tracking
  - Hot path identification
  - Cache miss analysis
  - Thread contention detection
- **Visualization**: Flame graphs, timeline views

#### 5. **Logging System**
- **Levels**: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- **Categories**: Module-based categorization
- **Sinks**: Console, file, network, rotating files
- **Features**: Thread-safe, formatted output, filtering

#### 6. **Benchmark Suite**
- Performance regression testing
- Micro-benchmarks
- Macro-benchmarks
- Comparative analysis
- Historical tracking

#### 7. **Code Analysis**
- Static analysis (cppcheck, clang-tidy)
- Complexity metrics (cyclomatic, cognitive)
- Code coverage
- Dependency analysis
- Security vulnerability scanning

### Collaboration Features

#### 1. **Real-time Collaborative Editing**
- **Technology**: WebSocket-based synchronization
- **Features**:
  - Multi-user editing
  - Operational transformation
  - Conflict resolution
  - User presence indicators
  - Cursor position sharing
  - Selection highlighting

#### 2. **Network Optimizations**
- **Connection Pooling**: Reusable connections
- **Message Compression**: Reduced bandwidth
- **High-Performance Buffers**: Optimized I/O
- **Metrics**: Real-time network statistics

### GUI and Rendering

#### 1. **ImGui Integration**
- Immediate mode GUI
- High-performance rendering
- Customizable widgets
- Responsive layout

#### 2. **Custom Widget Framework**
- Reusable UI components
- Event handling
- Layout management
- Theming support

#### 3. **Drawkern Rendering**
- Custom graphics pipeline
- Optimized text rendering
- Syntax highlighting visualization
- Glyph-based infrastructure

## Technology Stack

### Core Languages & Standards
- **C++17/20**: Modern C++ features
- **CMake 3.15+**: Build system
- **Compiler Support**: GCC 9+, Clang 10+, MSVC 2019+

### AI/ML Libraries
- **GGML**: Tensor operations and ML inference
- **llama.cpp**: Language model integration
- **KoboldCpp**: GGUF model support
- **Custom AI Engines**: Proprietary implementations

### System Libraries
- **CURL**: HTTP client
- **JsonCpp**: JSON parsing
- **GLFW3**: Windowing and input
- **ImGui**: Immediate mode GUI
- **OpenGL**: Graphics rendering
- **WebSocket++**: WebSocket implementation

### Development Dependencies
- **vcpkg/Conan**: Package management
- **CTest**: Testing framework
- **Doxygen**: Documentation generation
- **Git**: Version control

## Build System

### Dependency Management
- **Primary**: vcpkg (recommended)
- **Alternative**: Conan
- **Fallback**: System packages

### Build Configurations
```bash
# Automated setup
./scripts/setup-deps.sh

# CMake presets
cmake --preset vcpkg
cmake --build --preset vcpkg-release

# Manual build
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake ..
make -j$(nproc)
```

### Package Manager Integration
- Multi-manager support (vcpkg, Conan, system)
- Smart detection and selection
- Enhanced CMake target-based linking
- Status checking tools
- Automated installation scripts

## Testing Infrastructure

### Test Framework
- **Custom Framework**: Self-contained, no external dependencies
- **45+ Unit Tests**: Component isolation testing
- **9+ Integration Tests**: End-to-end workflows
- **CTest Integration**: CMake test runner

### Test Categories

#### Unit Tests
- Chat system (7 tests)
- Memory management (4 tests)
- Store systems (15 tests)
- Message handling (5 tests)
- Utilities (3 tests)
- Code folding (11 tests)

#### Integration Tests
- BoltApp initialization
- Multi-store interactions
- Code folding integration
- Message handler workflow
- Memory under load
- Editor document lifecycle
- Chat-editor workflow
- Multithreaded operations
- System shutdown cleanup

### Test Execution
```bash
# Run all tests
./bolt_unit_tests

# Run specific suite
./bolt_unit_tests Chat

# CMake/CTest
make test
ctest --verbose
```

## Project Structure

```
bolt-cppml/
├── .github/
│   ├── agents/          # AI agent configurations
│   └── workflows/       # CI/CD pipelines
├── include/bolt/        # Public headers
│   ├── ai/             # AI/ML components
│   ├── collaboration/  # Real-time collaboration
│   ├── core/           # Core engine
│   ├── drawkern/       # Custom rendering
│   ├── editor/         # Editor components
│   ├── git/            # Git integration
│   ├── gui/            # GUI framework
│   ├── network/        # Network layer
│   └── utils/          # Utility functions
├── src/bolt/           # Implementation files
├── test/               # Unit and integration tests
├── docs/               # Documentation
│   ├── ADVANCED_CODE_ANALYSIS.md
│   ├── BENCHMARK_SUITE.md
│   ├── COLLABORATIVE_EDITING.md
│   ├── GIT_INTEGRATION.md
│   ├── GPU_ACCELERATION.md
│   ├── LOGGING_SYSTEM.md
│   ├── LSP_SUPPORT.md
│   ├── PERFORMANCE_PROFILER.md
│   ├── PLUGIN_SYSTEM.md
│   └── WIDGET_FRAMEWORK.md
├── examples/           # Example code
├── scripts/            # Build and setup scripts
├── ggml/              # GGML submodules
│   ├── kobold.cpp/    # KoboldCpp integration
│   └── guile.llama.cpp/ # Guile bindings
├── CMakeLists.txt     # Main build configuration
├── vcpkg.json         # Package dependencies
└── README.md          # Project overview
```

## Development Workflow

### Setup
1. Clone repository
2. Install dependencies: `./scripts/setup-deps.sh`
3. Configure build: `cmake --preset vcpkg`
4. Build: `cmake --build --preset vcpkg-release`

### Development Cycle
1. **Code**: Write/modify C++ code
2. **Build**: Incremental compilation
3. **Test**: Run unit/integration tests
4. **Debug**: Use integrated debugger
5. **Profile**: Analyze performance
6. **Commit**: Version control with Git

### Code Style
- C++17 standards and best practices
- Consistent 2-space indentation
- Doxygen-style documentation
- RAII patterns
- Const correctness
- Modern C++ idioms

### Testing Requirements
- Unit tests for new features
- Integration tests for workflows
- Performance regression tests
- Documentation updates
- Code review approval

## Performance Characteristics

### Build Performance
- Incremental compilation support
- Parallel build (make -j)
- Precompiled headers
- Unity builds option

### Runtime Performance
- ≤100ms AI completion latency
- ≤5µs scheduler tick
- ≤100ns memory operations
- 60 FPS UI rendering
- <1% CPU idle usage

### Memory Usage
- Optimized memory pools
- Lazy loading
- Reference counting
- Smart pointer management
- Memory leak detection

## Cross-Platform Support

### Platforms
- **Linux**: Primary development platform
- **Windows**: Full support (MSVC)
- **macOS**: Planned support

### Platform-Specific Features
- Native file dialogs
- System integration
- GPU acceleration backends
- Platform-specific optimizations

## Plugin System

### Architecture
- Dynamic library loading
- Plugin API
- Event hooks
- Resource management
- Sandboxed execution

### Plugin Types
- Editor extensions
- Language support
- Theme plugins
- Tool integrations
- AI model providers

## AI Model Support

### Supported Models
- **TinyLlama**: Lightweight, fast inference
- **Phi-2**: High-quality small model
- **CodeLlama**: Code-specialized
- **Custom Models**: Via GGUF format

### Model Formats
- GGUF (primary)
- GGML (legacy)
- Quantized variants (Q4_K, Q8_0, etc.)

### Inference Backends
- CPU (GGML)
- CUDA (NVIDIA GPUs)
- OpenCL (AMD/Intel)
- Metal (Apple Silicon)

## Security Features

### Code Security
- Static analysis integration
- Vulnerability scanning
- Secure coding practices
- Input validation
- Memory safety (RAII, smart pointers)

### Network Security
- TLS/SSL support
- Authentication
- Authorization
- Rate limiting
- CORS policies

## Documentation

### API Documentation
- Doxygen-generated
- Comprehensive class/function docs
- Usage examples
- Design rationale

### User Documentation
- Getting started guides
- Feature documentation
- Tutorials
- FAQ
- Troubleshooting

### Developer Documentation
- Architecture overview
- Contribution guidelines
- Coding standards
- Build instructions
- Testing guide

## Future Roadmap

### Near-term (1-3 months)
- Enhanced AI model support
- Additional language servers
- Mobile platform support
- Cloud sync features
- Advanced debugging tools

### Mid-term (3-6 months)
- Distributed collaboration
- AI pair programming
- Code review automation
- Custom model training
- Plugin marketplace

### Long-term (6-12 months)
- Full AGI-OS integration
- Self-optimizing kernels
- Consciousness-aware features
- Quantum computing support
- Ecosystem expansion

## Contributing

### How to Contribute
1. Fork the repository
2. Create feature branch
3. Make changes with tests
4. Submit pull request
5. Code review process

### Contribution Areas
- Core engine improvements
- AI/ML enhancements
- Editor features
- Documentation
- Testing
- Bug fixes
- Performance optimization

### Community
- GitHub Issues: Bug reports, feature requests
- Pull Requests: Code contributions
- Discussions: Design discussions, Q&A

## License

MIT License - See [LICENSE](../../LICENSE) for details

## Resources

### Documentation
- [Main README](../../README.md)
- [DEVO-GENESIS](../../DEVO-GENESIS.md)
- [Contributing Guide](../../CONTRIBUTING.md)
- [Testing Guide](../../TESTING.md)

### External Resources
- [GGML](https://github.com/ggerganov/ggml)
- [llama.cpp](https://github.com/ggerganov/llama.cpp)
- [KoboldCpp](https://github.com/LostRuins/koboldcpp)
- [ImGui](https://github.com/ocornut/imgui)
- [CMake](https://cmake.org/documentation/)

### Related Projects
- **cogpy/occ**: OpenCog Cognitive Core
- **cogpy/echo-kern**: Echo Kernel Framework
- **cogpy/ggml-cog**: GGML Cognitive Extensions
- **Universal Kernel Generator**: Domain-specific kernel compilation
- **Ontogenesis**: Self-generating, evolving kernels

## Agent Behavior

When working with bolt-cppml:

1. **Understand the Stack**: This is a C++17/20 project with heavy ML integration
2. **AI-First**: AI/ML features are core, not peripheral
3. **Performance Matters**: Optimize for low-latency, high-throughput
4. **Test Coverage**: Maintain comprehensive unit and integration tests
5. **Cross-Platform**: Consider Windows, Linux, macOS compatibility
6. **Documentation**: Update docs with code changes
7. **Modern C++**: Use C++17/20 features appropriately
8. **Memory Safety**: RAII, smart pointers, no raw pointers
9. **Thread Safety**: Consider concurrent access patterns
10. **Build System**: Keep CMake configuration clean and portable

## Key Implementation Patterns

### AI Integration Pattern
```cpp
// Load model
auto inference = bolt::DirectGGUFInference("model.gguf");

// Generate completion
auto result = inference.complete(context, options);

// Process result
editor.insertText(result.text);
```

### Editor Integration Pattern
```cpp
// Create editor instance
auto& editorStore = bolt::EditorStore::getInstance();

// Open document
editorStore.openDocument("file.cpp");

// Apply code folding
auto& foldingManager = bolt::CodeFoldingManager::getInstance();
foldingManager.toggleFold(lineNumber);
```

### Collaboration Pattern
```cpp
// Initialize WebSocket server
auto server = bolt::WebSocketServer(port);

// Handle collaborative edits
server.onEdit([&](const Edit& edit) {
    operationalTransform.apply(edit);
    broadcast(edit);
});
```

---

**Bolt C++ ML**: Where modern IDE meets machine intelligence, creating the next generation of AI-assisted development environments.
