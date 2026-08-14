# NPU Documentation Index

Welcome to the NPU (Neural Processing Unit) documentation. This index provides quick access to all technical documentation and formal specifications.

## Quick Links

### 📖 Main Documentation
- [README.md](../README.md) - Main project overview and quick start
- [IMPLEMENTATION.md](../IMPLEMENTATION.md) - Implementation summary and test results
- [Architecture Overview](architecture_overview.md) - **Comprehensive architecture documentation** ⭐

### 🔬 Formal Specifications
- [Formal Spec Index](formal-spec/README.md) - Guide to Z++ specifications
- [Data Model](formal-spec/data_model.zpp) - Data layer formalization
- [System State](formal-spec/system_state.zpp) - Complete system state model
- [Operations](formal-spec/operations.zpp) - Operation specifications with pre/post-conditions
- [Integrations](formal-spec/integrations.zpp) - External integration contracts

### 📊 Summary & Statistics
- [Documentation Summary](DOCUMENTATION_SUMMARY.md) - Complete generation summary with statistics

### 🔧 Implementation Guides
- [Virtual Device README](../fhp/VIRTUAL_DEVICE_README.md) - Virtual PCB hardware guide
- [GGUF Integration](../fhp/GGUF_INTEGRATION.md) - GGUF runtime integration details
- [Financial Hardware Implementation](../fhp/financial-hardware-implementation.md) - Financial device implementation

## Documentation Overview

### Architecture Documentation (692 lines)

**File**: [architecture_overview.md](architecture_overview.md)

Comprehensive technical architecture with:
- ✅ 11 Mermaid diagrams (system components, memory map, class diagrams, sequence diagrams, state machines)
- ✅ Complete memory architecture and register layouts
- ✅ Component architecture with UML diagrams
- ✅ Data flow architecture (LLM inference, financial transactions)
- ✅ Integration architecture (GGUF runtime, I/O streams)
- ✅ State machines (device lifecycle, inference operations)
- ✅ Performance characteristics
- ✅ GPIO pin allocation and financial mapping
- ✅ Technology stack and design patterns

### Formal Specifications (2,454 lines)

**Directory**: [formal-spec/](formal-spec/)

Rigorous Z++ mathematical specifications:

1. **[data_model.zpp](formal-spec/data_model.zpp)** (475 lines)
   - Base types and memory regions
   - GPIO pins and financial mappings
   - DMA and interrupt controllers
   - Hardware register layouts
   - Configuration structures

2. **[system_state.zpp](formal-spec/system_state.zpp)** (411 lines)
   - VirtualPCBState with all hardware
   - FinancialDeviceState with registers
   - LLMCoprocessorState with inference tracking
   - NPUSystemState (top-level integration)
   - System invariants and state transitions

3. **[operations.zpp](formal-spec/operations.zpp)** (643 lines)
   - Virtual PCB lifecycle operations
   - Memory and GPIO operations
   - Financial device operations
   - LLM coprocessor operations (model loading, inference, tokens)
   - DMA and error handling

4. **[integrations.zpp](formal-spec/integrations.zpp)** (641 lines)
   - GGUF runtime integration (tokenization, generation, sampling)
   - I/O stream integration (UART, SPI, I2C)
   - External data integration
   - Rate limiting and retry policies

5. **[README.md](formal-spec/README.md)** (284 lines)
   - Z++ notation guide
   - Usage for verification, implementation, testing
   - Relationship to implementation
   - Invariant checking guidelines

## By Topic

### Hardware Architecture
- [Architecture Overview](architecture_overview.md) - Complete system architecture
- [Virtual Device README](../fhp/VIRTUAL_DEVICE_README.md) - Hardware abstraction details
- [Data Model Spec](formal-spec/data_model.zpp) - Hardware formalization

### Memory & Registers
- [Architecture Overview - Memory Map](architecture_overview.md#memory-architecture) - Memory layout
- [Data Model - Memory Regions](formal-spec/data_model.zpp) - Memory region schemas
- [System State - Register Mapping](formal-spec/system_state.zpp) - Register synchronization

### GPIO & Financial Mapping
- [Architecture Overview - GPIO Allocation](architecture_overview.md#gpio-pin-allocation) - Pin mapping
- [Data Model - Account Pins](formal-spec/data_model.zpp) - Financial mappings
- [Operations - Financial Ops](formal-spec/operations.zpp) - Balance operations

### LLM Integration
- [Architecture Overview - LLM Pipeline](architecture_overview.md#llm-inference-pipeline) - Inference flow
- [GGUF Integration](../fhp/GGUF_INTEGRATION.md) - GGUF runtime details
- [Integrations Spec](formal-spec/integrations.zpp) - GGUF contracts
- [Operations - LLM Ops](formal-spec/operations.zpp) - Inference operations

### State Management
- [Architecture Overview - State Machines](architecture_overview.md#state-machine-architecture) - State diagrams
- [System State Spec](formal-spec/system_state.zpp) - Complete state model
- [Operations Spec](formal-spec/operations.zpp) - State transitions

### External Integration
- [Architecture Overview - Integration](architecture_overview.md#integration-architecture) - Integration boundaries
- [Integrations Spec](formal-spec/integrations.zpp) - External contracts
- [GGUF Integration](../fhp/GGUF_INTEGRATION.md) - Runtime integration

## Visual Diagrams

All Mermaid diagrams are in [architecture_overview.md](architecture_overview.md):

1. **System Components** - High-level architecture
2. **Memory Map** - Address space layout
3. **Virtual PCB Core** - Component class diagram
4. **Device Drivers** - Driver architecture
5. **LLM Inference Pipeline** - Sequence diagram
6. **Financial Transaction** - Sequence diagram
7. **External Boundaries** - Integration diagram
8. **GGUF Integration** - Runtime layers
9. **Device States** - State machine
10. **Inference States** - State machine
11. **GPIO Allocation** - Pin mapping

## Statistics

| Metric | Value |
|--------|-------|
| Total Documentation | 3,146 lines |
| Architecture Docs | 692 lines |
| Formal Specifications | 2,454 lines |
| Mermaid Diagrams | 11 |
| Z++ Schemas | 50+ |
| Z++ Operations | 45+ |
| Files | 7 |

## For Different Audiences

### For Developers
**Start here**:
1. [README.md](../README.md) - Project overview
2. [Architecture Overview](architecture_overview.md) - System understanding
3. [Operations Spec](formal-spec/operations.zpp) - Operation contracts

**Then explore**:
- [Virtual Device README](../fhp/VIRTUAL_DEVICE_README.md) - Hardware API
- [GGUF Integration](../fhp/GGUF_INTEGRATION.md) - LLM integration

### For Verification Engineers
**Start here**:
1. [Formal Spec Guide](formal-spec/README.md) - Z++ notation
2. [Data Model](formal-spec/data_model.zpp) - Invariants
3. [System State](formal-spec/system_state.zpp) - State constraints

**Then explore**:
- [Operations Spec](formal-spec/operations.zpp) - Pre/post-conditions
- [Integrations Spec](formal-spec/integrations.zpp) - External contracts

### For Architects
**Start here**:
1. [Architecture Overview](architecture_overview.md) - Complete architecture
2. [Documentation Summary](DOCUMENTATION_SUMMARY.md) - Coverage analysis

**Then explore**:
- [System State Spec](formal-spec/system_state.zpp) - State model
- [Implementation Summary](../IMPLEMENTATION.md) - Implementation details

### For QA/Testers
**Start here**:
1. [Architecture Overview](architecture_overview.md) - System behavior
2. [Operations Spec](formal-spec/operations.zpp) - Test scenarios

**Then explore**:
- [Data Model](formal-spec/data_model.zpp) - Boundary conditions
- [Integrations Spec](formal-spec/integrations.zpp) - External APIs

## Repository Structure

```
npu/
├── docs/                           # Documentation (this directory)
│   ├── README.md                   # This index
│   ├── architecture_overview.md    # Complete architecture with diagrams
│   ├── DOCUMENTATION_SUMMARY.md    # Generation summary
│   └── formal-spec/                # Z++ formal specifications
│       ├── README.md               # Specification guide
│       ├── data_model.zpp          # Data layer
│       ├── system_state.zpp        # System state
│       ├── operations.zpp          # Operations
│       └── integrations.zpp        # Integrations
├── fhp/                            # Implementation
│   ├── virtual-device.h/cpp        # Virtual PCB
│   ├── llama-coprocessor-driver.h/cpp  # LLM driver
│   ├── financial-device-driver.h/cpp   # Financial driver
│   └── *.md                        # Implementation docs
├── README.md                       # Main project README
└── IMPLEMENTATION.md               # Implementation summary
```

## Next Steps

1. **Read** [Architecture Overview](architecture_overview.md) for system understanding
2. **Study** Mermaid diagrams for visual comprehension
3. **Review** [Formal Specifications](formal-spec/) for precise behavior
4. **Check** implementation against specifications
5. **Use** specifications for verification and testing

## Contributing

When contributing to NPU:
1. ✅ Review relevant architecture documentation
2. ✅ Check formal specifications for constraints
3. ✅ Ensure changes maintain invariants
4. ✅ Update documentation if architecture changes
5. ✅ Verify pre/post-conditions are satisfied

## License

All documentation is part of the NPU project and follows the same AGPL-3.0 license.

---

**Last Updated**: 2025-12-03  
**Repository**: https://github.com/o9nn/npu  
**Documentation Version**: 1.0
