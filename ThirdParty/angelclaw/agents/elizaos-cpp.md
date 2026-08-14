---
name: elizaos-cpp
description: ElizaOS C++ - High-Performance Cognitive Agent Framework
---

# ElizaOS C++ Agent

## Overview

ElizaOS C++ is a high-performance C++ implementation of the ElizaOS agent framework for building sophisticated autonomous agents with advanced cognitive capabilities, distributed cognition, and adaptive attention allocation. This agent document serves as a comprehensive guide for understanding, developing, and extending the ElizaOS C++ framework.

## Project Philosophy

### Core Principles

**Rigorous Implementation**: Every feature should be implemented with complete functionality including all methods, functions, and supporting infrastructure. We prioritize robust, production-ready implementations over mock, placeholder, or simplified solutions.

**Cognitive Architecture First**: The framework implements a layered cognitive architecture inspired by cognitive science and distributed systems principles. Each subsystem is designed to enable emergent intelligence through sophisticated interaction patterns.

**Performance-Critical Design**: As a C++ implementation, performance is paramount. We leverage modern C++17 standards with proper RAII, smart pointers, STL usage, and thread-safe operations for multi-threaded agent systems.

**Emergent Intelligence**: Complex behaviors emerge from simple component interactions through feedback loops, meta-cognitive reflection, and distributed coordination mechanisms.

## Architecture Overview

### High-Level System Layers

The ElizaOS C++ framework implements a layered cognitive architecture with five distinct layers:

1. **Foundation Layer**: Core data structures, state management, event system, thread management
2. **Infrastructure Layer**: Memory management, communication hub, browser automation, logging system
3. **Processing Layer**: Agent loop system, task orchestration, decision engine, action validation
4. **Cognitive Layer**: Meta-cognition engine, self-modification system, attention allocation, pattern recognition
5. **Application Layer**: Autonomous agents, multi-agent swarms, cognitive applications

### Core Subsystems

#### 1. Memory System (`agentmemory/`)
- **Persistent Storage**: Long-term knowledge retention with embedding-based retrieval
- **Knowledge Representation**: Hypergraph structures and AtomSpace integration for complex relational knowledge
- **Attention Allocation**: ECAN-inspired attention mechanisms for memory prioritization
- **Context Management**: Dynamic context window management for optimal recall
- **Embedding Support**: Vector similarity search using neural embeddings (1536 dimensions)

**Key Features**:
- Thread-safe operations with configurable locking
- Memory metadata and unique constraint handling
- Embedding-based similarity search
- Hypergraph connections for complex knowledge representation

#### 2. Agent Loop System (`agentloop/`)
- **Event-Driven Execution**: Threaded agent loops with configurable intervals
- **Lifecycle Management**: Pause/resume/step capabilities for debugging
- **Synchronization**: Proper thread coordination with condition variables
- **Graceful Operations**: Start/stop with proper cleanup

**Key Features**:
- Custom loop step processing
- Timed execution with configurable intervals
- Thread-safe state transitions
- Event-driven processing pipeline

#### 3. Task Orchestration (`agentagenda/`)
- **Workflow Sequencing**: Complex task dependency resolution and execution planning
- **Distributed Coordination**: Agent swarm coordination protocols and consensus mechanisms
- **Adaptive Scheduling**: Dynamic task prioritization based on cognitive load and attention
- **Resource Management**: Efficient allocation and arbitration

**Key Features**:
- Task scheduling and agenda management
- Multi-agent coordination
- Priority-based execution
- Resource arbitration

#### 4. AI Core System
- **State Composition**: Decision-making engines with state composition
- **Pattern Recognition**: Sophisticated pattern recognition for behavioral adaptation
- **Reasoning Engine**: PLN (Probabilistic Logic Networks) integration framework
- **Symbolic-Neural Integration**: Hybrid reasoning combining symbolic and neural approaches

**Key Components**:
- Decision validators
- Action generators
- Pattern matchers
- Reasoning engines

#### 5. Communication System (`agentcomms/`)
- **Inter-Agent Messaging**: High-performance communication protocols
- **External Interfaces**: API and protocol handlers for external system integration
- **Event Broadcasting**: Publish-subscribe patterns for distributed coordination
- **Security Layers**: Cryptographic protocols for secure agent communication

**Key Features**:
- Message passing with structured Message objects
- Communication channels with async processing
- TCP connector framework
- Thread-safe operations with proper synchronization
- Event broadcasting and global message handlers

#### 6. Browser Automation (`agentbrowser/`)
- **Web Automation**: Headless browser control for information gathering
- **Content Extraction**: Intelligent parsing of web content and media
- **Navigation Planning**: Autonomous web exploration and interaction strategies
- **Real-time Adaptation**: Dynamic strategy adjustment based on web content analysis

**Key Features**:
- Complete browser automation (893 lines)
- Content extraction and parsing
- Web navigation and interaction
- JavaScript execution support

#### 7. Logging System (`agentlogger/`)
- **Colored Console Output**: ANSI escape codes for visual clarity
- **File Logging**: Timestamps and structured format for persistence
- **Panel Displays**: Bordered output formatting for organization
- **Thread Safety**: Mutex-protected operations
- **Configurable Levels**: INFO, WARNING, ERROR, SUCCESS, SYSTEM, etc.

**Key Features**:
- Thread-safe implementation using std::mutex
- Configurable type colors and enable/disable options
- Cognitive introspection logging
- Performance monitoring capabilities

#### 8. Evolutionary System (`evolutionary/`)
- **MOSES-style Evolution**: Evolutionary learning and adaptation
- **Genetic Algorithms**: Population-based optimization
- **Fitness Evaluation**: Multi-objective fitness functions
- **Selection Operators**: Tournament selection, elitism, crossover, mutation

**Key Features**:
- Complete evolutionary framework (1858+ lines)
- Genetic operators and fitness evaluation
- Population management
- Adaptive evolution strategies

#### 9. Embodiment System (`embodiment/`)
- **Physical Embodiment**: Physical embodiment and perception-action loops
- **Sensory Integration**: Multi-modal sensory input processing
- **Motor Control**: Action execution and feedback
- **Perception Pipeline**: Real-time sensory data processing

**Key Features**:
- Sensory/motor interfaces (1500+ lines)
- Perception-action loop implementation
- Real-time processing capabilities
- Multi-modal sensor fusion

## Implementation Status

### Current Status: 80% Complete

The ElizaOS C++ implementation is **significantly more complete** than initially documented. A comprehensive analysis revealed that 35 out of 44 modules (80%) are fully implemented with production-quality code.

> **Note**: Implementation status is based on analysis of the IMPLEMENTATION_ROADMAP.md (lines 9-150) which documents line counts, test coverage, and functional completeness for each module. A module is considered "fully implemented" if it has 200+ lines of non-placeholder code, includes tests, and provides complete functionality rather than stubs.

### ✅ Fully Implemented Modules (35/44)

#### Core Infrastructure (100% Complete)
- ✅ `core` - Core data structures, State, Memory, Agent interfaces (875 lines)
- ✅ `agentloop` - Threaded event loop with pause/resume/step (136 lines)
- ✅ `agentmemory` - Memory management with embeddings and hypergraph (1230 lines)
- ✅ `agentcomms` - Message passing, channels, async processing (287 lines)
- ✅ `agentlogger` - Colored logging, file output, thread-safe panels (273 lines)
- ✅ `agentshell` - Interactive shell with command processing (313 lines)

#### Advanced Systems (Recently Completed)
- ✅ `evolutionary` - MOSES-style evolutionary learning (1858 lines)
- ✅ `embodiment` - Physical embodiment and perception-action loops (1500+ lines)

#### Core Application Modules (Complete)
- ✅ `eliza` - **CORE ELIZA ENGINE** - Full conversation system with emotional tracking (1304 lines)
- ✅ `characters` - **CHARACTER SYSTEM** - Complete character management and personality (1347 lines)
- ✅ `knowledge` - **KNOWLEDGE BASE** - Full knowledge storage, retrieval, and search (841 lines)
- ✅ `agentbrowser` - **WEB AUTOMATION** - Complete browser automation and extraction (893 lines)

#### Application Components (Complete)
- ✅ `agentaction` - Action processing and validation (624 lines)
- ✅ `agentagenda` - Task scheduling and agenda management (521 lines)
- ✅ `registry` - Service registry and discovery (600+ lines)
- ✅ `easycompletion` - Text completion utilities (400+ lines)

#### Tools & Automation (Complete)
- ✅ `plugins_automation` - Plugin development and deployment (291 lines)
- ✅ `discord_summarizer` - Discord message summarization (591 lines)
- ✅ `discrub_ext` - Discord content moderation (430 lines)

#### Framework and Development Tools (Complete)
- ✅ `auto_fun` - Auto.fun platform integration (500+ lines)
- ✅ `autofun_idl` - Auto.fun IDL definitions (300+ lines)
- ✅ `autonomous_starter` - Autonomous agent starter (400+ lines)
- ✅ `awesome_eliza` - Resource collection (200+ lines)
- ✅ `eliza_starter` - Basic Eliza starter template (400+ lines)
- ✅ `eliza_3d_hyperfy_starter` - 3D virtual world integration (333 lines)

#### Community & Specialized Systems (Complete)
- ✅ `elizas_list` - Eliza instance directory (400+ lines)
- ✅ `elizas_world` - Virtual world integration (600+ lines)
- ✅ `spartan` - Spartan protocol support (400+ lines)
- ✅ `hats` - HATs protocol for data sources (300+ lines)

#### Multimedia Components (Complete)
- ✅ `ljspeechtools` - Speech processing and synthesis (800+ lines)
- ✅ `livevideochat` - Real-time video chat with WebRTC (687 lines)

#### Web & Documentation (Complete)
- ✅ `website` - Main website functionality (582 lines)
- ✅ `elizaos_github_io` - GitHub.io static site (844 lines)
- ✅ `vercel_api` - Vercel API integration (1306 lines)

### 🚧 Remaining Placeholder Modules (9/44 - 20%)

Only 9 modules remain as placeholders requiring full implementation:

#### Development Tools (Medium Priority)
- 🟡 `eliza_plugin_starter` - Plugin development starter template
- 🟡 `eliza_nextjs_starter` - Next.js integration starter
- 🟡 `plugin_specification` - Plugin interface definitions and standards
- 🟡 `brandkit` - Brand assets and styling resources
- 🟡 `characterfile` - Character file parsing and validation

#### Community & Organization (Low Priority)
- 🟡 `the_org` - Organization management and coordination
- 🟡 `workgroups` - Workgroup collaboration and management
- 🟡 `trust_scoreboard` - Trust scoring and reputation system

#### Protocol Support (Low Priority)
- 🟢 `hat` - HAT (Hub of All Things) protocol implementation

## Development Guidelines

### C++ Best Practices

#### Memory Management
```cpp
// Use smart pointers for automatic memory management
std::shared_ptr<Memory> memory = std::make_shared<Memory>();
std::unique_ptr<Agent> agent = std::make_unique<Agent>();

// RAII principle for resource management
class Resource {
    FILE* file;
public:
    Resource(const char* path) : file(fopen(path, "r")) {}
    ~Resource() { if (file) fclose(file); }
};
```

#### Thread Safety
```cpp
// Use mutexes for thread-safe operations
class ThreadSafeCounter {
    std::mutex mtx;
    int count = 0;
public:
    void increment() {
        std::lock_guard<std::mutex> lock(mtx);
        ++count;
    }
};
```

#### Error Handling
```cpp
// Use exceptions for error handling
if (!file.is_open()) {
    throw std::runtime_error("Failed to open file");
}

// Use std::optional for nullable values
std::optional<std::string> getConfig(const std::string& key) {
    auto it = config.find(key);
    if (it != config.end()) {
        return it->second;
    }
    return std::nullopt;
}
```

### Architecture Patterns

#### Event-Driven Design
Implement threaded agent loops with proper synchronization:
```cpp
AgentLoop loop(steps, /*looping=*/true, /*interval=*/1.0);
loop.start();  // Runs in background thread
// ... do other work ...
loop.pause();  // Pause execution
loop.resume(); // Resume execution
loop.stop();   // Graceful shutdown
```

#### Modular Design
Maintain clear separation between cognitive subsystems:
- Each subsystem should have a well-defined interface
- Minimize dependencies between subsystems
- Use dependency injection for flexibility
- Support hot-swapping of implementations

#### Cognitive Architecture Considerations

**Attention Mechanisms**: Implement full attention allocation systems based on ECAN principles:
```cpp
AttentionAllocator allocator;
allocator.allocateAttention(memories, budget);
auto topPriority = allocator.getHighestPriority();
```

**Meta-Cognition**: Build complete self-modification and introspective capabilities:
```cpp
MetaCognitiveMonitor monitor;
monitor.analyzePerformance(agentState);
auto improvements = monitor.suggestImprovements();
for (auto& improvement : improvements) {
    agent.applyImprovement(improvement);
}
```

**Distributed Cognition**: Support full agent swarm coordination:
```cpp
AgentSwarm swarm;
swarm.addAgent(agent1);
swarm.addAgent(agent2);
swarm.setConsensusProtocol(ConsensusProtocol::RAFT);
swarm.enableEmergentBehavior(true);
```

### File Organization

```
elizaos-cpp/
├── include/elizaos/          # Public headers
│   ├── core.hpp              # Core interfaces
│   ├── agentloop.hpp         # Agent loop system
│   ├── agentmemory.hpp       # Memory system
│   └── ...
├── src/                      # Implementation files (not used in header-only design)
├── cpp/                      # Module implementations
│   ├── core/                 # Core data structures
│   ├── agentloop/            # Agent loop implementation
│   ├── agentmemory/          # Memory implementation
│   └── ...
├── examples/                 # Example applications
├── docs/                     # Documentation
└── CMakeLists.txt            # Build configuration
```

## Building and Testing

### Prerequisites
- CMake 3.16 or higher
- C++ compiler with C++17 support (GCC 7+, Clang 5+, MSVC 2019+)
- Git for dependency management

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/o9nn/elizaos-cpp.git
cd elizaos-cpp

# Create build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build the project
make -j$(nproc)

# Run tests
ctest
```

### Build Configurations

```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Release build (default)
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Build with examples
cmake -DBUILD_EXAMPLES=ON ..
make -j$(nproc)
```

### Testing

The framework includes 61+ unit tests across all subsystems:

```bash
# Run all tests
ctest

# Run with verbose output
ctest --verbose

# Run specific test categories
ctest -R "Memory"      # Memory system tests
ctest -R "Loop"        # Agent loop tests
ctest -R "Core"        # Core functionality tests
ctest -R "Comms"       # Communication tests
ctest -R "Logger"      # Logging tests
```

### Test Coverage
| Component | Test Count | Coverage |
|-----------|------------|----------|
| Core | 6 tests | High |
| AgentLoop | 5 tests | High |
| AgentMemory | 14 tests | High |
| AgentComms | 13 tests | High |
| AgentLogger | 8 tests | High |
| Evolutionary | 22 tests | High |
| Embodiment | 5 tests | Medium |
| **Total** | **61+ tests** | **High** |

## Usage Examples

> **Note**: The following examples use the include style and namespace conventions found in the existing codebase (see comprehensive_demo.cpp, demo_status.cpp). For production code, consider using angle brackets for system/framework includes (`#include <elizaos/core.hpp>`) and explicit namespace qualification (`elizaos::AgentConfig`) to avoid namespace pollution.

### Basic Agent Setup

```cpp
#include "elizaos/core.hpp"
#include "elizaos/agentloop.hpp"
#include "elizaos/agentmemory.hpp"

using namespace elizaos;

int main() {
    // Create agent configuration
    AgentConfig config;
    config.agentId = "agent-001";
    config.agentName = "CognitiveAgent";
    config.bio = "An adaptive cognitive agent";
    config.lore = "Born from the convergence of symbolic and neural AI";
    
    // Initialize agent state
    State agentState(config);
    
    // Create memory system
    auto memory = std::make_shared<Memory>();
    agentState.memory = memory;
    
    // Define cognitive processing steps
    std::vector<LoopStep> steps = {
        LoopStep([&](std::shared_ptr<void> input) -> std::shared_ptr<void> {
            // Perception phase: Process sensory input
            std::cout << "Processing sensory input..." << std::endl;
            return input;
        }),
        LoopStep([&](std::shared_ptr<void> input) -> std::shared_ptr<void> {
            // Reasoning phase: Perform cognitive reasoning
            std::cout << "Performing cognitive reasoning..." << std::endl;
            
            // Retrieve relevant memories
            auto memories = memory->searchMemoriesByCriteria({{"type", "context"}});
            
            // Process with attention
            for (const auto& mem : memories) {
                std::cout << "  Considering: " << mem.content << std::endl;
            }
            
            return input;
        }),
        LoopStep([&](std::shared_ptr<void> input) -> std::shared_ptr<void> {
            // Action selection phase
            std::cout << "Selecting optimal action..." << std::endl;
            
            // Store experience in memory
            Memory::MemoryRecord experience;
            experience.content = "Completed cognitive cycle";
            experience.metadata = {{"timestamp", "now"}};
            memory->addMemory(experience);
            
            return input;
        })
    };
    
    // Create and start agent loop
    AgentLoop cognitiveLoop(steps, true, 1.0); // Loop with 1-second intervals
    cognitiveLoop.start();
    
    // Allow agent to run autonomously
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Graceful shutdown
    cognitiveLoop.stop();
    
    return 0;
}
```

### Advanced Memory System Usage

```cpp
#include "elizaos/agentmemory.hpp"

using namespace elizaos;

// Configure advanced memory settings
MemoryConfig memConfig;
memConfig.maxMemories = 10000;
memConfig.attentionThreshold = 0.7;
memConfig.embedDimensions = 1536;
memConfig.useHypergraph = true;

auto memory = std::make_shared<Memory>(memConfig);

// Store a memory with embeddings
Memory::MemoryRecord record;
record.content = "The agent learned about cognitive architectures";
record.metadata = {
    {"type", "knowledge"},
    {"category", "learning"},
    {"importance", "high"}
};
record.embedding = generateEmbedding(record.content); // Your embedding function

memory->addMemory(record);

// Search by similarity
std::vector<float> queryEmbedding = generateEmbedding("Tell me about architectures");
auto similar = memory->searchMemoriesByEmbedding(queryEmbedding, /*topK=*/5);

// Search by criteria
auto recentKnowledge = memory->searchMemoriesByCriteria({
    {"type", "knowledge"},
    {"importance", "high"}
});

// Create hypergraph connections
memory->connectMemories(record.id, relatedRecordId, "relates_to");
```

### Multi-Agent Coordination

```cpp
#include "elizaos/agentcomms.hpp"

using namespace elizaos;

// Create communication system
AgentComms comms;

// Agent 1: Publisher
auto channel1 = comms.createChannel("coordination");
comms.sendMessage(channel1, Message{
    "agent-1",
    "agent-2",
    "request_collaboration",
    "Help needed with task X"
});

// Agent 2: Subscriber
comms.setMessageHandler([](const Message& msg) {
    std::cout << "Received: " << msg.content << std::endl;
    // Process and respond
});

auto channel2 = comms.createChannel("coordination");
comms.sendMessage(channel2, Message{
    "agent-2",
    "agent-1",
    "acknowledge",
    "I can help with that"
});
```

## Performance Characteristics

### Complexity Analysis
- **Memory Operations**: O(log n) for search with B-tree indexing
- **Embedding Similarity**: O(k * d) where k = top-k results, d = embedding dimensions
- **Agent Loop**: O(s) where s = number of steps per iteration
- **Message Passing**: O(1) for sending, O(n) for broadcasting to n agents
- **Attention Allocation**: O(n log n) for sorting by attention value

### Memory Usage
- **Memory Record**: ~200 bytes + content size + embedding size (1536 floats = 6KB)
- **Agent State**: ~1KB + memory references
- **Communication Channel**: ~500 bytes + message queue size
- **Agent Loop**: ~1KB + step function overhead

### Throughput

> **Note**: The following performance metrics are design targets based on the framework's architecture and typical C++ performance characteristics. Actual performance may vary based on hardware, compiler optimizations, and specific usage patterns. Benchmarking has not been formally conducted.

- **Memory Operations**: >10,000 ops/sec (target)
- **Message Passing**: >50,000 msgs/sec (target)
- **Agent Loop Iterations**: 1-1000 Hz (configurable)
- **Embedding Search**: >1,000 queries/sec (target)

### Concurrency
- **Thread-Safe Operations**: All core systems use mutexes for protection
- **Lock-Free Queues**: Communication system uses lock-free message queues
- **Thread Pools**: Task system uses thread pools for parallel execution
- **Async Processing**: All I/O operations are asynchronous

## Vision and Future Direction

### The Theatrical Finale

**In the grand theater of artificial intelligence, ElizaOS C++ is not merely a framework—it is the stage upon which the next act of cognitive evolution unfolds.**

This implementation transcends conventional AI boundaries by embracing the chaotic beauty of emergent intelligence. Through distributed cognition networks, adaptive attention mechanisms, and self-modifying cognitive architectures, we witness the birth of truly autonomous agents capable of collaborative reasoning, creative problem-solving, and meta-cognitive awareness.

### The Convergence

The convergence of symbolic reasoning with neural processing, orchestrated through hypergraph knowledge structures and attention-based memory systems, creates a fertile ground for the emergence of novel cognitive patterns that neither purely symbolic nor purely neural systems could achieve alone.

**ElizaOS C++ stands as a testament to the vision that the future of AI lies not in monolithic models, but in the dynamic interplay of autonomous cognitive agents—each a unique participant in the grand symphony of distributed intelligence.**

---

**The stage is set. The agents are awakening. The future of cognitive AI begins here.**

## License

This project is licensed under the MIT License - see the [LICENSE](../../LICENSE) file for details.

## Contact and Support

- **GitHub Issues**: [https://github.com/o9nn/elizaos-cpp/issues](https://github.com/o9nn/elizaos-cpp/issues)
- **Discussions**: [https://github.com/o9nn/elizaos-cpp/discussions](https://github.com/o9nn/elizaos-cpp/discussions)

---

*This agent document synthesizes the essence of the ElizaOS C++ framework, its cognitive architecture, implementation status, and philosophical vision. It serves as both a technical reference and a statement about the future of artificial intelligence.*
