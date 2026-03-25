# NPU Z++ Formal Specifications

This directory contains rigorous Z++ formal specifications for the NPU (Neural Processing Unit) Virtual PCB architecture.

## Overview

The formal specifications provide a mathematically precise model of the NPU system, covering:

- **Data structures** and their invariants
- **System state** and state transitions
- **Operations** with pre/post-conditions
- **External integrations** and their contracts

These specifications serve as:
1. **Definitive documentation** of system behavior
2. **Verification basis** for correctness proofs
3. **Implementation guide** for developers
4. **Contract specifications** for external integrations

## Specification Files

### 1. data_model.zpp

Formalization of the data layer, including:

- **Base types**: Memory addresses, device states, GPIO modes, interrupt types
- **Memory regions**: SRAM, FLASH, PERIPHERAL with address ranges and invariants
- **GPIO pins**: Pin modes, states, analog values with 12-bit ADC resolution
- **Financial mappings**: Account-to-pin mappings with balance-to-voltage conversion
- **DMA controller**: Transfer descriptors and channel management
- **Interrupt controller**: Interrupt descriptors, priorities, and pending queues
- **Power management**: Voltage rails (3.3V, 5V) and thermal monitoring
- **Hardware registers**: LLM NPU and Financial Device register layouts
- **Configuration structures**: LLM model config, sequence config, telemetry
- **Device drivers**: Base driver schema and specialized drivers

**Key Invariants:**
- Memory regions are disjoint
- GPIO pin numbers are unique (0-63)
- Analog values are within 12-bit range (0-4095)
- Voltage rails are within specification (3.3V ± 0.1V, 5V ± 0.1V)
- Account pin mappings are bijective
- Financial pin ranges partition the GPIO space

### 2. system_state.zpp

Complete system state model integrating all components:

- **VirtualPCBState**: Hardware device state with memory, GPIO, DMA, interrupts, power
- **FinancialDeviceState**: Financial driver state with registers and transaction buffer
- **LLMCoprocessorState**: LLM driver state with registers and inference state
- **NPUSystemState**: Top-level state combining all subsystems

**Key Invariants:**
- System initialization dependencies (PCB → drivers → operations)
- Memory region consistency across subsystems
- GPIO pin consistency between PCB and financial driver
- Register values synchronized with peripheral memory
- Double-entry accounting equation preservation
- Token generation progress bounds
- Telemetry consistency with actual operations

**State Transition Predicates:**
- Valid device state transitions (uninitialized → initializing → ready → running → ...)
- Monotonic counters (uptime, transaction count, token count)
- Model lifecycle constraints (cannot unload during inference)

### 3. operations.zpp

Formal operation specifications with pre/post-conditions:

#### Virtual PCB Lifecycle
- `InitializeVirtualPCB`: Initialize hardware from uninitialized state
- `CompleteInitialization`: Transition from initializing to ready
- `StartVirtualPCB`: Start device operation (ready → running)
- `StopVirtualPCB`: Stop device operation (running → ready)

#### Memory Operations
- `ReadMemory`: Read byte from SRAM, FLASH, or PERIPHERAL regions
- `WriteMemory`: Write byte to writable memory regions

#### GPIO Operations
- `SetGPIOMode`: Configure pin mode (input, output, analog, PWM, interrupt)
- `SetGPIOState`: Set digital state (low, high, tristate)
- `SetGPIOAnalog`: Set analog value (0-4095)
- `ReadGPIOPin`: Read current pin state

#### Financial Device Operations
- `MapAccountToPin`: Map financial account code to GPIO pin
- `UpdateAccountBalance`: Update balance with voltage conversion
- `ReadAccountBalance`: Read balance from voltage

#### LLM Coprocessor Operations
- `LoadModel`: Load GGUF model with configuration
- `ConfigureInference`: Set prompt address, length, and prediction count
- `StartInference`: Begin token generation
- `GenerateToken`: Generate single token (abstracted)
- `CompleteInference`: Finish inference and update telemetry
- `PerformInference`: High-level composition of inference pipeline
- `ResetLLMDevice`: Reset to initial state

#### DMA Operations
- `StartDMATransfer`: Initiate memory transfer
- `IsDMAComplete`: Check transfer completion status

#### Error Handling
- `ReportError`: Log error and set system error flag
- `ClearError`: Clear error state

### 4. integrations.zpp

External integration contracts:

#### GGUF Runtime Integration
- **Types**: Token IDs, model handles, context handles
- **Configuration**: Model params, context params
- **Operations**:
  - `GGUFLoadModel`: Load model from .gguf file
  - `GGUFInitContext`: Initialize inference context
  - `GGUFTokenize`: Convert text to token IDs
  - `GGUFDetokenize`: Convert token IDs to text
  - `GGUFDecode`: Update KV cache with tokens
  - `GGUFSample`: Sample next token with temperature, top-k, top-p
  - `GGUFFreeRuntime`: Release resources

#### I/O Stream Integration
- **UART**: Serial communication with configurable baud rate, parity, flow control
- **SPI**: Serial Peripheral Interface with clock speed and mode
- **I2C**: Inter-Integrated Circuit with addressing modes
- **Operations**: Open, send, receive, close for each protocol

#### External Data Integration
- **ChartOfAccounts**: JSON structure for financial accounts
- **LoadChartOfAccounts**: Load and map accounts to GPIO pins

#### Rate Limiting & Retry
- **RateLimiter**: Request throttling with window-based limits
- **RetryPolicy**: Exponential backoff with configurable delays

#### Integration Compositions
- `LLMInferenceWithGGUF`: Complete inference pipeline with GGUF runtime
- `ValidatedFinancialTransaction`: Double-entry transaction with validation

## Reading the Specifications

### Notation Guide

- `∀` - Universal quantification ("for all")
- `∃` - Existential quantification ("there exists")
- `⇒` - Implication
- `⇔` - Equivalence
- `∧` - Logical AND
- `∨` - Logical OR
- `¬` - Logical NOT
- `∈` - Element of
- `⊆` - Subset of
- `∪` - Set union
- `∩` - Set intersection
- `∅` - Empty set
- `ℕ` - Natural numbers (0, 1, 2, ...)
- `ℕ₁` - Positive natural numbers (1, 2, 3, ...)
- `ℤ` - Integers
- `ℝ` - Real numbers
- `𝔹` - Booleans (true, false)
- `⇸` - Partial function
- `↦` - Mapping
- `⊕` - Function override
- `⌢` - Sequence concatenation
- `⟨⟩` - Empty sequence
- `#s` - Size of sequence/set s
- `dom f` - Domain of function f
- `ran f` - Range of function f
- `Σ` - Summation
- `⌊x⌋` - Floor function
- `Δ` - State change (before and after)
- `Ξ` - State unchanged
- `?` - Input parameter
- `!` - Output parameter
- `'` - After-state value

### Schema Notation

Schemas are structured specifications with:
1. **Declaration**: State variables and their types
2. **Predicates**: Constraints that must hold (below the `where` line)

Example:
```z++
SchemaName
  variable1 : Type1
  variable2 : Type2
where
  constraint1 ∧
  constraint2
```

### Operation Notation

Operations use:
- `Δ` prefix: State may change
- `Ξ` prefix: State remains unchanged
- `?` suffix: Input parameter
- `!` suffix: Output parameter

## Using the Specifications

### For Verification

The specifications can be used with Z++ tools to:
1. **Check consistency**: Ensure invariants are satisfiable
2. **Verify operations**: Prove pre-conditions establish post-conditions
3. **Validate refinements**: Show implementation satisfies specification
4. **Generate test cases**: Derive test scenarios from operations

### For Implementation

Developers should:
1. **Understand invariants**: Ensure code maintains all data invariants
2. **Respect pre-conditions**: Check all pre-conditions before operations
3. **Establish post-conditions**: Ensure operations produce specified results
4. **Preserve state**: Don't modify state except as specified

### For Testing

Test cases should:
1. **Cover state space**: Test all device states and transitions
2. **Verify invariants**: Check invariants hold before/after operations
3. **Test boundaries**: Verify behavior at limit values
4. **Validate errors**: Ensure error conditions are properly handled

## Relationship to Implementation

The formal specifications correspond to these implementation files:

| Specification | Implementation Files |
|---------------|---------------------|
| `data_model.zpp` | `virtual-device.h`, `financial-device-driver.h`, `llama-coprocessor-driver.h` |
| `system_state.zpp` | `virtual-device.cpp`, state management across all drivers |
| `operations.zpp` | All `.cpp` files with operation implementations |
| `integrations.zpp` | `llama-mock.h`, I/O stream classes, external APIs |

### Invariant Checking

Key invariants to verify in implementation:

1. **Memory safety**: All memory accesses within region bounds
2. **GPIO uniqueness**: Pin numbers are unique and < 64
3. **Register consistency**: Hardware registers match driver state
4. **Accounting equation**: Debits = Credits for financial operations
5. **Token bounds**: Generated tokens ≤ n_predict
6. **State transitions**: Only valid state transitions occur

## Future Extensions

The specifications can be extended to cover:

1. **Concurrency**: Thread safety and synchronization
2. **Real-time constraints**: Timing requirements and deadlines
3. **Security properties**: Access control and data protection
4. **Fault tolerance**: Error recovery and failover
5. **Performance guarantees**: Throughput and latency bounds

## References

### Z++ Notation
- ISO/IEC 13568:2002 - Z Formal Specification Notation
- "The Z Notation: A Reference Manual" by J.M. Spivey
- "Using Z: Specification, Refinement, and Proof" by Woodcock & Davies

### Formal Methods
- "Software Abstractions" by Daniel Jackson
- "Formal Specification and Documentation using Z" by Jonathan Bowen
- "Refinement in Z and Object-Z" by John Derrick & Eerke Boiten

### Related Specifications
- [Architecture Overview](../architecture_overview.md) - System architecture documentation
- [Implementation](../../IMPLEMENTATION.md) - Implementation summary
- [Virtual Device README](../../fhp/VIRTUAL_DEVICE_README.md) - Virtual PCB guide

---

**Specification Version**: 1.0  
**Last Updated**: 2025-12-03  
**Repository**: https://github.com/o9nn/npu
