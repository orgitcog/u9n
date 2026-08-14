# NPU - Neural Processing Unit Coprocessor Architecture

## Executive Summary

The NPU (Neural Processing Unit) is a hardware-first abstraction framework that models GGUF-backed Large Language Model (LLM) inference as memory-mapped peripheral devices within a virtual Printed Circuit Board (PCB) architecture. This unique approach treats LLMs as hardware coprocessors with MMIO (Memory-Mapped I/O) registers, DMA controllers, and interrupt systems, enabling novel approaches to financial computation where accounting operations are modeled as electrical signals flowing through circuits.

### Key Architectural Principles

1. **Hardware-First Design**: LLM inference treated as peripheral device operations
2. **Memory-Mapped Interface**: All control through MMIO registers
3. **Virtual PCB Abstraction**: Complete simulation of physical hardware device
4. **Financial Circuit Modeling**: Accounts mapped to GPIO pins, transactions as signals
5. **Telemetry Integration**: Performance metrics exposed through hardware registers

## System Architecture Overview

### High-Level System Components

```mermaid
graph TB
    subgraph "Application Layer"
        CLI[CLI Admin Tool]
        FinSim[Financial Simulation]
        Tests[Test Suites]
    end
    
    subgraph "Device Driver Layer"
        FinDriver[Financial Device Driver<br/>0x40000000]
        LLMDriver[LLM Coprocessor Driver<br/>0x40001000]
    end
    
    subgraph "Virtual PCB Hardware"
        VirtualPCB[Virtual PCB Core]
        GPIO[64 GPIO Pins]
        Memory[Memory Subsystem]
        DMA[DMA Controller<br/>8 Channels]
        IRQ[Interrupt Controller<br/>256 Vectors]
        Power[Power Management<br/>3.3V, 5V Rails]
    end
    
    subgraph "I/O Interfaces"
        UART[UART Stream]
        SPI[SPI Stream]
        I2C[I2C Stream]
    end
    
    subgraph "External Integration"
        GGUF[GGUF Runtime<br/>llama.cpp]
        Models[GGUF Models<br/>.gguf files]
    end
    
    CLI --> FinDriver
    CLI --> LLMDriver
    FinSim --> FinDriver
    FinSim --> LLMDriver
    Tests --> FinDriver
    Tests --> LLMDriver
    
    FinDriver --> VirtualPCB
    LLMDriver --> VirtualPCB
    
    VirtualPCB --> GPIO
    VirtualPCB --> Memory
    VirtualPCB --> DMA
    VirtualPCB --> IRQ
    VirtualPCB --> Power
    
    VirtualPCB --> UART
    VirtualPCB --> SPI
    VirtualPCB --> I2C
    
    LLMDriver --> GGUF
    GGUF --> Models
    
    style VirtualPCB fill:#4a90e2,color:#fff
    style FinDriver fill:#7ed321,color:#fff
    style LLMDriver fill:#f5a623,color:#fff
    style GGUF fill:#d0021b,color:#fff
```

## Memory Architecture

### Memory Map Layout

```mermaid
graph LR
    subgraph "Physical Address Space"
        SRAM["SRAM Region<br/>0x20000000<br/>256 KB<br/>General Purpose Memory"]
        FLASH["FLASH Region<br/>0x08000000<br/>1 MB<br/>Firmware Storage"]
        PERIPH["PERIPHERAL Region<br/>0x40000000<br/>64 KB<br/>Device Registers"]
    end
    
    subgraph "PERIPH Subregions"
        FinRegs["Financial Device<br/>0x40000000<br/>Transaction Status<br/>Balance Total<br/>Transaction Count<br/>Error Codes"]
        LLMRegs["LLM NPU<br/>0x40001000<br/>Command Register<br/>Status Register<br/>Inference Config<br/>Token I/O<br/>Telemetry"]
    end
    
    PERIPH --> FinRegs
    PERIPH --> LLMRegs
    
    style SRAM fill:#50e3c2
    style FLASH fill:#f8e71c
    style PERIPH fill:#bd10e0
    style FinRegs fill:#7ed321
    style LLMRegs fill:#f5a623
```

### LLM NPU Register Layout (0x40001000 - 0x4000102F)

| Offset | Register Name | Description | Access |
|--------|--------------|-------------|--------|
| 0x00 | REG_CMD | Command register (reset, load, start, stop) | R/W |
| 0x04 | REG_STATUS | Status flags (idle, busy, EOG, error, ready) | R |
| 0x08 | REG_PROMPT_ADDR | SRAM address of prompt buffer | W |
| 0x0C | REG_PROMPT_LEN | Prompt length in bytes | W |
| 0x10 | REG_N_PREDICT | Number of tokens to generate | W |
| 0x14 | REG_TOKEN_OUT | Output token register | R |
| 0x18 | REG_TOKEN_READY | Token ready flag | R |
| 0x1C | REG_MODEL_ID | Loaded model identifier | R |
| 0x20 | REG_CTX_USED | Context size currently used | R |
| 0x24 | REG_ERROR_CODE | Hardware error code | R |
| 0x28 | REG_PERF_TOKENS_SEC | Performance counter (tokens/sec) | R |

### Financial Device Register Layout (0x40000000 - 0x4000000F)

| Offset | Register Name | Description | Access |
|--------|--------------|-------------|--------|
| 0x00 | REG_TRANSACTION_STATUS | Transaction processing status | R |
| 0x04 | REG_BALANCE_TOTAL | Total balance across all accounts | R |
| 0x08 | REG_TRANSACTION_COUNT | Number of transactions processed | R |
| 0x0C | REG_ERROR_CODE | Financial operation error codes | R |

## Component Architecture

### Virtual PCB Core

```mermaid
classDiagram
    class VirtualPCB {
        -string device_id
        -string model_number
        -DeviceState current_state
        -vector~MemoryRegion~ memory_regions
        -vector~GPIOPin~ gpio_pins
        -DMAController dma
        -InterruptController irq_ctrl
        -PowerManagement power
        +initialize() bool
        +start() bool
        +stop() bool
        +read_memory(addr) uint8_t
        +write_memory(addr, value)
        +attach_driver(driver) bool
    }
    
    class MemoryRegion {
        -uint64_t base_address
        -uint64_t size
        -string name
        -vector~uint8_t~ data
        -bool is_writable
        +read_byte(offset) uint8_t
        +write_byte(offset, value)
        +read_word(offset) uint16_t
        +write_word(offset, value)
    }
    
    class GPIOPin {
        -uint32_t pin_number
        -PinMode mode
        -PinState state
        -uint16_t analog_value
        +set_mode(mode)
        +set_state(state)
        +get_state() PinState
        +set_analog(value)
        +get_analog() uint16_t
    }
    
    class DMAController {
        -vector~DMATransfer~ active_transfers
        -size_t max_channels
        +start_transfer(src, dst, len) int
        +is_transfer_complete(channel) bool
        +abort_transfer(channel)
        +process_transfers()
    }
    
    class InterruptController {
        -array~InterruptDescriptor~ interrupt_table
        -queue~uint32_t~ pending_interrupts
        +register_handler(vector, handler) bool
        +enable_interrupt(vector)
        +disable_interrupt(vector)
        +trigger_interrupt(vector)
        +process_pending()
    }
    
    class PowerManagement {
        -map~string-double~ voltage_rails
        -map~string-double~ current_draw
        -double temperature_celsius
        +set_voltage(rail, voltage)
        +get_voltage(rail) double
        +get_current(rail) double
        +get_temperature() double
    }
    
    VirtualPCB "1" --> "*" MemoryRegion
    VirtualPCB "1" --> "64" GPIOPin
    VirtualPCB "1" --> "1" DMAController
    VirtualPCB "1" --> "1" InterruptController
    VirtualPCB "1" --> "1" PowerManagement
```

### Device Driver Architecture

```mermaid
classDiagram
    class DeviceDriver {
        <<abstract>>
        #VirtualPCB* pcb
        #string driver_name
        #string version
        +load(pcb) bool*
        +initialize() bool*
        +probe() bool*
        +remove() bool*
    }
    
    class FinancialDeviceDriver {
        -map~string-AccountPin~ account_pins
        -MemoryRegion* transaction_buffer
        -TelemetrySystem telemetry
        +map_account_to_pin(code, pin) bool
        +update_account_balance(code, balance) bool
        +read_account_balance(code) double
        +process_hardware_transaction(data) bool
        +get_hardware_diagnostics() string
        +run_self_test() bool
    }
    
    class LlamaCoprocessorDriver {
        -LlamaRuntime* llama_runtime
        -LlamaModelConfig model_cfg
        -LlamaTelemetry telemetry
        -uint32_t model_id
        +set_model_config(cfg)
        +load_model() bool
        +configure_inference(addr, len, cfg) bool
        +start_inference() bool
        +infer(prompt, cfg) string
        +infer_streaming(prompt, cfg, callback) bool
        +read_status() uint32_t
        +token_available() bool
        +read_token() int32_t
        +reset_device() bool
    }
    
    class LlamaModelConfig {
        +string model_path
        +string model_name
        +int32_t n_ctx
        +int32_t n_threads
        +int32_t n_gpu_layers
        +int32_t batch_size
        +bool offload_kv_cache
        +bool low_vram_mode
    }
    
    class LlamaSequenceConfig {
        +int32_t n_predict
        +int32_t max_ctx
        +bool echo_prompt
        +bool stream_tokens
        +int32_t stream_delay_ms
        +string system_prompt
    }
    
    class LlamaTelemetry {
        +double tokens_per_second
        +uint64_t total_tokens_generated
        +uint64_t total_prompts
        +uint64_t last_prompt_tokens
        +uint64_t last_completion_tokens
        +time_point last_inference_start
        +time_point last_inference_end
    }
    
    DeviceDriver <|-- FinancialDeviceDriver
    DeviceDriver <|-- LlamaCoprocessorDriver
    LlamaCoprocessorDriver --> LlamaModelConfig
    LlamaCoprocessorDriver --> LlamaSequenceConfig
    LlamaCoprocessorDriver --> LlamaTelemetry
```

## Data Flow Architecture

### LLM Inference Pipeline

```mermaid
sequenceDiagram
    participant App as Application
    participant Driver as LLM Coprocessor Driver
    participant Regs as Hardware Registers
    participant Runtime as GGUF Runtime
    participant Model as GGUF Model
    
    App->>Driver: infer("prompt text", config)
    Driver->>Runtime: tokenize(prompt)
    Runtime->>Model: encode text to tokens
    Model-->>Runtime: token_ids[]
    Runtime-->>Driver: token_ids[]
    
    Driver->>Regs: write REG_PROMPT_LEN
    Driver->>Regs: write REG_N_PREDICT
    Driver->>Regs: write REG_CMD (START_INF)
    Driver->>Regs: set STATUS_BUSY flag
    
    loop Token Generation
        Driver->>Runtime: generate_next_token()
        Runtime->>Model: sample from context
        Model-->>Runtime: token_id
        Runtime-->>Driver: token_id
        
        opt Streaming Mode
            Driver->>Regs: write REG_TOKEN_OUT
            Driver->>Regs: set TOKEN_READY flag
            Driver->>App: callback(token_text, token_id)
        end
    end
    
    Driver->>Runtime: detokenize(tokens)
    Runtime->>Model: decode tokens to text
    Model-->>Runtime: completion_text
    Runtime-->>Driver: completion_text
    
    Driver->>Regs: clear STATUS_BUSY
    Driver->>Regs: set STATUS_EOG
    Driver->>Regs: write REG_PERF_TOKENS_SEC
    Driver-->>App: completion_text
```

### Financial Transaction Pipeline

```mermaid
sequenceDiagram
    participant App as Application
    participant FinDriver as Financial Driver
    participant GPIO as GPIO Pins
    participant Regs as Financial Registers
    participant PCB as Virtual PCB
    
    App->>FinDriver: update_account_balance("1101", 50000.0)
    FinDriver->>FinDriver: map account to pin (1101 -> GPIO0)
    FinDriver->>FinDriver: convert balance to voltage (log scale)
    FinDriver->>GPIO: set_analog(pin=0, voltage=2.85V)
    GPIO->>PCB: update pin state
    PCB-->>GPIO: ack
    
    FinDriver->>Regs: update REG_BALANCE_TOTAL
    FinDriver->>Regs: increment REG_TRANSACTION_COUNT
    FinDriver-->>App: success
    
    App->>FinDriver: read_account_balance("1101")
    FinDriver->>GPIO: get_analog(pin=0)
    GPIO-->>FinDriver: voltage=2.85V
    FinDriver->>FinDriver: convert voltage to balance
    FinDriver-->>App: balance=50000.0
```

## Integration Architecture

### External System Boundaries

```mermaid
graph TB
    subgraph "NPU System"
        VirtualPCB[Virtual PCB]
        FinDriver[Financial Driver]
        LLMDriver[LLM Driver]
    end
    
    subgraph "External Runtime"
        GGUF[GGUF Runtime<br/>llama.cpp]
        Tokenizer[Tokenizer]
        Sampler[Token Sampler]
    end
    
    subgraph "External Data"
        Models[GGUF Model Files<br/>.gguf]
        ChartAccts[Chart of Accounts<br/>JSON]
    end
    
    subgraph "External I/O"
        UART_Dev[/dev/ttyUSB0<br/>UART Device]
        SPI_Dev[/dev/spidev0.0<br/>SPI Device]
        I2C_Dev[/dev/i2c-1<br/>I2C Device]
    end
    
    LLMDriver --> GGUF
    GGUF --> Tokenizer
    GGUF --> Sampler
    GGUF --> Models
    
    FinDriver --> ChartAccts
    
    VirtualPCB --> UART_Dev
    VirtualPCB --> SPI_Dev
    VirtualPCB --> I2C_Dev
    
    style GGUF fill:#d0021b,color:#fff
    style Models fill:#f5a623,color:#fff
    style ChartAccts fill:#7ed321,color:#fff
```

### GGUF Runtime Integration

```mermaid
graph LR
    subgraph "LlamaCoprocessorDriver"
        HighLevel[High-Level API<br/>infer, infer_streaming]
        LowLevel[Low-Level MMIO API<br/>configure, start, read_token]
        Integration[GGUF Integration Layer<br/>tokenize, generate, detokenize]
    end
    
    subgraph "LlamaRuntime Struct"
        ModelPtr[llama_model*]
        CtxPtr[llama_context*]
        Params[Model & Context Params]
    end
    
    subgraph "llama.cpp API"
        ModelLoad[llama_load_model_from_file]
        CtxCreate[llama_new_context_with_model]
        Tokenize[llama_tokenize]
        Decode[llama_decode]
        Sample[llama_sampler_sample]
    end
    
    HighLevel --> Integration
    Integration --> LowLevel
    LowLevel --> Registers[Hardware Registers]
    
    Integration --> ModelPtr
    Integration --> CtxPtr
    
    ModelPtr --> ModelLoad
    CtxPtr --> CtxCreate
    Integration --> Tokenize
    Integration --> Decode
    Integration --> Sample
    
    style Integration fill:#f5a623,color:#fff
    style Registers fill:#bd10e0,color:#fff
```

## State Machine Architecture

### Virtual PCB Device States

```mermaid
stateDiagram-v2
    [*] --> UNINITIALIZED
    UNINITIALIZED --> INITIALIZING : initialize()
    INITIALIZING --> READY : boot_sequence_complete
    INITIALIZING --> ERROR : initialization_failed
    
    READY --> RUNNING : start()
    RUNNING --> SLEEPING : enter_sleep_mode()
    SLEEPING --> RUNNING : wake_up()
    
    RUNNING --> READY : stop()
    RUNNING --> ERROR : runtime_error
    
    ERROR --> READY : reset_device()
    ERROR --> SHUTDOWN : critical_error
    
    READY --> SHUTDOWN : shutdown()
    RUNNING --> SHUTDOWN : shutdown()
    SLEEPING --> SHUTDOWN : shutdown()
    
    SHUTDOWN --> [*]
```

### LLM Inference States

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> MODEL_LOADING : CMD_LOAD_MODEL
    MODEL_LOADING --> READY : model_loaded
    MODEL_LOADING --> ERROR : load_failed
    
    READY --> BUSY : CMD_START_INF
    BUSY --> BUSY : generating_tokens
    BUSY --> EOG : completion_reached
    BUSY --> ERROR : inference_error
    BUSY --> STOPPED : CMD_SOFT_STOP
    
    EOG --> READY : clear_status
    STOPPED --> READY : clear_status
    ERROR --> IDLE : CMD_RESET
    READY --> IDLE : CMD_RESET
```

## Performance Characteristics

### Virtual Hardware Performance

| Component | Metric | Value |
|-----------|--------|-------|
| GPIO Pins | Pin operation latency | < 1 µs |
| Memory Access | Read/Write latency | ~10 ns |
| DMA Transfer | Simulated bandwidth | 1 GB/s |
| Interrupt | Latency | < 1 µs |
| Device Update | Cycle time | 1 ms (configurable) |

### LLM Coprocessor Performance

| Metric | Description | Notes |
|--------|-------------|-------|
| Tokenization | Word-based (mock) or subword (llama.cpp) | Depends on implementation |
| Generation Speed | Model and hardware dependent | Real-time tracking via REG_PERF_TOKENS_SEC |
| Telemetry Accuracy | Timestamp-based measurement | High precision |
| Status Tracking | Hardware state machine | IDLE→BUSY→EOG transitions |
| Token Counting | Precise token-level accounting | Per-inference and cumulative |

## GPIO Pin Allocation

### Financial Account Mapping

```mermaid
graph TB
    subgraph "Assets (GPIO 0-15)"
        GPIO0[GPIO0: Cash 1101]
        GPIO1[GPIO1: A/R 1102]
        GPIO2[GPIO2: Inventory 1103]
        GPIO3[GPIO3: Equipment 1201]
        GPIO4[GPIO4: Buildings 1202]
    end
    
    subgraph "Liabilities (GPIO 16-31)"
        GPIO16[GPIO16: A/P 2101]
        GPIO17[GPIO17: ST Loans 2102]
        GPIO18[GPIO18: LT Loans 2201]
    end
    
    subgraph "Equity (GPIO 32-39)"
        GPIO32[GPIO32: Owner's Equity 3100]
        GPIO33[GPIO33: Retained Earnings 3200]
    end
    
    subgraph "Revenue (GPIO 40-47)"
        GPIO40[GPIO40: Sales 4100]
        GPIO41[GPIO41: Services 4200]
    end
    
    subgraph "Expenses (GPIO 48-63)"
        GPIO48[GPIO48: Salaries 5101]
        GPIO49[GPIO49: Rent 5102]
        GPIO50[GPIO50: Utilities 5103]
    end
    
    style GPIO0 fill:#7ed321
    style GPIO16 fill:#f5a623
    style GPIO32 fill:#4a90e2
    style GPIO40 fill:#50e3c2
    style GPIO48 fill:#bd10e0
```

### Balance-to-Voltage Conversion

Account balances are mapped to analog voltages using a logarithmic scale:
- **Voltage Range**: 0V - 3.3V
- **ADC Resolution**: 12-bit (0-4095)
- **Mapping Formula**: `voltage = log10(balance + 1) / 6.0 * 3.3`
- **Example**: $50,000 → log₁₀(50001) ≈ 4.70 → 2.59V

## Technology Stack

### Core Technologies

| Category | Technology | Version | Purpose |
|----------|-----------|---------|---------|
| Language | C++ | C++17 | Core implementation |
| Build System | CMake | 3.10+ | Build configuration |
| Threading | pthread | POSIX | Concurrency support |
| LLM Runtime | llama.cpp | Latest | GGUF model inference |
| Model Format | GGUF | - | Model file format |

### Build Targets

| Target | Description | Dependencies |
|--------|-------------|--------------|
| llama-device-admin | Interactive admin CLI | Virtual device, both drivers |
| llama-financial-sim | Financial simulation demo | llama.cpp, common |
| test-virtual-device | Virtual device tests | Virtual device, both drivers |
| test-financial-logic | Financial logic tests | - |
| test-gguf-integration | GGUF integration tests | Virtual device, LLM driver |
| demo-virtual-device | Simple device demo | Virtual device, financial driver |

## Design Patterns

### Hardware Abstraction Pattern

The NPU uses a layered hardware abstraction:
1. **Physical Layer**: VirtualPCB simulates physical hardware
2. **Register Layer**: Memory-mapped I/O for device control
3. **Driver Layer**: Device-specific drivers (Financial, LLM)
4. **API Layer**: High-level convenience APIs for applications

### Command-Status Pattern

All hardware operations follow a command-status pattern:
1. Write command to REG_CMD
2. Monitor REG_STATUS for completion
3. Check REG_ERROR_CODE for errors
4. Read results from data registers

### Telemetry Pattern

Performance metrics are collected at multiple levels:
1. **Hardware Registers**: REG_PERF_TOKENS_SEC, REG_CTX_USED
2. **Driver Telemetry**: LlamaTelemetry struct with detailed metrics
3. **System Telemetry**: TelemetrySystem with metric aggregation

## Security Considerations

### Memory Protection

- Memory regions have configurable write protection
- FLASH region is typically read-only after firmware load
- PERIPH region has register-level access control

### Error Handling

- Hardware error codes in REG_ERROR_CODE
- Status flags indicate error conditions
- Graceful degradation on failures
- Diagnostic output for troubleshooting

### Resource Limits

- DMA limited to 8 concurrent channels
- Interrupt vectors limited to 256
- GPIO pins limited to 64
- Memory regions have fixed sizes

## Future Extensions

### Planned Enhancements

1. **Real llama.cpp Integration**: Replace mock with actual library
2. **KV-Cache Management**: Map KV-cache to SRAM for persistence
3. **GPU Offloading**: Add GPU layer control via registers
4. **Interrupt Support**: Implement TOKEN_READY interrupt handling
5. **Model Hot-Swapping**: Dynamic model loading/unloading
6. **Batch Inference**: Process multiple prompts simultaneously
7. **LoRA Adapters**: Support fine-tuned model loading

### Extensibility Points

- **Custom Device Drivers**: Inherit from DeviceDriver base class
- **Custom I/O Streams**: Implement IOStream interface
- **Custom Memory Regions**: Add new memory regions to VirtualPCB
- **Custom Interrupts**: Register new interrupt handlers
- **Custom Telemetry**: Add metrics to TelemetrySystem

## References

### Documentation

- [README.md](../README.md) - Main project documentation
- [IMPLEMENTATION.md](../IMPLEMENTATION.md) - Implementation summary
- [fhp/VIRTUAL_DEVICE_README.md](../fhp/VIRTUAL_DEVICE_README.md) - Virtual device guide
- [fhp/GGUF_INTEGRATION.md](../fhp/GGUF_INTEGRATION.md) - GGUF integration guide

### Source Code

- [virtual-device.h](../fhp/virtual-device.h) - Virtual PCB header
- [virtual-device.cpp](../fhp/virtual-device.cpp) - Virtual PCB implementation
- [llama-coprocessor-driver.h](../fhp/llama-coprocessor-driver.h) - LLM driver header
- [llama-coprocessor-driver.cpp](../fhp/llama-coprocessor-driver.cpp) - LLM driver implementation
- [financial-device-driver.h](../fhp/financial-device-driver.h) - Financial driver header
- [financial-device-driver.cpp](../fhp/financial-device-driver.cpp) - Financial driver implementation

---

**Document Version**: 1.0  
**Last Updated**: 2025-12-03  
**Repository**: https://github.com/o9nn/npu
