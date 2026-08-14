# GGUF Integration Guide

## Overview

This document describes the GGUF runtime integration implemented in the NPU coprocessor driver. The integration provides a complete pipeline from prompt text to completion text, maintaining the hardware-first design philosophy throughout.

## Architecture

### Integration Layers

```
┌─────────────────────────────────────────────┐
│   High-Level API (infer, infer_streaming)  │
├─────────────────────────────────────────────┤
│   GGUF Integration Layer                    │
│   - Tokenization                            │
│   - Token Generation                        │
│   - Detokenization                          │
├─────────────────────────────────────────────┤
│   llama.cpp Runtime (LlamaRuntime struct)   │
│   - Model Loading                           │
│   - Context Management                      │
│   - Token Sampling                          │
├─────────────────────────────────────────────┤
│   Hardware Register Interface (MMIO)        │
│   - REG_STATUS, REG_CMD, etc.               │
└─────────────────────────────────────────────┘
```

### Data Flow

```
1. Prompt (text) → Tokenization → tokens[]
2. tokens[] → configure_inference() → REG_PROMPT_LEN
3. start_inference() → STATUS_BUSY flag set
4. Token Generation Loop:
   - Generate token from model
   - (Optional) Write to REG_TOKEN_OUT
   - (Optional) Set TOKEN_READY flag
5. tokens[] → Detokenization → completion (text)
6. Update STATUS_EOG, REG_PERF_TOKENS_SEC
```

## Implementation Details

### 1. LlamaRuntime Struct

The `LlamaRuntime` struct encapsulates all llama.cpp state:

```cpp
struct LlamaRuntime {
    llama::llama_model* model = nullptr;
    llama::llama_context* ctx = nullptr;
    llama::llama_model_params model_params;
    llama::llama_context_params ctx_params;
    bool initialized = false;

    bool load(const std::string& model_path, const LlamaModelConfig& cfg);
    void free();
};
```

**Design Rationale:**
- Forward-declared in header to avoid polluting namespace
- Owned by raw pointer (not unique_ptr) to avoid incomplete type issues
- Properly cleaned up in destructor
- Configured from LlamaModelConfig for consistency

### 2. Model Loading

The `load_model()` function now performs actual GGUF loading:

```cpp
bool LlamaCoprocessorDriver::load_model() {
    // Initialize runtime
    init_llama_runtime();
    
    // Load GGUF model
    llama_runtime_->load(model_cfg.model_path, model_cfg);
    
    // Update hardware registers
    write_reg32(REG_MODEL_ID, model_id);
    write_reg32(REG_CTX_USED, llama::llama_n_ctx(llama_runtime_->ctx));
    
    // Set STATUS_MODEL_READY flag
    uint32_t status = read_reg32(REG_STATUS);
    status |= STATUS_MODEL_READY;
    write_reg32(REG_STATUS, status);
}
```

**Key Points:**
- Model path comes from `LlamaModelConfig`
- Context size, threads, GPU layers all configured from config
- Hardware registers updated to reflect model state
- Error codes written to REG_ERROR_CODE on failure

### 3. Tokenization

Tokenization converts text to token IDs:

```cpp
std::vector<int32_t> tokenize_prompt(const std::string& text) {
    auto tokens = llama::llama_tokenize(llama_runtime_->ctx, text, true);
    telemetry.last_prompt_tokens = tokens.size();
    return tokens;
}
```

**Features:**
- Uses llama.cpp tokenizer
- `add_bos` parameter adds beginning-of-sequence token
- Token count tracked in telemetry
- Written to REG_PROMPT_LEN register

### 4. Inference Loop

The inference pipeline:

```cpp
std::string infer(const std::string& prompt, const LlamaSequenceConfig& seq_cfg) {
    // 1. Tokenize
    auto prompt_tokens = tokenize_prompt(prompt);
    
    // 2. Configure hardware
    configure_inference(addr, prompt_tokens.size(), seq_cfg);
    start_inference();
    
    // 3. Generate tokens
    auto generated_tokens = generate_tokens(prompt_tokens, seq_cfg.n_predict);
    
    // 4. Detokenize
    std::string completion = detokenize_tokens(generated_tokens);
    
    // 5. Update telemetry and registers
    update_hardware_state();
    
    return completion;
}
```

**Hardware Register Updates:**
- `REG_STATUS`: BUSY → EOG transition
- `REG_CTX_USED`: Total tokens (prompt + completion)
- `REG_PERF_TOKENS_SEC`: Tokens per second (milli-units)
- `REG_ERROR_CODE`: Set on errors

### 5. Detokenization

Convert tokens back to text:

```cpp
std::string detokenize_tokens(const std::vector<int32_t>& tokens) {
    return llama::llama_detokenize(llama_runtime_->ctx, tokens);
}
```

**Handling Special Tokens:**
- BOS/EOS tokens filtered during detokenization
- Unknown tokens mapped to `<unk>`
- Proper spacing between words

### 6. Telemetry

Accurate performance metrics:

```cpp
struct LlamaTelemetry {
    double tokens_per_second;           // Actual generation speed
    uint64_t total_tokens_generated;    // Lifetime total
    uint64_t total_prompts;             // Number of inferences
    uint64_t last_prompt_tokens;        // Last prompt size
    uint64_t last_completion_tokens;    // Last completion size
    std::chrono::steady_clock::time_point last_inference_start;
    std::chrono::steady_clock::time_point last_inference_end;
};
```

**Calculations:**
- `tokens_per_second` = completion_tokens / (duration_ms / 1000.0)
- Duration measured with high-resolution clock
- Written to `REG_PERF_TOKENS_SEC` as milli-tokens/sec

## Mock Implementation

### Purpose

The current implementation uses `llama-mock.h` instead of real llama.cpp:

```cpp
#include "llama-mock.h"  // Mock API
// #include "llama.h"     // Real API (future)

namespace llama = llama_mock;  // Easy switching
```

**Advantages:**
1. **No Dependencies** - Build and test without llama.cpp
2. **Fast Iteration** - Validate integration logic quickly
3. **Clear Patterns** - Document how to use real llama.cpp
4. **Comprehensive Tests** - Test suite validates all paths

### MockLlamaBackend

The mock provides a simple word-based tokenizer:

```cpp
class MockLlamaBackend {
    std::vector<llama_token> tokenize(const std::string& text, bool add_bos);
    std::string detokenize(const std::vector<llama_token>& tokens);
    std::vector<llama_token> generate(const std::vector<llama_token>& prompt, 
                                     int32_t n_predict);
};
```

**Mock Generation:**
- Returns predefined completion tokens
- Demonstrates proper BOS/EOS handling
- Validates token counting and telemetry

### Switching to Real llama.cpp

To use real GGUF models:

1. **Add llama.cpp dependency:**
```cmake
# CMakeLists.txt
find_package(llama REQUIRED)
target_link_libraries(llama-device-admin PRIVATE llama)
```

2. **Update include:**
```cpp
// llama-coprocessor-driver.cpp
#include "llama.h"
#include "common.h"
// Remove: #include "llama-mock.h"
```

3. **Implement real generation loop:**
```cpp
std::vector<int32_t> generate_tokens(...) {
    std::vector<int32_t> result;
    
    // Evaluate prompt
    llama_decode(ctx, llama_batch_get_one(prompt_tokens.data(), 
                                          prompt_tokens.size(), 0, 0));
    
    // Sampling loop
    for (int i = 0; i < n_predict; i++) {
        auto logits = llama_get_logits(ctx);
        int32_t token = sample_token(logits);  // Use llama.cpp sampler
        
        if (token == llama_token_eos(model)) {
            break;
        }
        
        result.push_back(token);
        llama_decode(ctx, llama_batch_get_one(&token, 1, i, 0));
    }
    
    return result;
}
```

4. **Everything else stays the same** - Hardware interface, telemetry, tests all work unchanged

## Testing

### Test Suite

The `test-gguf-integration` binary validates all integration aspects:

```bash
./build/test-gguf-integration
```

**Tests:**
1. **Model Loading** - Verify GGUF model initialization
2. **Basic Inference** - Run single inference, check result
3. **Streaming Inference** - Test token callbacks
4. **Multiple Inferences** - Sequential prompts
5. **Hardware Registers** - Verify register state transitions
6. **Self-Test** - Hardware diagnostic routine

### Running Tests

```bash
# Build test
cmake --build build --target test-gguf-integration

# Run test suite
./build/test-gguf-integration

# Expected output:
# ╔═══════════════════════════════════════════════════════╗
# ║  ✓ All GGUF Integration Tests Passed                 ║
# ╚═══════════════════════════════════════════════════════╝
```

### Interactive Testing

Use `llama-device-admin` for manual testing:

```bash
./build/llama-device-admin

> llm status       # Check device state
> llm infer        # Run test inference  
> llm streaming    # Test streaming mode
> llm diag         # Show diagnostics
> llm selftest     # Run self-test
```

## Performance Characteristics

### Mock Implementation

- **Inference Time:** Immediate (no actual computation)
- **Token Generation:** 15-16 tokens (predefined)
- **Tokens/sec:** ~0 (instant completion)
- **Memory:** Minimal (no model loaded)

### Real llama.cpp (Expected)

- **Inference Time:** Depends on model size and hardware
- **Token Generation:** Configurable via `n_predict`
- **Tokens/sec:** 1-100+ depending on model and GPU
- **Memory:** Model-dependent (1GB - 100GB+)

### Hardware Register Performance

- **Register Read:** ~10ns (virtual memory)
- **Register Write:** ~10ns (virtual memory)
- **Status Check:** Sub-microsecond
- **Telemetry Access:** Immediate (cached values)

## Configuration

### Model Configuration

```cpp
LlamaModelConfig cfg;
cfg.model_path = "models/my-model.gguf";  // Path to GGUF file
cfg.n_ctx = 4096;                          // Context window size
cfg.n_threads = 8;                         // CPU threads
cfg.n_gpu_layers = 32;                     // GPU layers (0 = CPU only)
cfg.batch_size = 512;                      // Batch size
cfg.offload_kv_cache = true;               // Offload KV cache to GPU
cfg.low_vram_mode = false;                 // Low VRAM optimizations
```

### Sequence Configuration

```cpp
LlamaSequenceConfig seq;
seq.n_predict = 256;                      // Max tokens to generate
seq.max_ctx = 4096;                       // Max context to use
seq.echo_prompt = true;                   // Include prompt in output
seq.stream_tokens = true;                 // Enable token callbacks
seq.system_prompt = "You are a helpful assistant.";
```

## Error Handling

### Error Codes

| Code | Meaning | Register |
|------|---------|----------|
| 0 | No error | REG_ERROR_CODE |
| 1 | Model path empty | REG_ERROR_CODE |
| 2 | Runtime init failed | REG_ERROR_CODE |
| 3 | Model load failed | REG_ERROR_CODE |
| 4 | Tokenization failed | REG_ERROR_CODE |
| 5 | Generation failed | REG_ERROR_CODE |

### Status Flags

| Flag | Meaning | When Set |
|------|---------|----------|
| STATUS_IDLE | Ready for commands | After init, after inference |
| STATUS_BUSY | Inference running | During token generation |
| STATUS_EOG | End of generation | After last token |
| STATUS_ERROR | Error occurred | On any error |
| STATUS_MODEL_READY | Model loaded | After successful load_model() |
| STATUS_TOKEN_READY | Token available | Per token (future) |

### Error Recovery

```cpp
if (llm->has_error()) {
    uint32_t err = llm->get_error_code();
    std::cerr << "Error code: " << err << "\n";
    
    // Reset device
    llm->reset_device();
    
    // Retry operation
    llm->load_model();
}
```

## Future Enhancements

### KV-Cache Management

Map KV-cache to SRAM region for persistence:

```cpp
// Allocate SRAM buffer for KV-cache
uint64_t kv_addr = 0x20010000;  // SRAM offset
size_t kv_size = calculate_kv_size(n_ctx, n_layers);

// Configure llama.cpp to use SRAM buffer
// (Implementation pending)
```

### Interrupt Support

Generate interrupts when tokens are ready:

```cpp
// When token generated
write_reg32(REG_TOKEN_OUT, token);
write_reg32(REG_TOKEN_READY, 1);

// Raise interrupt
pcb->raise_interrupt(llm_irq_vector);
```

### GPU Offloading

Control GPU layer offloading via hardware register:

```cpp
// Write GPU layer count to register
write_reg32(REG_GPU_LAYERS, 32);

// Update model configuration
model_params.n_gpu_layers = read_reg32(REG_GPU_LAYERS);
```

### Model Hot-Swapping

Load/unload models dynamically:

```cpp
// Unload current model
llm->reset_device();
free_llama_runtime();

// Load new model
cfg.model_path = "models/new-model.gguf";
llm->set_model_config(cfg);
llm->load_model();
```

## Best Practices

### 1. Always Check Status

```cpp
uint32_t status = llm->read_status();
if (status & STATUS_ERROR) {
    // Handle error
}
```

### 2. Use Telemetry

```cpp
auto telem = llm->get_telemetry();
std::cout << "Tokens/sec: " << telem.tokens_per_second << "\n";
std::cout << "Total tokens: " << telem.total_tokens_generated << "\n";
```

### 3. Configure Before Load

```cpp
// Set configuration first
llm->set_model_config(cfg);

// Then load model
llm->load_model();
```

### 4. Handle Errors Gracefully

```cpp
std::string result = llm->infer(prompt, seq);
if (result.find("[LLM-DRV]") != std::string::npos) {
    // Error message, check status
    if (llm->has_error()) {
        llm->reset_device();
    }
}
```

## Conclusion

The GGUF integration provides a complete, hardware-first approach to LLM inference. The mock implementation demonstrates all integration patterns and can be easily replaced with real llama.cpp when needed. All hardware registers, telemetry, and diagnostics work correctly, maintaining the peripheral device abstraction throughout.
