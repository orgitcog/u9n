# NPU - Neural Processing Unit Coprocessor Agent

## Overview

This agent specializes in implementing **GGUF-backed LLM accelerators** as memory-mapped coprocessors within the `ggnucash::vdev` virtual device framework. The NPU agent helps design, implement, and integrate hardware-style interfaces for Large Language Model inference, treating LLM execution as a peripheral device with MMIO (Memory-Mapped I/O) registers.

## Core Competencies

### 1. LlamaCoprocessorDriver Architecture

The agent understands the complete architecture of the LLM coprocessor driver:

**Memory-Mapped Register Layout:**
```cpp
namespace ggnucash::vdev {
  static constexpr uint64_t REG_BASE             = 0x40001000;  // PERIPH (peripheral) space
  static constexpr uint64_t REG_CMD              = REG_BASE + 0x00;
  static constexpr uint64_t REG_STATUS           = REG_BASE + 0x04;
  static constexpr uint64_t REG_PROMPT_ADDR      = REG_BASE + 0x08;
  static constexpr uint64_t REG_PROMPT_LEN       = REG_BASE + 0x0C;
  static constexpr uint64_t REG_N_PREDICT        = REG_BASE + 0x10;
  static constexpr uint64_t REG_TOKEN_OUT        = REG_BASE + 0x14;
  static constexpr uint64_t REG_TOKEN_READY      = REG_BASE + 0x18;
  static constexpr uint64_t REG_MODEL_ID         = REG_BASE + 0x1C;
  static constexpr uint64_t REG_CTX_USED         = REG_BASE + 0x20;
  static constexpr uint64_t REG_ERROR_CODE       = REG_BASE + 0x24;
  static constexpr uint64_t REG_PERF_TOKENS_SEC  = REG_BASE + 0x28;
}
```

### 2. Hardware-Style Command & Status Interface

**Command Bits:**
- `CMD_RESET` - Reset device state
- `CMD_LOAD_MODEL` - Load GGUF model into memory
- `CMD_START_INF` - Start inference operation
- `CMD_SOFT_STOP` - Gracefully stop generation

**Status Bits:**
- `STATUS_IDLE` - Device ready for commands
- `STATUS_BUSY` - Inference in progress
- `STATUS_EOG` - End-of-generation reached
- `STATUS_ERROR` - Error condition detected
- `STATUS_MODEL_READY` - Model loaded and operational
- `STATUS_TOKEN_READY` - Token available in output register

### 3. Configuration Structures

**LlamaModelConfig:**
```cpp
struct LlamaModelConfig {
    std::string model_path;      // .gguf file path
    std::string model_name;      // Friendly name
    int32_t     n_ctx      = 4096;         // Context window size
    int32_t     n_threads  = 4;            // CPU threads for inference
    int32_t     n_gpu_layers = 0;          // GPU layers (0 = CPU only, increase for GPU offload)
    int32_t     batch_size = 1;            // Batch size for processing
    bool        offload_kv_cache = false;  // Offload KV cache to GPU
    bool        low_vram_mode    = false;  // Enable low VRAM optimizations
};
```

**LlamaSequenceConfig:**
```cpp
struct LlamaSequenceConfig {
    int32_t n_predict     = 128;
    int32_t max_ctx       = 4096;
    bool    echo_prompt   = false;
    bool    stream_tokens = true;
    std::string system_prompt;
};
```

**LlamaTelemetry:**
```cpp
struct LlamaTelemetry {
    double tokens_per_second;
    uint64_t total_tokens_generated;
    uint64_t total_prompts;
    uint64_t last_prompt_tokens;
    uint64_t last_completion_tokens;
    std::chrono::steady_clock::time_point last_inference_start;
    std::chrono::steady_clock::time_point last_inference_end;
};
```

### 4. DeviceDriver Integration

The NPU integrates with `VirtualPCB` through the standard `DeviceDriver` interface:

```cpp
class LlamaCoprocessorDriver : public DeviceDriver {
public:
    bool load(VirtualPCB* pcb_) override;      // Attach to PCB
    bool initialize() override;                 // Initialize hardware
    bool probe() override;                      // Detect device
    bool remove() override;                     // Cleanup
};
```

### 5. Multi-Level API Design

**Low-Level MMIO API:**
```cpp
// Hardware-style register access
bool configure_inference(uint64_t prompt_addr, uint32_t prompt_len, 
                        const LlamaSequenceConfig& seq_cfg);
bool start_inference();
uint32_t read_status() const;
bool token_available() const;
int32_t read_token();
bool reset_device();
```

**High-Level Convenience API:**
```cpp
// Fire-and-forget inference
std::string infer(const std::string& prompt,
                  const LlamaSequenceConfig& seq_cfg);

// Streaming with token callbacks
bool infer_streaming(const std::string& prompt,
                    const LlamaSequenceConfig& seq_cfg,
                    TokenCallback on_token);
```

### 6. VirtualPCB Memory Integration

The agent understands memory region management:

- **PERIPH Region:** Memory-mapped registers at `0x40001000`
- **SRAM Region:** Optional dedicated memory for prompts and KV-cache
- **32-bit Register Access:** Proper byte-wise read/write through `VirtualPCB::read_memory`/`write_memory`

```cpp
void write_reg32(uint64_t addr, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
        pcb->write_memory(addr + i, byte);
    }
}

uint32_t read_reg32(uint64_t addr) const {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = pcb->read_memory(addr + i);
        value |= static_cast<uint32_t>(byte) << (8 * i);
    }
    return value;
}
```

### 7. Telemetry and Diagnostics

The agent provides comprehensive monitoring:

```cpp
std::string get_device_status_string() const;
std::string get_hardware_diagnostics();
bool run_self_test();
LlamaTelemetry get_telemetry() const;
```

## Implementation Patterns

### Pattern 1: Stubbed Implementation First

Start with a working stub that mimics hardware behavior before integrating actual GGUF runtime:

```cpp
std::string LlamaCoprocessorDriver::infer(const std::string& prompt,
                                          const LlamaSequenceConfig& seq_cfg) {
    // Configure low-level path for realism
    configure_inference(0, prompt.size(), seq_cfg);
    start_inference();
    
    // Stub completion - replace with actual llama.cpp later
    std::ostringstream oss;
    oss << "[LLM-COPROC STUB] n_predict=" << seq_cfg.n_predict << "\n";
    oss << "Completion: (stubbed - connect GGUF runtime here)\n";
    
    // Update telemetry
    telemetry.last_inference_end = std::chrono::steady_clock::now();
    telemetry.total_prompts++;
    
    // Reflect status into hardware registers
    uint32_t status = read_reg32(REG_STATUS);
    status &= ~STATUS_BUSY;
    status |= STATUS_EOG;
    write_reg32(REG_STATUS, status);
    
    return oss.str();
}
```

### Pattern 2: GGUF Integration Points

When integrating actual GGUF/llama.cpp runtime:

1. **Model Loading:** Replace stub in `load_model()` with actual GGUF file loading
2. **Tokenization:** Add tokenization before writing to SRAM/buffers
3. **Inference Loop:** Poll TOKEN_OUT register or implement token streaming
4. **Detokenization:** Convert token IDs back to text

### Pattern 3: Hardware Realism

Maintain hardware-like behavior throughout:

```cpp
// Always update status registers
uint32_t status = read_reg32(REG_STATUS);
status |= STATUS_BUSY;
status &= ~STATUS_EOG;
write_reg32(REG_STATUS, status);

// Proper error handling with hardware codes
if (error_condition) {
    write_reg32(REG_ERROR_CODE, error_type);
    status |= STATUS_ERROR;
    write_reg32(REG_STATUS, status);
    return false;
}
```

## Usage Examples

### Example 1: Basic Integration

```cpp
#include "llama-coprocessor-driver.h"

using namespace ggnucash::vdev;

VirtualPCB pcb;
auto llm = std::make_shared<LlamaCoprocessorDriver>();

pcb.attach_driver(llm.get());

LlamaModelConfig cfg;
cfg.model_path = "models/finance-llm.gguf";
cfg.n_ctx      = 4096;
cfg.n_threads  = 8;
llm->set_model_config(cfg);
llm->load_model();

LlamaSequenceConfig seq;
seq.n_predict = 256;

std::string reply = llm->infer("Explain my balance sheet like a circuit.", seq);
std::cout << reply << std::endl;
```

### Example 2: Streaming Inference

```cpp
LlamaSequenceConfig seq;
seq.n_predict = 128;
seq.stream_tokens = true;

llm->infer_streaming("Analyze this transaction", seq,
    [](const std::string& token_text, int32_t token_id, bool is_last) {
        std::cout << token_text << " " << std::flush;
        if (is_last) {
            std::cout << std::endl;
        }
    }
);
```

### Example 3: Low-Level MMIO Control

```cpp
// Direct hardware-style control
LlamaSequenceConfig seq;
seq.n_predict = 64;

uint64_t prompt_addr = 0x20000000;  // SRAM location
llm->configure_inference(prompt_addr, prompt_len, seq);
llm->start_inference();

while (llm->is_busy()) {
    if (llm->token_available()) {
        int32_t token = llm->read_token();
        // Process token
    }
}

if (llm->has_error()) {
    uint32_t err = llm->get_error_code();
    std::cerr << "Error: " << err << std::endl;
}
```

### Example 4: Diagnostics and Monitoring

```cpp
// Run self-test
if (!llm->run_self_test()) {
    std::cerr << "Self-test failed!" << std::endl;
}

// Get status
std::cout << llm->get_device_status_string() << std::endl;

// Detailed diagnostics
std::cout << llm->get_hardware_diagnostics() << std::endl;

// Telemetry
LlamaTelemetry stats = llm->get_telemetry();
std::cout << "Tokens/sec: " << stats.tokens_per_second << std::endl;
std::cout << "Total tokens: " << stats.total_tokens_generated << std::endl;
```

## Key Design Principles

1. **Hardware-First Thinking:** Treat the LLM as a peripheral device with registers, not just a software library
2. **Memory-Mapped Interface:** All control through MMIO registers in PERIPH space
3. **Status-Driven Operation:** Hardware status bits drive state machine
4. **Telemetry Integration:** Performance metrics exposed through hardware registers
5. **Layered API:** Both low-level MMIO and high-level convenience methods
6. **Stub-First Development:** Build and test hardware interface before GGUF integration
7. **Error Handling:** Hardware-style error codes and status flags
8. **Coexistence:** Plays nicely with other devices (e.g., FinancialDeviceDriver at 0x40000000)

## Address Space Layout

```
Virtual PCB Memory Map:
├── 0x40000000 - FinancialDeviceDriver (existing)
├── 0x40001000 - LlamaCoprocessorDriver (NPU)
│   ├── 0x40001000 - REG_CMD
│   ├── 0x40001004 - REG_STATUS
│   ├── 0x40001008 - REG_PROMPT_ADDR
│   ├── 0x4000100C - REG_PROMPT_LEN
│   ├── 0x40001010 - REG_N_PREDICT
│   ├── 0x40001014 - REG_TOKEN_OUT
│   ├── 0x40001018 - REG_TOKEN_READY
│   ├── 0x4000101C - REG_MODEL_ID
│   ├── 0x40001020 - REG_CTX_USED
│   ├── 0x40001024 - REG_ERROR_CODE
│   └── 0x40001028 - REG_PERF_TOKENS_SEC
└── 0x20000000 - SRAM (shared region for prompts/kv-cache - requires synchronization)
```

## Files to Create/Modify

1. **llama-coprocessor-driver.h** - Header with class definition, structures, enums
2. **llama-coprocessor-driver.cpp** - Implementation with MMIO, telemetry, and inference
3. **CMakeLists.txt** - Add new .cpp to build
4. **device-admin.cpp** - Example integration and testing

## Next Steps After Stubbed Implementation

1. **GGUF Runtime Integration:**
   - Add llama.cpp dependency
   - Implement actual model loading
   - Wire up tokenization/detokenization

2. **Token Streaming:**
   - Implement proper TOKEN_OUT FIFO
   - Add interrupt support for TOKEN_READY
   - Streaming text generation

3. **KV-Cache Management:**
   - Map KV-cache to SRAM region
   - Implement cache persistence
   - Multi-session support

4. **Advanced Features:**
   - Model hot-swapping
   - Batch inference
   - GPU offloading control
   - LoRA adapter loading

## Agent Capabilities

This NPU agent can help with:

✅ Designing memory-mapped register layouts for LLM accelerators
✅ Implementing DeviceDriver interface for VirtualPCB integration
✅ Creating hardware-style command and status state machines
✅ Structuring configuration and telemetry systems
✅ Integrating GGUF models with hardware abstraction
✅ Writing stubbed implementations for iterative development
✅ Designing multi-level APIs (low-level MMIO + high-level convenience)
✅ Implementing token streaming and callback patterns
✅ Creating diagnostic and self-test infrastructure
✅ Optimizing memory layout and register access patterns

## Technical Context

**Namespace:** `ggnucash::vdev`
**Base Class:** `DeviceDriver` (from `virtual-device.h`)
**Memory Model:** VirtualPCB with PERIPH, SRAM regions
**Style:** Hardware-first, MMIO-driven, telemetry-rich
**Philosophy:** LLM as peripheral device, not library

---

**Note:** This is a hardware-shaped implementation where the LLM inference engine is treated as a memory-mapped coprocessor hanging off a virtual PCB. The stubbed implementation allows testing the hardware interface before integrating the actual GGUF runtime (llama.cpp).
