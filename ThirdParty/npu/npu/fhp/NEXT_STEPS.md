# Next Steps Implementation Guide

## Overview

This document describes the production-ready "Next Steps" features implemented for the NPU (Neural Processing Unit) coprocessor. These features enhance the hardware-first design philosophy and enable advanced use cases for LLM inference.

## Implemented Features

### 1. KV-Cache Management

Map KV-cache to SRAM region for persistence across inferences.

#### Hardware Registers

```
REG_KV_CACHE_ADDR  (0x40001030)  - SRAM address of KV-cache
REG_KV_CACHE_SIZE  (0x40001034)  - Size of KV-cache in bytes
```

#### Command Bits

```
CMD_ALLOC_KVCACHE  (1u << 5)     - Allocate KV-cache command
```

#### Status Flags

```
STATUS_KV_CACHED   (1u << 5)     - KV-cache is allocated
```

#### API

```cpp
// Enable KV-cache (auto-calculates size)
bool enable_kv_cache();

// Disable and free KV-cache
bool disable_kv_cache();

// Query cache state
bool is_kv_cache_enabled() const;
size_t get_kv_cache_size() const;
uint64_t get_kv_cache_addr() const;
```

#### Usage Example

```cpp
auto llm = std::make_shared<LlamaCoprocessorDriver>();
// ... initialize and load model ...

// Enable KV-cache
llm->enable_kv_cache();

// Run inference - cache is preserved
std::string result1 = llm->infer("First prompt", seq_cfg);
std::string result2 = llm->infer("Second prompt", seq_cfg);  // Uses cached context

// Check cache status
std::cout << "Cache size: " << llm->get_kv_cache_size() << " bytes\n";
std::cout << "Cache addr: 0x" << std::hex << llm->get_kv_cache_addr() << "\n";

// Disable when done
llm->disable_kv_cache();
```

#### Implementation Details

- KV-cache is allocated in SRAM region at offset `0x20010000`
- Size is calculated based on context window: `n_ctx * 1024` bytes
- Cache is automatically freed when model is unloaded
- Hardware register `REG_KV_CACHE_ADDR` stores the SRAM address
- Hardware register `REG_KV_CACHE_SIZE` stores the allocated size
- `STATUS_KV_CACHED` flag indicates active cache

### 2. GPU Offloading Control

Dynamic control of GPU layer offloading via hardware registers.

#### Hardware Registers

```
REG_GPU_LAYERS     (0x4000102C)  - Number of layers to offload to GPU
```

#### Status Flags

```
STATUS_GPU_ACTIVE  (1u << 6)     - GPU offloading is active
```

#### API

```cpp
// Set number of GPU layers (0 = CPU only)
bool set_gpu_layers(int32_t n_layers);

// Query GPU configuration
int32_t get_gpu_layers() const;
bool is_gpu_active() const;
```

#### Usage Example

```cpp
auto llm = std::make_shared<LlamaCoprocessorDriver>();
// ... initialize and load model ...

// Enable GPU offloading for 32 layers
llm->set_gpu_layers(32);

// Check GPU status
if (llm->is_gpu_active()) {
    std::cout << "GPU active with " << llm->get_gpu_layers() << " layers\n";
}

// Run inference with GPU acceleration
std::string result = llm->infer("GPU-accelerated prompt", seq_cfg);

// Disable GPU
llm->set_gpu_layers(0);
```

#### Implementation Details

- GPU layer count stored in `REG_GPU_LAYERS` register
- `STATUS_GPU_ACTIVE` flag set when `n_layers > 0`
- Changes take effect on next model load
- Compatible with existing model configuration
- Works with mock implementation (no real GPU required)

### 3. Interrupt Support

TOKEN_READY interrupt handling via VirtualPCB interrupt controller.

#### API

```cpp
// Enable interrupts on specified vector
bool enable_token_interrupts(int vector_index);

// Disable interrupts
bool disable_token_interrupts();

// Query interrupt state
bool are_interrupts_enabled() const;
```

#### Usage Example

```cpp
auto llm = std::make_shared<LlamaCoprocessorDriver>();
// ... initialize and load model ...

// Enable token-ready interrupts on vector 10
llm->enable_token_interrupts(10);

// Register interrupt handler with VirtualPCB
auto irq_ctrl = device->get_interrupt_controller();
irq_ctrl->register_interrupt(10, InterruptType::LEVEL_TRIGGERED, 5);

// Run inference - interrupts will fire when tokens are ready
llm->infer("Prompt with interrupts", seq_cfg);

// Disable interrupts
llm->disable_token_interrupts();
```

#### Implementation Details

- Integrates with VirtualPCB `InterruptController`
- Uses `trigger_interrupt()` to raise interrupts
- `STATUS_TOKEN_READY` flag set when token available
- Configurable interrupt vector (default: -1, disabled)
- Hardware-style interrupt handling model

### 4. Model Hot-Swapping

Dynamic model loading and unloading without device restart.

#### Command Bits

```
CMD_UNLOAD_MODEL   (1u << 4)     - Unload current model
```

#### API

```cpp
// Unload current model
bool unload_model();

// Switch to new model (unload + load)
bool switch_model(const LlamaModelConfig& new_cfg);
```

#### Usage Example

```cpp
auto llm = std::make_shared<LlamaCoprocessorDriver>();
// ... initialize ...

// Load initial model
LlamaModelConfig cfg1;
cfg1.model_path = "model1.gguf";
cfg1.n_ctx = 2048;
llm->set_model_config(cfg1);
llm->load_model();

// Run inference
std::string result1 = llm->infer("Prompt for model 1", seq_cfg);

// Switch to different model
LlamaModelConfig cfg2;
cfg2.model_path = "model2.gguf";
cfg2.n_ctx = 4096;
llm->switch_model(cfg2);

// Run inference with new model
std::string result2 = llm->infer("Prompt for model 2", seq_cfg);

// Or unload without loading new model
llm->unload_model();
```

#### Implementation Details

- `unload_model()` cleans up GGUF runtime and frees KV-cache
- `switch_model()` performs atomic unload + configure + load
- Updates `STATUS_MODEL_READY` flag appropriately
- Proper resource cleanup prevents memory leaks
- Compatible with all other features (GPU, KV-cache, etc.)

### 5. Batch Inference

Process multiple prompts efficiently in a queue.

#### Hardware Registers

```
REG_BATCH_SIZE     (0x40001038)  - Number of requests in batch queue
REG_BATCH_IDX      (0x4000103C)  - Current batch processing index
```

#### API

```cpp
// Add request to batch queue
bool add_batch_request(const std::string& prompt, const LlamaSequenceConfig& cfg);

// Process all queued requests
bool process_batch();

// Get all completed results
std::vector<std::string> get_batch_results();

// Clear queue
void clear_batch();

// Query queue state
size_t get_batch_queue_size() const;
```

#### Usage Example

```cpp
auto llm = std::make_shared<LlamaCoprocessorDriver>();
// ... initialize and load model ...

LlamaSequenceConfig seq;
seq.n_predict = 128;

// Queue multiple prompts
llm->add_batch_request("First prompt", seq);
llm->add_batch_request("Second prompt", seq);
llm->add_batch_request("Third prompt", seq);

std::cout << "Queue size: " << llm->get_batch_queue_size() << "\n";

// Process all at once
llm->process_batch();

// Retrieve results
auto results = llm->get_batch_results();
for (size_t i = 0; i < results.size(); i++) {
    std::cout << "Result " << i << ": " << results[i] << "\n";
}

// Clear queue for next batch
llm->clear_batch();
```

#### Implementation Details

- Internal `BatchRequest` struct stores prompt, config, and result
- Sequential processing (not parallel in current implementation)
- `REG_BATCH_SIZE` updated as requests are added
- `REG_BATCH_IDX` tracks current processing position
- Results stored until `clear_batch()` is called
- Can be extended for parallel processing in future

## Combined Features Example

All features can be used together:

```cpp
auto device = std::make_shared<VirtualPCB>("NPU-001", "NPU-X1");
device->initialize();
device->start();

auto llm = std::make_shared<LlamaCoprocessorDriver>();
llm->load(device.get());
llm->initialize();

// Configure model with GPU support
LlamaModelConfig cfg;
cfg.model_path = "large-model.gguf";
cfg.n_ctx = 8192;
cfg.n_gpu_layers = 32;
llm->set_model_config(cfg);
llm->load_model();

// Enable all features
llm->set_gpu_layers(32);        // GPU acceleration
llm->enable_kv_cache();         // KV-cache persistence
llm->enable_token_interrupts(5); // Interrupt support

// Process batch with all features active
LlamaSequenceConfig seq;
seq.n_predict = 256;

llm->add_batch_request("First complex query", seq);
llm->add_batch_request("Second complex query", seq);
llm->add_batch_request("Third complex query", seq);

llm->process_batch();
auto results = llm->get_batch_results();

// Switch to different model while preserving cache
LlamaModelConfig cfg2;
cfg2.model_path = "specialized-model.gguf";
llm->switch_model(cfg2);

// Continue with new model
std::string result = llm->infer("Query for specialized model", seq);
```

## Hardware Register Summary

Complete register map for Next Steps features:

| Address | Register | Purpose |
|---------|----------|---------|
| 0x4000102C | REG_GPU_LAYERS | GPU layer count |
| 0x40001030 | REG_KV_CACHE_ADDR | KV-cache SRAM address |
| 0x40001034 | REG_KV_CACHE_SIZE | KV-cache size in bytes |
| 0x40001038 | REG_BATCH_SIZE | Batch queue size |
| 0x4000103C | REG_BATCH_IDX | Current batch index |

## Status Flag Summary

Additional status flags:

| Flag | Bit | Meaning |
|------|-----|---------|
| STATUS_KV_CACHED | 5 | KV-cache allocated |
| STATUS_GPU_ACTIVE | 6 | GPU offloading active |

## Command Bit Summary

Additional command bits:

| Command | Bit | Purpose |
|---------|-----|---------|
| CMD_UNLOAD_MODEL | 4 | Unload current model |
| CMD_ALLOC_KVCACHE | 5 | Allocate KV-cache |

## Testing

The `test-next-steps` binary provides comprehensive validation:

```bash
./build/test-next-steps

# Tests:
# 1. KV-Cache Management - allocation, deallocation, status
# 2. GPU Offloading Control - dynamic configuration
# 3. Interrupt Support - enable/disable, vector config
# 4. Model Hot-Swapping - unload, load, switch
# 5. Batch Inference - queue, process, results
# 6. Combined Features - all features working together
```

All tests verify:
- Hardware register updates
- Status flag transitions
- Proper resource cleanup
- Feature interactions
- Error handling

## Performance Considerations

### KV-Cache
- **Memory:** ~1KB per context token (configurable)
- **Benefit:** Faster sequential inferences
- **Trade-off:** SRAM space usage

### GPU Offloading
- **Speed:** Depends on model and GPU
- **Layers:** More layers = more GPU compute
- **Memory:** GPU VRAM required

### Batch Inference
- **Throughput:** Sequential (not parallel in current version)
- **Latency:** Sum of individual inference times
- **Future:** Can be parallelized with thread pool

### Model Hot-Swapping
- **Reload Time:** Model-dependent (similar to initial load)
- **Memory:** Brief spike during switch (2x model size)
- **Use Case:** Switch between specialized models

## Best Practices

1. **Enable KV-cache for conversation-style workloads**
   - Reuses context across turns
   - Reduces redundant computation

2. **Tune GPU layers based on VRAM**
   - Start with half the model layers
   - Increase until VRAM limit reached

3. **Use batch inference for similar prompts**
   - Process similar-length prompts together
   - Reduces per-prompt overhead

4. **Switch models for specialized tasks**
   - Use general model for most queries
   - Switch to specialized model when needed
   - Switch back to general model

5. **Combine features for optimal performance**
   - GPU + KV-cache for long conversations
   - Batch + GPU for high throughput
   - Interrupts for real-time applications

## Future Enhancements

Potential improvements to these features:

1. **Parallel Batch Processing**
   - Thread pool for concurrent inference
   - Configurable worker threads
   - Load balancing

2. **Advanced KV-Cache**
   - Multi-session cache management
   - Cache sharing between related prompts
   - Persistent cache across reboots

3. **Dynamic GPU Reallocation**
   - Runtime layer migration
   - Adaptive offloading based on load
   - Mixed CPU/GPU execution

4. **LoRA Adapter Support**
   - Load fine-tuned adapters
   - Switch adapters without model reload
   - Multiple active adapters

## Conclusion

The Next Steps features transform the NPU coprocessor from a prototype into a production-ready LLM inference accelerator. All features maintain the hardware-first design philosophy with memory-mapped registers, status flags, and proper telemetry integration.

The implementation is fully tested, documented, and ready for use with the mock llama.cpp API. When migrating to real llama.cpp, all these features will work unchanged - only the underlying inference engine needs to be swapped.
