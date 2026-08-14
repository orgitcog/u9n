
# The Pure Limbo DTESN: A Unified Cognitive Architecture

**Date**: December 29, 2025
**Project**: `un9n` - Distributed Cognitive Architecture

## 1. The Vision: DTESN as an Inferno Service

This document outlines the architecture of a pure Limbo DTESN. In this vision, the DTESN is not an application running on an operating system; it is an **Inferno service**, deeply integrated with the OS and accessible to all other Inferno services.

## 2. Core Architectural Principles

- **Everything is a File**: The DTESN itself is a file server. Its internal state (reservoirs, agents, arenas) is exposed as a file system that other processes can interact with.
- **Concurrency is Native**: The concurrent components of the DTESN (agents, arenas, relations) are implemented as native Limbo threads, communicating via channels.
- **Distribution is Transparent**: The b9/p9/j9 layers are not separate systems; they are different views of the same unified Inferno namespace.

## 3. The DTESN File System

The DTESN service would mount a file system at `/services/dtesn`, with the following structure:

```
/services/dtesn/
    ctl             # Control file for the service
    models/         # Directory of available models
        llama2/
            ctl
            prompt
            tokens
    agents/         # Directory of active agents
        agent1/
            ctl
            state
            in
            out
    arenas/         # Directory of active arenas
        arena1/
            ctl
            state
            relations
```

- **`ctl` files**: Used to send commands to the service (e.g., `create agent`, `load model`).
- **`prompt`/`tokens`**: Writing to `prompt` initiates a generation, and the results are read from `tokens`.
- **`in`/`out`**: Channels for communicating with an agent.

## 4. Module Breakdown

The DTESN would be implemented as a collection of Limbo modules:

| Module | Role | Inferno/Limbo Features Used |
|---|---|---|
| `dtesn.m` | Main service module | Spawns threads, manages the file system |
| `ggml.m` | Tensor library | Implements core tensor operations in Limbo |
| `llama.m` | LLM inference | Uses `ggml.m` to run LLM models |
| `esn.m` | Echo State Network | Implements the core reservoir computing dynamics |
| `agent.m` | Agent implementation | A Limbo thread that communicates via channels |
| `arena.m` | Arena implementation | Manages the state of an arena and its agents |

## 5. A Day in the Life of a Pure Limbo DTESN

1.  A client process (e.g., a UI) opens `/services/dtesn/ctl` and writes the command `create agent`.
2.  The `dtesn` service spawns a new Limbo thread for the agent and creates a new directory `/services/dtesn/agents/agent2`.
3.  The client opens `/services/dtesn/agents/agent2/in` and writes a message.
4.  The agent thread receives the message from its input channel, processes it using the `llama` and `esn` modules, and writes a response to its output channel.
5.  The client reads the response from `/services/dtesn/agents/agent2/out`.

## 6. The b9/p9/j9 Layers Revisited

In this architecture, the layers are defined by their **namespace** and **resources**:

- **b9 (Edge)**: A process running on a mobile device. Its namespace includes the local `/services/dtesn` and mounts the remote `/services/dtesn` from the server.
- **p9 (Server)**: The main `dtesn` service running on a server. It has access to powerful hardware and large models.
- **j9 (Network)**: The Styx protocol itself. It is the fabric that connects the b9 and p9 namespaces.

## 7. Conclusion: A Radical Simplification

This pure Limbo architecture is a radical simplification of the current multi-language, multi-protocol stack. It is a return to the original vision of Plan 9 and Inferno: a simple, elegant, and powerful system for distributed computing, where the operating system itself is the cognitive architecture.
