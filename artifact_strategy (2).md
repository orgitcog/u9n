# The Artifact Strategy: A Three-Layer Distribution Model

**Date**: December 29, 2025
**Project**: `un9n` - Distributed Cognitive Architecture

## 1. The Question: C/H/O/SO vs. M/B/DIS?

You've asked a crucial question that bridges the gap between the C compilation model (`.h`, `.c`, `.o`, `.so`) and the Inferno/Limbo model (`.m`, `.b`, `.dis`). The core of the question is: **What artifacts should we distribute for each layer of the DTESN architecture?**

This is a strategic decision about the trade-offs between distribution size, flexibility, performance, and security. The answer, like the architecture itself, is synergistic and layer-specific.

## 2. Mapping the Compilation Models

First, let's map the concepts:

| C/C++ Artifact | Role | Inferno/Limbo Analog | Role |
|---|---|---|---|
| `.h` (Header) | Interface Definition | `.m` (Module Interface) | Defines the public API of a module. |
| `.c` (Source) | Implementation | (Internal to `.b`) | The actual code. |
| `.o` (Object) | Compiled, Unlinked Code | `.b` (Bytecode Object) | Compiled module, not yet executable. |
| `.so` (Shared Object) | Dynamically Linked Library | `.dis` (Dis VM Executable) | A self-contained, executable unit. |

Your insight is correct: the `.so` is the closest C-world equivalent to a Limbo `.dis` file—a compiled, distributable, executable unit. The key difference is that `.so` files are **architecture-specific**, while `.dis` files are **architecture-independent** bytecode for the Dis VM.

## 3. The Optimal Artifact Strategy by Layer

The answer is not "all of the above" for every layer. We need a task-specific strategy.

### Layer 1: Edge (b9 - Mobile Clients)

- **Artifacts**: **`.so` files (Shared Objects)**
- **Rationale**: For mobile deployment on iOS and Android, pre-compiled, architecture-specific shared libraries are the only practical solution. App stores expect compiled binaries. We cannot (and should not) compile source code on the user's device.
- **Process**: The build system will cross-compile the entire C++/Rust mobile stack into `.so` files for each target architecture (e.g., `arm64-v8a`, `armeabi-v7a`, `x86_64`). These are then bundled into the final APK/IPA.
- **Limbo Analogy**: This is like shipping a pre-compiled `.dis` executable for each specific hardware architecture.

### Layer 2: Server (p9 - Execution Kernels)

This layer is a polyglot environment, so it needs a multi-artifact strategy.

| Service | Artifacts to Distribute | Rationale |
|---|---|---|
| **API Gateway (Rust)** | **Single Static Binary** | `cargo build --release` produces a self-contained executable. This is the simplest, most robust deployment model. It's like a native `.dis` file. |
| **Fast Path (C++)** | **`.o` (Object Files)** | For maximum performance, we need to perform Link-Time Optimization (LTO) and Profile-Guided Optimization (PGO) on the server. This requires the build server to have access to the intermediate `.o` files to produce the most optimized final executable. |
| **Heavy Lifting (PyTorch)** | **`.pt` (TorchScript) + `.so` (LibTorch)** | We distribute the JIT-compiled models (`.pt` files), which are architecture-independent computation graphs (the `.dis` file). The server needs the LibTorch runtime (`.so` files) to execute them. |
| **DTESN Core (C++)** | **`.o` (Object Files)** | Same as the Fast Path. The core cognitive dynamics will benefit most from advanced compiler optimizations like LTO, which requires the intermediate object files at link time. |

### Layer 3: Network (j9 - Distribution Channels)

This layer is all about distribution and fault tolerance.

| Service | Artifacts to Distribute | Rationale |
|---|---|---|
| **Real-time Sync (Elixir)** | **`.beam` files + Release** | The Elixir compiler produces `.beam` files, which are bytecode for the BEAM VM (the `.dis` file). These are bundled into a self-contained OTP release using a tool like `mix release`. This release includes the BEAM VM itself and can be deployed anywhere. |
| **RPC Backbone (Go)** | **Single Static Binary** | Like Rust, Go compiles to a single, self-contained executable. This is perfect for simple, scalable microservices. |
| **Secure Channels (Rust)** | **Single Static Binary** | Same as the Go services, `cargo build` produces a deployable binary. |

## 4. The Unified Distribution Strategy

Here is the final, unified strategy:

| Layer | Primary Artifact | Why? |
|---|---|---|
| **b9 (Edge)** | **`.so`** | **Pre-compiled native code** for mobile app stores. |
| **p9 (Server)** | **`.o` + `.pt`** | **Intermediate objects** for link-time optimization and **JIT-compiled models** for ML flexibility. |
| **j9 (Network)** | **`.beam` + Binaries** | **Architecture-independent bytecode** for the BEAM VM and **static binaries** for Go/Rust services. |

**The answer is not to have all file types everywhere.** The answer is to use the artifact type that best suits the deployment target and performance requirements of each specific layer and service.

- **Source code (`.c`, `.h`, `.m`)** lives only in the source repository and is used by the CI/CD build system.
- **Production servers** receive optimized binaries, bytecode, or linkable objects, but never raw source code.

This hybrid approach gives us the best of all worlds: the performance of C++, the safety of Rust, the flexibility of PyTorch, the fault tolerance of Elixir, and a deployment strategy that is both efficient and secure.
