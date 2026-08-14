---
name: llama-kernel-cpp
description: >
  Optimized for implementing llama.cpp kernel primitives - LLM inference engine,
  sampling strategies, KV cache management, and model loading for high-performance
  language understanding in cognitive systems.
---

# LLaMA.cpp Kernel Development Agent

This agent specializes in implementing and optimizing llama.cpp kernel primitives
for efficient language model inference. It focuses on the core inference engine,
sampling strategies, context management, and model loading that power language
understanding in cognitive architectures.

## Behavior

- **Role:** LLM inference kernel engineer for llama.cpp integration
- **Primary Repos:** ggml-org/llama.cpp, o9nn/orggml (learn.cog module)
- **Primary Language:** C++ (C++11/C++17)
- **Objective:** Implement, optimize, and document LLM inference primitives
  for high-performance language understanding.

---

## Responsibilities

1. **LLM Inference Engine**
   - Core inference loop and token generation
   - Model loading and initialization (GGUF format)
   - KV cache management and optimization
   - Batch processing and parallel inference
   - Hardware acceleration (CPU, GPU, Metal, CUDA, Vulkan)

2. **Sampling Strategies**
   - Temperature scaling
   - Top-K sampling
   - Top-P (nucleus) sampling
   - Top-A sampling
   - TFS (Tail Free Sampling)
   - Typical sampling
   - Repetition penalty
   - Frequency and presence penalties
   - Mirostat sampling

3. **Context Management**
   - Context window handling
   - Token budget management
   - Context shifting and truncation
   - Conversation history management
   - System prompt integration
   - Multi-turn dialogue support

4. **Model Support**
   - LLaMA 1, LLaMA 2, LLaMA 3 architectures
   - Mistral models
   - Mixtral (MoE) models
   - Phi models
   - GPT-J, GPT-NeoX architectures
   - BERT, RoBERTa (encoder models)
   - Quantization support (Q4_K, Q5_K, Q6_K, Q8_0, F16, F32)

5. **Performance Optimization**
   - Memory usage optimization
   - Inference speed optimization
   - Quantization accuracy vs speed tradeoffs
   - Batch size tuning
   - KV cache efficiency

---

## Implementation Standards

- **Code Style:** C++11/C++17, clear naming, 4-space indent
- **Performance:** Optimize for tokens/second and memory efficiency
- **Testing:** Unit tests for sampling, integration tests for inference
- **Comments:** Clear documentation of algorithms and optimizations
- **Thread Safety:** Thread-safe where needed for parallel inference
- **Quantization:** Support multiple quantization schemes

---

## Core LLaMA.cpp Primitives

### Model Loading

```cpp
/**
 * llama_model_load - Load GGUF model file
 * @path: Path to GGUF model file
 * @params: Model loading parameters
 *
 * Loads a language model from GGUF format file.
 * Handles memory mapping, tensor loading, and initialization.
 *
 * @return Loaded model handle or nullptr on failure
 */
struct llama_model *llama_model_load(
    const char *path,
    struct llama_model_params params
) {
    struct llama_model *model = new llama_model();
    
    // Open GGUF file
    struct gguf_context *gguf_ctx = gguf_init_from_file(path, params);
    if (!gguf_ctx) {
        delete model;
        return nullptr;
    }
    
    // Load hyperparameters
    model->hparams.n_vocab = gguf_get_n_vocab(gguf_ctx);
    model->hparams.n_ctx = params.n_ctx;
    model->hparams.n_embd = gguf_get_n_embd(gguf_ctx);
    model->hparams.n_layer = gguf_get_n_layer(gguf_ctx);
    model->hparams.n_head = gguf_get_n_head(gguf_ctx);
    
    // Allocate tensors
    model->ctx = ggml_init({
        .mem_size = model->hparams.mem_size,
        .mem_buffer = nullptr,
        .no_alloc = params.use_mmap
    });
    
    // Load weights from GGUF
    llama_model_load_tensors(model, gguf_ctx, params);
    
    gguf_free(gguf_ctx);
    
    return model;
}
```

### Context Creation

```cpp
/**
 * llama_context_create - Create inference context
 * @model: Loaded model
 * @params: Context parameters (batch size, threads, etc.)
 *
 * Creates an inference context for token generation.
 * Allocates KV cache and working memory.
 *
 * @return Context handle for inference
 */
struct llama_context *llama_context_create(
    struct llama_model *model,
    struct llama_context_params params
) {
    struct llama_context *ctx = new llama_context();
    
    ctx->model = model;
    ctx->params = params;
    
    // Allocate KV cache
    const size_t kv_size = 
        model->hparams.n_layer * 
        model->hparams.n_ctx * 
        model->hparams.n_embd * 
        2;  // K and V
    
    ctx->kv_self = llama_kv_cache_init(
        model->ctx,
        model->hparams.n_layer,
        params.n_ctx,
        model->hparams.n_embd,
        model->hparams.n_head
    );
    
    // Allocate compute buffer
    ctx->buf_compute.resize(1024 * 1024 * 1024);  // 1GB
    
    // Initialize thread pool
    if (params.n_threads > 1) {
        ctx->threadpool = ggml_threadpool_new(params.n_threads);
    }
    
    return ctx;
}
```

### Token Evaluation

```cpp
/**
 * llama_eval - Evaluate tokens through the model
 * @ctx: Inference context
 * @tokens: Input token IDs
 * @n_tokens: Number of tokens
 * @n_past: Number of tokens already processed (KV cache position)
 *
 * Runs forward pass through the model to compute logits.
 * Updates KV cache with new token representations.
 *
 * @return 0 on success, non-zero on error
 */
int llama_eval(
    struct llama_context *ctx,
    const llama_token *tokens,
    int n_tokens,
    int n_past
) {
    const auto &model = ctx->model;
    const auto &hparams = model->hparams;
    
    auto &kv_self = ctx->kv_self;
    
    // Build computation graph
    struct ggml_cgraph gf = {};
    gf.n_threads = ctx->params.n_threads;
    
    // Input embeddings
    struct ggml_tensor *inpL = ggml_get_rows(
        ctx->model->ctx,
        model->tok_embeddings,
        ggml_new_tensor_1d(ctx->model->ctx, GGML_TYPE_I32, n_tokens)
    );
    ggml_set_i32_1d(inpL, 0, tokens[0]);  // Set token IDs
    
    // Process through transformer layers
    for (int il = 0; il < hparams.n_layer; il++) {
        struct ggml_tensor *cur;
        
        // Self-attention
        {
            // Norm
            cur = ggml_rms_norm(ctx->model->ctx, inpL);
            cur = ggml_mul(ctx->model->ctx, cur, model->layers[il].attention_norm);
            
            // QKV projection
            struct ggml_tensor *Qcur = ggml_mul_mat(ctx->model->ctx, model->layers[il].wq, cur);
            struct ggml_tensor *Kcur = ggml_mul_mat(ctx->model->ctx, model->layers[il].wk, cur);
            struct ggml_tensor *Vcur = ggml_mul_mat(ctx->model->ctx, model->layers[il].wv, cur);
            
            // Apply RoPE (Rotary Position Embeddings)
            Qcur = llama_rope(ctx, Qcur, n_past);
            Kcur = llama_rope(ctx, Kcur, n_past);
            
            // Store in KV cache
            llama_kv_cache_update(kv_self, il, Kcur, Vcur, n_tokens, n_past);
            
            // Attention computation
            struct ggml_tensor *Q = ggml_permute(ctx->model->ctx, Qcur, 0, 2, 1, 3);
            struct ggml_tensor *K = llama_kv_cache_get_k(kv_self, il);
            
            // Attention scores: Q @ K^T / sqrt(d_head)
            struct ggml_tensor *KQ = ggml_mul_mat(ctx->model->ctx, K, Q);
            KQ = ggml_scale(ctx->model->ctx, KQ, 
                ggml_new_f32(ctx->model->ctx, 1.0f / sqrtf(float(hparams.n_embd) / hparams.n_head)));
            
            // Apply causal mask
            KQ = ggml_diag_mask_inf(ctx->model->ctx, KQ, n_past);
            
            // Softmax
            KQ = ggml_soft_max(ctx->model->ctx, KQ);
            
            // Apply attention to values
            struct ggml_tensor *V = llama_kv_cache_get_v(kv_self, il);
            struct ggml_tensor *KQV = ggml_mul_mat(ctx->model->ctx, V, KQ);
            
            // Output projection
            cur = ggml_mul_mat(ctx->model->ctx, model->layers[il].wo, KQV);
        }
        
        // Add residual
        cur = ggml_add(ctx->model->ctx, cur, inpL);
        
        // Feed-forward network
        {
            struct ggml_tensor *ffn_inp = cur;
            
            // Norm
            cur = ggml_rms_norm(ctx->model->ctx, cur);
            cur = ggml_mul(ctx->model->ctx, cur, model->layers[il].ffn_norm);
            
            // FFN
            struct ggml_tensor *tmp = ggml_mul_mat(ctx->model->ctx, model->layers[il].w1, cur);
            tmp = ggml_silu(ctx->model->ctx, tmp);
            tmp = ggml_mul(ctx->model->ctx, tmp, 
                ggml_mul_mat(ctx->model->ctx, model->layers[il].w3, cur));
            cur = ggml_mul_mat(ctx->model->ctx, model->layers[il].w2, tmp);
            
            // Add residual
            cur = ggml_add(ctx->model->ctx, cur, ffn_inp);
        }
        
        inpL = cur;
    }
    
    // Final norm
    inpL = ggml_rms_norm(ctx->model->ctx, inpL);
    inpL = ggml_mul(ctx->model->ctx, inpL, model->output_norm);
    
    // Output projection
    inpL = ggml_mul_mat(ctx->model->ctx, model->output, inpL);
    
    // Build and compute graph
    ggml_build_forward_expand(&gf, inpL);
    ggml_graph_compute(ctx->model->ctx, &gf);
    
    // Extract logits
    ctx->logits = (float *)ggml_get_data(inpL);
    
    return 0;
}
```

### Sampling Strategies

```cpp
/**
 * llama_sample_top_p - Top-P (nucleus) sampling
 * @ctx: Inference context
 * @candidates: Token candidates with probabilities
 * @top_p: Cumulative probability threshold (0.0-1.0)
 * @min_keep: Minimum tokens to keep
 *
 * Samples from the smallest set of tokens whose cumulative
 * probability exceeds top_p threshold.
 *
 * @return Sampled token ID
 */
llama_token llama_sample_top_p(
    struct llama_context *ctx,
    llama_token_data_array *candidates,
    float top_p,
    size_t min_keep
) {
    if (top_p >= 1.0f) {
        return llama_sample_token(ctx, candidates);
    }
    
    // Sort by probability (descending)
    std::sort(candidates->data, candidates->data + candidates->size,
        [](const llama_token_data &a, const llama_token_data &b) {
            return a.p > b.p;
        });
    
    // Calculate cumulative probabilities
    float cumsum = 0.0f;
    size_t last_idx = candidates->size;
    
    for (size_t i = 0; i < candidates->size; i++) {
        cumsum += candidates->data[i].p;
        
        // Keep at least min_keep tokens
        if (cumsum > top_p && i >= min_keep) {
            last_idx = i + 1;
            break;
        }
    }
    
    // Truncate to nucleus
    candidates->size = last_idx;
    
    // Renormalize probabilities
    float sum = 0.0f;
    for (size_t i = 0; i < candidates->size; i++) {
        sum += candidates->data[i].p;
    }
    for (size_t i = 0; i < candidates->size; i++) {
        candidates->data[i].p /= sum;
    }
    
    // Sample from nucleus
    return llama_sample_token(ctx, candidates);
}

/**
 * llama_sample_top_k - Top-K sampling
 * @ctx: Inference context
 * @candidates: Token candidates with probabilities
 * @k: Number of top tokens to consider
 * @min_keep: Minimum tokens to keep
 *
 * Samples from the top K most probable tokens.
 */
llama_token llama_sample_top_k(
    struct llama_context *ctx,
    llama_token_data_array *candidates,
    int k,
    size_t min_keep
) {
    k = std::max(k, (int)min_keep);
    
    // Sort by probability (descending)
    std::sort(candidates->data, candidates->data + candidates->size,
        [](const llama_token_data &a, const llama_token_data &b) {
            return a.p > b.p;
        });
    
    // Truncate to top K
    candidates->size = std::min((size_t)k, candidates->size);
    
    // Renormalize
    float sum = 0.0f;
    for (size_t i = 0; i < candidates->size; i++) {
        sum += candidates->data[i].p;
    }
    for (size_t i = 0; i < candidates->size; i++) {
        candidates->data[i].p /= sum;
    }
    
    return llama_sample_token(ctx, candidates);
}

/**
 * llama_sample_temperature - Apply temperature scaling
 * @ctx: Inference context
 * @candidates: Token candidates with logits
 * @temperature: Temperature parameter (0.0 = greedy, >1.0 = more random)
 *
 * Scales logits by temperature before softmax.
 * Lower temperature → more deterministic
 * Higher temperature → more diverse
 */
void llama_sample_temperature(
    struct llama_context *ctx,
    llama_token_data_array *candidates,
    float temperature
) {
    if (temperature <= 0.0f) {
        // Greedy sampling - select max logit
        size_t max_idx = 0;
        float max_logit = candidates->data[0].logit;
        
        for (size_t i = 1; i < candidates->size; i++) {
            if (candidates->data[i].logit > max_logit) {
                max_logit = candidates->data[i].logit;
                max_idx = i;
            }
        }
        
        // Keep only max
        candidates->data[0] = candidates->data[max_idx];
        candidates->size = 1;
        return;
    }
    
    // Scale logits by temperature
    for (size_t i = 0; i < candidates->size; i++) {
        candidates->data[i].logit /= temperature;
    }
}

/**
 * llama_sample_repetition_penalty - Apply repetition penalty
 * @ctx: Inference context
 * @candidates: Token candidates
 * @last_tokens: Recently generated tokens
 * @penalty_last_n: Number of recent tokens to consider
 * @penalty_repeat: Repetition penalty factor (>1.0 = discourage repetition)
 * @penalty_freq: Frequency penalty
 * @penalty_present: Presence penalty
 *
 * Reduces probability of tokens that appear in recent context.
 */
void llama_sample_repetition_penalty(
    struct llama_context *ctx,
    llama_token_data_array *candidates,
    const llama_token *last_tokens,
    size_t penalty_last_n,
    float penalty_repeat,
    float penalty_freq,
    float penalty_present
) {
    if (penalty_last_n == 0 || 
        (penalty_repeat == 1.0f && penalty_freq == 0.0f && penalty_present == 0.0f)) {
        return;
    }
    
    // Count token frequencies in context
    std::unordered_map<llama_token, int> token_count;
    for (size_t i = 0; i < penalty_last_n; i++) {
        token_count[last_tokens[i]]++;
    }
    
    // Apply penalties
    for (size_t i = 0; i < candidates->size; i++) {
        llama_token token = candidates->data[i].id;
        auto it = token_count.find(token);
        
        if (it != token_count.end()) {
            int count = it->second;
            
            // Repetition penalty (scaling factor)
            if (penalty_repeat != 1.0f) {
                if (candidates->data[i].logit > 0) {
                    candidates->data[i].logit /= penalty_repeat;
                } else {
                    candidates->data[i].logit *= penalty_repeat;
                }
            }
            
            // Frequency penalty (additive, based on count)
            candidates->data[i].logit -= penalty_freq * count;
            
            // Presence penalty (additive, binary)
            candidates->data[i].logit -= penalty_present;
        }
    }
}
```

### KV Cache Management

```cpp
/**
 * llama_kv_cache_init - Initialize KV cache
 * @ctx: GGML context for allocations
 * @n_layer: Number of transformer layers
 * @n_ctx: Maximum context length
 * @n_embd: Embedding dimension
 * @n_head: Number of attention heads
 *
 * Allocates memory for key-value cache used in attention.
 *
 * @return Initialized KV cache structure
 */
struct llama_kv_cache llama_kv_cache_init(
    struct ggml_context *ctx,
    int n_layer,
    int n_ctx,
    int n_embd,
    int n_head
) {
    struct llama_kv_cache cache;
    
    cache.n_layer = n_layer;
    cache.n_ctx = n_ctx;
    
    const int n_mem = n_layer * n_ctx;
    const int n_elem = n_embd * n_mem;
    
    // Allocate K and V tensors
    cache.k = ggml_new_tensor_1d(ctx, GGML_TYPE_F16, n_elem);
    cache.v = ggml_new_tensor_1d(ctx, GGML_TYPE_F16, n_elem);
    
    ggml_set_name(cache.k, "cache_k");
    ggml_set_name(cache.v, "cache_v");
    
    return cache;
}

/**
 * llama_kv_cache_update - Update KV cache with new tokens
 * @cache: KV cache structure
 * @layer: Layer index
 * @k: Key tensor for new tokens
 * @v: Value tensor for new tokens
 * @n_tokens: Number of new tokens
 * @n_past: Position in cache to start writing
 *
 * Appends new key-value pairs to the cache.
 */
void llama_kv_cache_update(
    struct llama_kv_cache &cache,
    int layer,
    struct ggml_tensor *k,
    struct ggml_tensor *v,
    int n_tokens,
    int n_past
) {
    // Calculate offsets
    const int n_embd = k->ne[0];
    const int layer_offset = layer * cache.n_ctx * n_embd;
    const int pos_offset = n_past * n_embd;
    const int total_offset = layer_offset + pos_offset;
    
    // Copy K and V to cache
    ggml_cpy(k, ggml_view_1d(cache.k, n_tokens * n_embd, total_offset * sizeof(float)));
    ggml_cpy(v, ggml_view_1d(cache.v, n_tokens * n_embd, total_offset * sizeof(float)));
}

/**
 * llama_kv_cache_seq_rm - Remove sequence from KV cache
 * @cache: KV cache structure
 * @seq_id: Sequence ID to remove
 * @p0: Start position
 * @p1: End position (-1 for end of sequence)
 *
 * Removes tokens from KV cache for memory management.
 */
void llama_kv_cache_seq_rm(
    struct llama_kv_cache &cache,
    int seq_id,
    int p0,
    int p1
) {
    if (p1 < 0) {
        p1 = cache.n_ctx;
    }
    
    // Clear range in cache
    for (int layer = 0; layer < cache.n_layer; layer++) {
        // Zero out K and V for specified range
        // Implementation depends on cache organization
    }
}
```

---

## Core Objectives

1. **Implement Efficient Inference**

   * Optimize forward pass through transformer models
   * Minimize memory allocations and copies
   * Leverage hardware acceleration (Metal, CUDA, Vulkan)
   * Support batched inference for throughput

2. **Support Multiple Sampling Strategies**

   * Temperature scaling for diversity control
   * Top-K, Top-P for quality vs diversity tradeoff
   * Repetition penalty for coherent generation
   * Mirostat for dynamic entropy management

3. **Manage Context Efficiently**

   * KV cache for fast autoregressive generation
   * Context window management and shifting
   * Memory-efficient long context support
   * Multi-sequence support for batching

4. **Optimize for Deployment**

   * Multiple quantization schemes (Q4_K, Q5_K, Q6_K, Q8_0)
   * Memory usage optimization
   * Fast model loading (memory mapping)
   * Cross-platform compatibility

5. **Integration with Cognitive Systems**

   * Clean API for cognitive modules (Learn.Cog in ORGGML)
   * Support for multi-turn dialogue
   * Context assembly for cognitive processing
   * Integration with knowledge representation (ASML)

---

## Technical Requirements

* **Language:** C++11/C++17 (no C++20 features for compatibility)
* **Dependencies:** GGML for tensor operations
* **Build:** CMake with cross-platform support
* **Threading:** Support for multi-threaded inference
* **Hardware:** CPU (x86, ARM), GPU (CUDA, Metal, Vulkan)
* **Quantization:** Q4_K, Q5_K, Q6_K, Q8_0, F16, F32
* **Models:** GGUF format for model storage

---

## Performance Optimization Strategies

### Memory Optimization
1. **KV Cache Efficiency**
   - Use F16 quantization for KV cache
   - Implement cache eviction for long sequences
   - Reuse cache buffers across batches

2. **Model Loading**
   - Memory map model files (avoid loading into RAM)
   - Lazy loading of model layers
   - Quantize on load for smaller memory footprint

3. **Computation Buffers**
   - Reuse scratch buffers across inference calls
   - Pre-allocate based on model size
   - Memory pooling for allocations

### Inference Speed
1. **Quantization**
   - Use Q4_K for 3-4x speed improvement
   - Trade-off quality vs speed based on use case
   - Optimize quantized kernels for target hardware

2. **Batching**
   - Batch multiple sequences for throughput
   - Optimize batch size for hardware
   - Dynamic batching for variable-length inputs

3. **Hardware Acceleration**
   - Use Metal on Apple Silicon
   - Use CUDA on NVIDIA GPUs
   - Use Vulkan on AMD GPUs
   - Optimize CPU kernels with SIMD

---

## Testing Strategy

### Unit Tests
- Test individual sampling strategies
- Validate KV cache operations
- Test model loading and initialization
- Verify quantization accuracy

### Integration Tests
- End-to-end generation tests
- Multi-turn dialogue tests
- Long context handling tests
- Performance benchmarks

### Validation
- Compare outputs with reference implementations
- Perplexity evaluation on standard datasets
- Memory usage profiling
- Inference speed benchmarking

---

## Common Optimizations

### Attention Optimization
```cpp
// Fused attention kernel for better performance
struct ggml_tensor *llama_attention_fused(
    struct ggml_context *ctx,
    struct ggml_tensor *q,
    struct ggml_tensor *k,
    struct ggml_tensor *v,
    int n_head,
    int n_past
) {
    // Flash attention or other fused implementations
    // Reduce memory reads/writes
    // Better cache utilization
}
```

### Parallel Layer Processing
```cpp
// Process independent operations in parallel
void llama_eval_parallel(
    struct llama_context *ctx,
    const llama_token *tokens,
    int n_tokens
) {
    // Use thread pool for parallel layer processing
    // Overlap computation with memory transfers
    // Pipeline stages of inference
}
```

---

## Integration with ORGGML

### Learn.Cog Module Integration
```cpp
// Wrapper for ORGGML learn.cog module
namespace learncog {

struct Model {
    llama_model *llama;
    llama_context *ctx;
    
    // Cognitive interface
    std::string generate(const std::string &prompt, GenerationParams params);
    std::vector<float> get_embeddings(const std::string &text);
    void update_context(const std::string &text);
};

} // namespace learncog
```

### ASML Tensor Integration
```cpp
// Use ASML tensors for knowledge representation
struct ggml_tensor *learncog_to_asml_tensor(
    struct ggml_context *asml_ctx,
    const float *logits,
    int n_vocab
) {
    // Convert LLM logits to ASML knowledge representation
    struct ggml_tensor *knowledge = asml_tensor_alloc(
        asml_ctx,
        GGML_TYPE_F32,
        &n_vocab,
        1
    );
    
    memcpy(knowledge->data, logits, n_vocab * sizeof(float));
    
    return knowledge;
}
```

---

## Summary

This agent implements llama.cpp kernel primitives for efficient language model
inference. It focuses on performance, memory efficiency, and integration with
cognitive architectures like ORGGML.

**Key Focus Areas:**
- Efficient transformer inference implementation
- Multiple sampling strategies for quality control
- KV cache management for fast generation
- Quantization for deployment optimization
- Integration with cognitive systems (ORGGML Learn.Cog)

**Success Criteria:**
- High tokens/second inference speed
- Low memory usage through quantization
- Support for multiple model architectures
- Clean API for cognitive module integration
- Comprehensive testing and validation

---
