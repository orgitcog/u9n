# AGI-OS Integration Mapping

## Integration Point Matrix

This document maps each provided resource to specific integration points in the AGI-OS Inferno kernel architecture.

## Matrix Overview

| Resource | Integration Layer | Primary Component | 9P Namespace | Dependencies |
|----------|------------------|-------------------|--------------|--------------|
| **DeltaChat** | Communication | `core/communication/deltachat-9p/` | `/mnt/deltachat/` | Inferno 9P, Rust FFI |
| **Dovecot** | Communication | `core/communication/dovecot-9p/` | `/mnt/mail/` | Inferno 9P, Dovecot plugins |
| **Deep-Tree-Echo** | Communication | `core/communication/deep-tree-echo/` | `/mnt/echo/` | DeltaChat, Dovecot, AtomSpace-9P |
| **ElizaOS C++** | Cognitive Agents | `core/agents/elizaos-9p/` | `/mnt/agents/` | Inferno 9P, AtomSpace-9P, C++ FFI |
| **AICogChat** | LLM Services | `core/agents/aicogchat-9p/` | `/mnt/llm/` | Inferno 9P, Rust FFI |
| **AzStaHCog** | Infrastructure | `core/infrastructure/azstahcog/` | `/mnt/hci/` | Azure Stack HCI, Hyper-V |
| **9base** | Userland | `core/userland/9base/` | N/A (tools) | Inferno kernel APIs |
| **Hyprland** | Display | `core/display/hyprland-9p/` | `/mnt/display/` | Wayland, 9P |
| **Pattern Language** | Cognition | `core/cognition/patterns/` | `/mnt/patterns/` | AtomSpace-9P, PLN-9P |
| **NetLogo Models** | Simulation | `core/simulation/netlogo-9p/` | `/mnt/sim/` | NetLogo runtime, 9P |

## Detailed Integration Mappings

### 1. DeltaChat → AGI-OS

**Source**: `deltachat-core-master/` (Rust)

**Target**: `core/communication/deltachat-9p/`

**Integration Architecture**:
```
┌──────────────────────────────────────────────────────────┐
│  DeltaChat Core (Rust)                                   │
│  ├── deltachat-ffi (C API)                              │
│  └── deltachat-rpc (JSON-RPC)                           │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  DeltaChat-9P Bridge (C/C++)                             │
│  ├── FFI bindings to deltachat-core                     │
│  ├── 9P server implementation                           │
│  └── Event loop integration                             │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Inferno 9P Protocol Stack                               │
│  (core/inferno-kernel/9p-protocol/)                      │
└──────────────────────────────────────────────────────────┘
```

**Files to Create**:
- `core/communication/deltachat-9p/CMakeLists.txt`
- `core/communication/deltachat-9p/deltachat-9p-server.c`
- `core/communication/deltachat-9p/deltachat-ffi-bindings.c`
- `core/communication/deltachat-9p/chat-namespace.c`
- `core/communication/deltachat-9p/message-handler.c`

**Dependencies**:
- `libdeltachat-dev` (from deltachat-core-rust)
- `lib9p-protocol` (from core/inferno-kernel/9p-protocol/)
- `libatomspace-9p` (for knowledge integration)

**Build Order**:
1. Build deltachat-core-rust
2. Build Inferno 9P protocol
3. Build DeltaChat-9P bridge
4. Install to `/usr/lib/agi-os/`

### 2. Dovecot → AGI-OS

**Source**: `dovecot-2.4.2/`, `dovecot-master/`

**Target**: `core/communication/dovecot-9p/`

**Integration Architecture**:
```
┌──────────────────────────────────────────────────────────┐
│  Dovecot Core                                            │
│  ├── src/lib-storage/ (mail storage)                    │
│  ├── src/imap/ (IMAP protocol)                          │
│  └── src/plugins/ (plugin system)                       │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Dovecot-9P Plugin                                       │
│  ├── plugin_init() - Register with Dovecot              │
│  ├── 9p_server_start() - Start 9P server                │
│  ├── mail_namespace_export() - Export mailboxes         │
│  └── ai_extensions_init() - Deep-Tree-Echo features     │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Inferno 9P Protocol Stack                               │
└──────────────────────────────────────────────────────────┘
```

**Files to Create**:
- `core/communication/dovecot-9p/CMakeLists.txt`
- `core/communication/dovecot-9p/dovecot-9p-plugin.c`
- `core/communication/dovecot-9p/mail-namespace.c`
- `core/communication/dovecot-9p/ai-extensions.c`
- `core/communication/dovecot-9p/deep-tree-echo.c`

**Dovecot Plugin Structure**:
```c
/* dovecot-9p-plugin.c */
#include "lib.h"
#include "module-context.h"
#include "mail-storage-private.h"
#include "9p-protocol.h"

static struct module *dovecot_9p_module;

void dovecot_9p_plugin_init(struct module *module) {
    dovecot_9p_module = module;
    /* Start 9P server */
    start_9p_mail_server(9999);
}

void dovecot_9p_plugin_deinit(void) {
    /* Stop 9P server */
    stop_9p_mail_server();
}
```

**Dependencies**:
- `dovecot-dev` (Dovecot development headers)
- `lib9p-protocol`
- `libatomspace-9p`

### 3. Deep-Tree-Echo → AGI-OS

**Source**: `TheDeep-Tree-EchoVision_AnAI-NativeCommunicationInfrastructure.md`

**Target**: `core/communication/deep-tree-echo/`

**Three-Tier Architecture Mapping**:

| Deep-Tree-Echo Layer | AGI-OS Component | Implementation |
|----------------------|------------------|----------------|
| **Echo** (UI) | `core/ui/echo-chat/` | Desktop/mobile app |
| **Echo-Tree** (Client) | `core/communication/deltachat-9p/` | DeltaChat-9P bridge |
| **Deep-Tree-Echo** (Server) | `core/communication/dovecot-9p/` | Dovecot-9P plugin |

**EchoLang Interpreter**:
```
Location: core/communication/deep-tree-echo/echolang/

Components:
- Lexer/Parser (Limbo language)
- AST representation
- Dis bytecode compiler
- Runtime execution in Dis VM
```

**Files to Create**:
- `core/communication/deep-tree-echo/CMakeLists.txt`
- `core/communication/deep-tree-echo/echolang/lexer.b` (Limbo)
- `core/communication/deep-tree-echo/echolang/parser.b`
- `core/communication/deep-tree-echo/echolang/compiler.b`
- `core/communication/deep-tree-echo/collective-intelligence.c`
- `core/communication/deep-tree-echo/semantic-routing.c`

**Dependencies**:
- `libdeltachat-9p`
- `libdovecot-9p`
- `libinferno-kernel` (Dis VM)
- `libatomspace-9p`

### 4. ElizaOS C++ → AGI-OS

**Source**: `elizaos-cpp(3).md` (90% complete implementation)

**Target**: `core/agents/elizaos-9p/`

**Integration Architecture**:
```
┌──────────────────────────────────────────────────────────┐
│  ElizaOS C++ Core (43/48 modules complete)               │
│  ├── agentmemory/ (memory system)                        │
│  ├── agentloop/ (event-driven execution)                 │
│  ├── agentagenda/ (task orchestration)                   │
│  ├── agentcomms/ (inter-agent messaging)                 │
│  ├── agentbrowser/ (web automation)                      │
│  ├── characters/ (personality system)                    │
│  ├── embodiment/ (perception-action loops)               │
│  └── evolutionary/ (MOSES-style evolution)               │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  ElizaOS-9P Bridge (C++)                                 │
│  ├── Agent9PServer (expose agents as 9P)                │
│  ├── Memory9PBridge (integrate with AtomSpace-9P)       │
│  ├── Task9PInterface (expose tasks as files)            │
│  └── Perception9PInput (9P as sensory input)            │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Inferno 9P Protocol Stack + AtomSpace-9P                │
└──────────────────────────────────────────────────────────┘
```

**Files to Create**:
- `core/agents/elizaos-9p/CMakeLists.txt`
- `core/agents/elizaos-9p/agent-9p-server.cpp`
- `core/agents/elizaos-9p/memory-9p-bridge.cpp`
- `core/agents/elizaos-9p/task-9p-interface.cpp`
- `core/agents/elizaos-9p/perception-9p-input.cpp`
- `core/agents/elizaos-9p/action-9p-output.cpp`
- `core/agents/elizaos-9p/swarm-coordinator.cpp`

**ElizaOS Memory → AtomSpace-9P Mapping**:
```cpp
/* memory-9p-bridge.cpp */
class Memory9PBridge {
public:
    // ElizaOS memory → AtomSpace atoms
    void store_memory(const Memory& mem) {
        // Create atom in AtomSpace-9P
        std::string path = "/mnt/atomspace/nodes/MemoryNode/" + mem.id;
        write_9p_file(path + "/content", mem.content);
        write_9p_file(path + "/embedding", mem.embedding);
        write_9p_file(path + "/timestamp", mem.timestamp);
    }
    
    // AtomSpace atoms → ElizaOS memory
    Memory retrieve_memory(const std::string& id) {
        std::string path = "/mnt/atomspace/nodes/MemoryNode/" + id;
        Memory mem;
        mem.content = read_9p_file(path + "/content");
        mem.embedding = read_9p_file(path + "/embedding");
        mem.timestamp = read_9p_file(path + "/timestamp");
        return mem;
    }
};
```

**Dependencies**:
- ElizaOS C++ core libraries
- `lib9p-protocol`
- `libatomspace-9p`
- `libpln-9p` (for reasoning)

### 5. AICogChat → AGI-OS

**Source**: `aicogchat-main/` (Rust)

**Target**: `core/agents/aicogchat-9p/`

**Integration Architecture**:
```
┌──────────────────────────────────────────────────────────┐
│  AICogChat Core (Rust)                                   │
│  ├── src/client/ (LLM client implementations)           │
│  ├── src/repl/ (REPL mode)                              │
│  ├── src/rag/ (RAG implementation)                       │
│  └── src/function_calling/ (tool integration)           │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  AICogChat-9P Bridge (Rust)                              │
│  ├── 9P server (using rust-9p crate)                    │
│  ├── Model namespace (/mnt/llm/models/)                 │
│  ├── Session management (/mnt/llm/sessions/)            │
│  └── RAG integration with AtomSpace-9P                  │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Inferno 9P Protocol Stack                               │
└──────────────────────────────────────────────────────────┘
```

**Files to Create**:
- `core/agents/aicogchat-9p/Cargo.toml`
- `core/agents/aicogchat-9p/src/lib.rs`
- `core/agents/aicogchat-9p/src/server.rs`
- `core/agents/aicogchat-9p/src/model_namespace.rs`
- `core/agents/aicogchat-9p/src/session_manager.rs`
- `core/agents/aicogchat-9p/src/rag_bridge.rs`

**Rust 9P Server Example**:
```rust
/* src/server.rs */
use rust_9p::server::Server;
use aichat::Client;

pub struct AIChatServer {
    client: Client,
    server: Server,
}

impl AIChatServer {
    pub fn new() -> Self {
        let client = Client::new();
        let server = Server::new();
        
        // Register handlers
        server.register_file("/mnt/llm/models/gpt-4/chat", 
            Box::new(ChatHandler::new(client.clone())));
        
        AIChatServer { client, server }
    }
    
    pub fn run(&self) {
        self.server.listen("0.0.0.0:9999");
    }
}
```

**Dependencies**:
- `aichat` (Rust crate)
- `rust-9p` (9P protocol implementation)
- AtomSpace-9P (for RAG)

### 6. AzStaHCog → AGI-OS

**Source**: `AzStaHCog-main/`

**Target**: `core/infrastructure/azstahcog/`

**Aion Cognitive Architecture Mapping**:

| Aion Component | AGI-OS Mapping | Implementation |
|----------------|----------------|----------------|
| **Entelechy System** | Cognitive scheduler | Attention-based VM scheduling |
| **Ontogenesis Pipeline** | Learning system | Developmental emergence |
| **Echo Reflection** | Meta-cognition | Self-monitoring and adaptation |
| **Cognitive Monitoring** | Telemetry | Coherence tracking |

**Integration Architecture**:
```
┌──────────────────────────────────────────────────────────┐
│  Azure Stack HCI                                         │
│  ├── Hyper-V (VMs as cognitive processes)               │
│  ├── SDN (cognitive communication)                       │
│  └── Storage Spaces Direct (cognitive memory)            │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Aion Cognitive Architecture                             │
│  ├── Entelechy (purposeful optimization)                │
│  ├── Ontogenesis (developmental stages)                 │
│  ├── Echo Reflection (introspection)                    │
│  └── Cognitive Monitoring (coherence)                   │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  AGI-OS Cognitive Scheduler                              │
│  (core/inferno-kernel/cognitive-scheduler/)              │
└──────────────────────────────────────────────────────────┘
```

**Files to Create**:
- `core/infrastructure/azstahcog/CMakeLists.txt`
- `core/infrastructure/azstahcog/entelechy-scheduler.c`
- `core/infrastructure/azstahcog/ontogenesis-pipeline.c`
- `core/infrastructure/azstahcog/echo-reflection.c`
- `core/infrastructure/azstahcog/cognitive-monitoring.c`

**Dependencies**:
- Azure Stack HCI APIs
- Hyper-V management libraries
- `libinferno-kernel` (cognitive scheduler)

### 7. 9base → AGI-OS

**Source**: `9base-debian-master/`

**Target**: `core/userland/9base/`

**Integration Strategy**: Port 9base tools to AGI-OS

**Tools to Port**:
- `rc` - Plan 9 shell
- `sam` - Structural editor
- `acme` - Text editor/IDE
- `plumber` - Message routing
- Standard utilities (ls, cat, grep, awk, sed, etc.)

**Porting Process**:
1. Replace Unix syscalls with 9P operations
2. Compile against Inferno kernel APIs
3. Link with Dis VM for bytecode execution
4. Package as Debian packages

**Files to Modify**:
- `core/userland/9base/Makefile` - Update build system
- `core/userland/9base/lib9/syscalls.c` - Replace with 9P ops
- `core/userland/9base/rc/rc.h` - Update for Inferno
- `core/userland/9base/sam/sam.c` - Update for 9P

**Dependencies**:
- `libinferno-kernel`
- `lib9p-protocol`
- `libdis-vm`

### 8. Hyprland → AGI-OS

**Source**: `hyprland-debian-latest/`

**Target**: `core/display/hyprland-9p/`

**Integration Architecture**:
```
┌──────────────────────────────────────────────────────────┐
│  Hyprland Wayland Compositor                             │
│  ├── Window management                                   │
│  ├── Input handling                                      │
│  └── Rendering                                           │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Hyprland-9P Bridge                                      │
│  ├── Expose windows as 9P files                         │
│  ├── Input via 9P writes                                │
│  └── Screenshot via 9P reads                            │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Inferno 9P Protocol Stack                               │
└──────────────────────────────────────────────────────────┘
```

**9P Namespace**:
```
/mnt/display/
  ├── windows/
  │   ├── window-1/
  │   │   ├── title
  │   │   ├── geometry
  │   │   ├── screenshot
  │   │   └── ctl
  │   └── window-2/
  ├── workspaces/
  └── ctl
```

**Files to Create**:
- `core/display/hyprland-9p/CMakeLists.txt`
- `core/display/hyprland-9p/hyprland-9p-plugin.cpp`
- `core/display/hyprland-9p/window-namespace.cpp`
- `core/display/hyprland-9p/input-handler.cpp`

**Dependencies**:
- `hyprland-dev`
- `lib9p-protocol`
- Wayland libraries

### 9. Pattern Language → AGI-OS

**Source**: Pattern language documents and CSVs

**Target**: `core/cognition/patterns/`

**Integration Architecture**:
```
┌──────────────────────────────────────────────────────────┐
│  Pattern Catalog (253 patterns)                         │
│  ├── TheTimelessWayofBuilding.md                        │
│  ├── PatternsList-APL(1).md                             │
│  └── laetusinpraesens*.csv                              │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Pattern Parser & Loader                                 │
│  ├── Parse pattern descriptions                         │
│  ├── Extract forces and solutions                       │
│  └── Build pattern graph                                │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  AtomSpace-9P Representation                             │
│  ├── Patterns as ConceptNodes                           │
│  ├── Forces as PredicateNodes                           │
│  └── Relationships as Links                             │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  PLN-9P Reasoning                                        │
│  ├── Pattern composition                                │
│  ├── Pattern discovery                                  │
│  └── Quality assessment                                 │
└──────────────────────────────────────────────────────────┘
```

**Files to Create**:
- `core/cognition/patterns/CMakeLists.txt`
- `core/cognition/patterns/pattern-parser.cpp`
- `core/cognition/patterns/pattern-loader.cpp`
- `core/cognition/patterns/pattern-atomspace.cpp`
- `core/cognition/patterns/pattern-reasoning.cpp`
- `core/cognition/patterns/quality-assessment.cpp`

**Pattern Representation in AtomSpace**:
```scheme
; Pattern 88: Street Cafe
(ConceptNode "Pattern88:StreetCafe")

; Context
(ContextLink
  (ConceptNode "Pattern88:StreetCafe")
  (ConceptNode "PublicSpace")
  (ConceptNode "UrbanSetting"))

; Forces
(EvaluationLink
  (PredicateNode "resolves-force")
  (ListLink
    (ConceptNode "Pattern88:StreetCafe")
    (ConceptNode "NeedForLeisure")))

(EvaluationLink
  (PredicateNode "resolves-force")
  (ListLink
    (ConceptNode "Pattern88:StreetCafe")
    (ConceptNode "SocialConnection")))

; Solution elements
(MemberLink
  (ConceptNode "IntimateRooms")
  (ConceptNode "Pattern88:StreetCafe"))

(MemberLink
  (ConceptNode "TablesInStreet")
  (ConceptNode "Pattern88:StreetCafe"))

; Related patterns
(AssociativeLink
  (ConceptNode "Pattern88:StreetCafe")
  (ConceptNode "Pattern30:ActivityNodes"))

(AssociativeLink
  (ConceptNode "Pattern88:StreetCafe")
  (ConceptNode "Pattern106:PositiveOutdoorSpace"))
```

**Dependencies**:
- `libatomspace-9p`
- `libpln-9p`
- JSON/CSV parsing libraries

### 10. NetLogo Models → AGI-OS

**Source**: `UrbanSuite-Bordertowns.nlogo`, `UrbanSuite-TijuanaBordertowns.nlogo`

**Target**: `core/simulation/netlogo-9p/`

**Integration Architecture**:
```
┌──────────────────────────────────────────────────────────┐
│  NetLogo Runtime                                         │
│  ├── Model execution                                     │
│  ├── Agent simulation                                    │
│  └── Data collection                                     │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  NetLogo-9P Bridge                                       │
│  ├── Expose models as 9P files                          │
│  ├── Control via 9P writes                              │
│  └── Results via 9P reads                               │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Inferno 9P Protocol Stack                               │
└──────────────────────────────────────────────────────────┘
```

**9P Namespace**:
```
/mnt/sim/
  ├── models/
  │   ├── bordertowns/
  │   │   ├── setup
  │   │   ├── go
  │   │   ├── parameters/
  │   │   └── results/
  │   └── tijuana/
  ├── agents/
  └── ctl
```

**Files to Create**:
- `core/simulation/netlogo-9p/CMakeLists.txt`
- `core/simulation/netlogo-9p/netlogo-9p-server.java`
- `core/simulation/netlogo-9p/model-namespace.java`
- `core/simulation/netlogo-9p/jni-bridge.c`

**Dependencies**:
- NetLogo runtime
- JNI (Java Native Interface)
- `lib9p-protocol`

## Build Dependency Graph

```
Layer 0: Inferno Kernel
├── dis-vm
├── 9p-protocol
└── namespace-core
    │
    ▼
Layer 1: 9P-Enabled OpenCog
├── atomspace-9p (depends on: atomspace, 9p-protocol)
├── pln-9p (depends on: pln, atomspace-9p)
└── ecan-9p (depends on: attention, atomspace-9p)
    │
    ▼
Layer 2: Communication Infrastructure
├── deltachat-9p (depends on: deltachat-core, 9p-protocol)
├── dovecot-9p (depends on: dovecot, 9p-protocol, atomspace-9p)
└── deep-tree-echo (depends on: deltachat-9p, dovecot-9p, atomspace-9p)
    │
    ▼
Layer 3: Cognitive Agents
├── elizaos-9p (depends on: elizaos-cpp, 9p-protocol, atomspace-9p, pln-9p)
├── aicogchat-9p (depends on: aichat, 9p-protocol, atomspace-9p)
└── azstahcog (depends on: azure-stack-hci, cognitive-scheduler)
    │
    ▼
Layer 4: Pattern Language & Simulation
├── patterns (depends on: atomspace-9p, pln-9p)
└── netlogo-9p (depends on: netlogo, 9p-protocol)
    │
    ▼
Layer 5: Userland
├── 9base (depends on: inferno-kernel, 9p-protocol)
└── hyprland-9p (depends on: hyprland, 9p-protocol)
```

## CMakeLists.txt Integration

Update `core/CMakeLists.txt`:

```cmake
# Layer 2: Communication Infrastructure
OPTION(BUILD_DELTACHAT_9P "Build DeltaChat 9P bridge" ON)
OPTION(BUILD_DOVECOT_9P "Build Dovecot 9P plugin" ON)
OPTION(BUILD_DEEP_TREE_ECHO "Build Deep-Tree-Echo" ON)

# Layer 3: Cognitive Agents
OPTION(BUILD_ELIZAOS_9P "Build ElizaOS 9P bridge" ON)
OPTION(BUILD_AICOGCHAT_9P "Build AICogChat 9P bridge" ON)
OPTION(BUILD_AZSTAHCOG "Build AzStaHCog integration" OFF)

# Layer 4: Pattern Language & Simulation
OPTION(BUILD_PATTERNS "Build Pattern Language system" ON)
OPTION(BUILD_NETLOGO_9P "Build NetLogo 9P bridge" OFF)

# Layer 5: Userland
OPTION(BUILD_9BASE "Build 9base userland tools" ON)
OPTION(BUILD_HYPRLAND_9P "Build Hyprland 9P bridge" OFF)

# Communication Infrastructure
IF(BUILD_DELTACHAT_9P AND BUILD_INFERNO_KERNEL)
    add_subdirectory(communication/deltachat-9p)
ENDIF()

IF(BUILD_DOVECOT_9P AND BUILD_INFERNO_KERNEL)
    add_subdirectory(communication/dovecot-9p)
ENDIF()

IF(BUILD_DEEP_TREE_ECHO AND BUILD_DELTACHAT_9P AND BUILD_DOVECOT_9P)
    add_subdirectory(communication/deep-tree-echo)
ENDIF()

# Cognitive Agents
IF(BUILD_ELIZAOS_9P AND BUILD_ATOMSPACE_9P)
    add_subdirectory(agents/elizaos-9p)
ENDIF()

IF(BUILD_AICOGCHAT_9P AND BUILD_ATOMSPACE_9P)
    add_subdirectory(agents/aicogchat-9p)
ENDIF()

# Pattern Language
IF(BUILD_PATTERNS AND BUILD_ATOMSPACE_9P AND BUILD_PLN_9P)
    add_subdirectory(cognition/patterns)
ENDIF()

# Userland
IF(BUILD_9BASE AND BUILD_INFERNO_KERNEL)
    add_subdirectory(userland/9base)
ENDIF()
```

## Debian Packaging

New packages to create:

1. `libdeltachat-9p1` - DeltaChat 9P bridge
2. `libdovecot-9p1` - Dovecot 9P plugin
3. `libdeep-tree-echo1` - Deep-Tree-Echo
4. `libelizaos-9p1` - ElizaOS 9P bridge
5. `libaicogchat-9p1` - AICogChat 9P bridge
6. `libpatterns1` - Pattern Language system
7. `9base-agi-os` - 9base userland tools

## Summary

This mapping provides:

1. **Clear integration points** for each resource
2. **Dependency graphs** showing build order
3. **File structures** for each integration
4. **CMake configuration** for build system
5. **Debian packaging** for distribution

All integrations follow the **9P-first philosophy**: everything is exposed as file servers, enabling uniform access and composition.

---

**Date**: December 13, 2025  
**Status**: Mapping complete  
**Next**: Design unified integration strategy
