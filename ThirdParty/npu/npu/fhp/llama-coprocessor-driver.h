#pragma once

#include "virtual-device.h"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>
#include <chrono>
#include <cstdint>

namespace ggnucash {
namespace vdev {

// ============================================================================
// Llama GGUF Co-Processor Driver
// ============================================================================
// This driver models a GGUF-backed LLM accelerator as a memory-mapped
// co-processor hanging off the VirtualPCB PERIPH space.
//
// High level:
//   - Model is configured via LlamaModelConfig
//   - Prompts are written/buffered by the driver
//   - Inference is kicked by writing command registers
//   - Tokens are conceptually read from TOKEN_OUT (stubbed for now)
// ============================================================================

// ----------------------------------------------------------------------------
// Model configuration and telemetry structs
// ----------------------------------------------------------------------------

struct LlamaModelConfig {
    std::string model_path;      // .gguf file path
    std::string model_name;      // Friendly name
    int32_t     n_ctx        = 4096;
    int32_t     n_threads    = 4;
    int32_t     n_gpu_layers = 0;   // 0 = CPU only
    int32_t     batch_size   = 1;
    bool        offload_kv_cache = false;
    bool        low_vram_mode    = false;
};

struct LlamaTelemetry {
    double tokens_per_second           = 0.0;
    uint64_t total_tokens_generated    = 0;
    uint64_t total_prompts             = 0;
    uint64_t last_prompt_tokens        = 0;
    uint64_t last_completion_tokens    = 0;
    std::chrono::steady_clock::time_point last_inference_start;
    std::chrono::steady_clock::time_point last_inference_end;
};

struct LlamaSequenceConfig {
    int32_t n_predict     = 128;
    int32_t max_ctx       = 4096;
    bool    echo_prompt   = false;
    bool    stream_tokens = true;
    int32_t stream_delay_ms = 5;  // Delay between tokens in streaming mode
    std::string system_prompt;
};

// ----------------------------------------------------------------------------
// LlamaCoprocessorDriver
// ----------------------------------------------------------------------------

class LlamaCoprocessorDriver : public DeviceDriver {
public:
    // ------------------------------------------------------------------------
    // Memory-mapped register layout (inside PERIPH region)
    // ------------------------------------------------------------------------
    static constexpr uint64_t REG_BASE             = 0x40001000;

    // Control & status
    static constexpr uint64_t REG_CMD              = REG_BASE + 0x00;
    static constexpr uint64_t REG_STATUS           = REG_BASE + 0x04;

    // Inference configuration
    static constexpr uint64_t REG_PROMPT_ADDR      = REG_BASE + 0x08;
    static constexpr uint64_t REG_PROMPT_LEN       = REG_BASE + 0x0C;
    static constexpr uint64_t REG_N_PREDICT        = REG_BASE + 0x10;

    // Token I/O
    static constexpr uint64_t REG_TOKEN_OUT        = REG_BASE + 0x14;
    static constexpr uint64_t REG_TOKEN_READY      = REG_BASE + 0x18;

    // Model & context info
    static constexpr uint64_t REG_MODEL_ID         = REG_BASE + 0x1C;
    static constexpr uint64_t REG_CTX_USED         = REG_BASE + 0x20;

    // Diagnostics / perf
    static constexpr uint64_t REG_ERROR_CODE       = REG_BASE + 0x24;
    static constexpr uint64_t REG_PERF_TOKENS_SEC  = REG_BASE + 0x28;
    
    // GPU offloading control (Next Steps feature)
    static constexpr uint64_t REG_GPU_LAYERS       = REG_BASE + 0x2C;
    
    // KV-Cache management (Next Steps feature)
    static constexpr uint64_t REG_KV_CACHE_ADDR    = REG_BASE + 0x30;
    static constexpr uint64_t REG_KV_CACHE_SIZE    = REG_BASE + 0x34;
    
    // KV-Cache configuration constants
    static constexpr uint64_t KV_CACHE_SRAM_OFFSET = 0x20010000;  // SRAM offset for KV-cache
    static constexpr size_t BYTES_PER_CONTEXT_TOKEN = 1024;       // Approximate cache size per token
    
    // Batch inference (Next Steps feature)
    static constexpr uint64_t REG_BATCH_SIZE       = REG_BASE + 0x38;
    static constexpr uint64_t REG_BATCH_IDX        = REG_BASE + 0x3C;

    // Command bits
    enum CommandBits : uint32_t {
        CMD_NONE        = 0,
        CMD_RESET       = 1u << 0,
        CMD_LOAD_MODEL  = 1u << 1,
        CMD_START_INF   = 1u << 2,
        CMD_SOFT_STOP   = 1u << 3,
        CMD_UNLOAD_MODEL = 1u << 4,  // Next Steps: Model hot-swapping
        CMD_ALLOC_KVCACHE = 1u << 5, // Next Steps: KV-cache management
    };

    // Status bits
    enum StatusBits : uint32_t {
        STATUS_IDLE        = 0,
        STATUS_BUSY        = 1u << 0,
        STATUS_EOG         = 1u << 1,  // End-of-generation
        STATUS_ERROR       = 1u << 2,
        STATUS_MODEL_READY = 1u << 3,
        STATUS_TOKEN_READY = 1u << 4,
        STATUS_KV_CACHED   = 1u << 5,  // Next Steps: KV-cache allocated
        STATUS_GPU_ACTIVE  = 1u << 6,  // Next Steps: GPU offloading active
    };

private:
    VirtualPCB* pcb = nullptr;

    // Bookkeeping
    LlamaModelConfig model_cfg;
    LlamaTelemetry   telemetry;
    bool             model_loaded   = false;
    bool             device_online  = false;
    uint32_t         model_id       = 0x00001; // arbitrary ID for now

    // GGUF Runtime Integration (forward declarations to avoid header pollution)
    struct LlamaRuntime;
    LlamaRuntime* llama_runtime_ = nullptr;

    // Internal buffers for stubbed/demo behaviour
    std::vector<int32_t> last_prompt_tokens;
    std::vector<int32_t> last_output_tokens;

    // Interrupt vector index for "LLM_DONE" or "TOKEN_READY"
    int llm_irq_vector = -1;
    
    // Next Steps: KV-Cache management
    uint64_t kv_cache_addr_ = 0;
    size_t kv_cache_size_ = 0;
    bool kv_cache_allocated_ = false;
    
    // Next Steps: Batch inference support
    struct BatchRequest {
        std::string prompt;
        LlamaSequenceConfig config;
        std::string result;
        bool completed = false;
    };
    std::vector<BatchRequest> batch_queue_;
    size_t current_batch_idx_ = 0;

    // Internal helpers
    bool map_register_region();
    bool allocate_llm_sram();
    void update_telemetry_from_hw();
    void write_reg32(uint64_t addr, uint32_t value);
    uint32_t read_reg32(uint64_t addr) const;
    
    // GGUF integration helpers
    bool init_llama_runtime();
    void free_llama_runtime();
    std::vector<int32_t> tokenize_prompt(const std::string& text);
    std::string detokenize_tokens(const std::vector<int32_t>& tokens);
    std::vector<int32_t> generate_tokens(const std::vector<int32_t>& prompt, int32_t n_predict);
    
    // Next Steps: KV-Cache management helpers
    bool allocate_kv_cache(size_t size);
    void free_kv_cache();
    size_t calculate_kv_cache_size() const;
    
    // Next Steps: Interrupt support helpers
    void raise_token_ready_interrupt();
    void configure_interrupt_vector(int vector_index);

public:
    // ------------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------------
    LlamaCoprocessorDriver()
        : DeviceDriver("GGNuCash-LLM-Coprocessor", "0.1.0") {}
    
    // Destructor - needs to be declared to properly destroy unique_ptr<LlamaRuntime>
    ~LlamaCoprocessorDriver();

    // ------------------------------------------------------------------------
    // DeviceDriver interface
    // ------------------------------------------------------------------------
    bool load(VirtualPCB* pcb_) override;
    bool initialize() override;
    bool probe() override;
    bool remove() override;

    // ------------------------------------------------------------------------
    // Model / configuration operations
    // ------------------------------------------------------------------------

    void set_model_config(const LlamaModelConfig& cfg) {
        model_cfg = cfg;
    }

    const LlamaModelConfig& get_model_config() const {
        return model_cfg;
    }

    bool load_model();
    bool is_model_loaded() const {
        return model_loaded;
    }
    
    // Next Steps: Model hot-swapping
    bool unload_model();
    bool switch_model(const LlamaModelConfig& new_cfg);

    // ------------------------------------------------------------------------
    // Low-level hardware-style API (MMIO abstraction)
    // ------------------------------------------------------------------------

    bool configure_inference(uint64_t prompt_addr,
                             uint32_t prompt_len,
                             const LlamaSequenceConfig& seq_cfg);

    bool start_inference();

    uint32_t read_status() const;
    bool is_busy() const;
    bool has_error() const;
    uint32_t get_error_code() const;

    bool token_available() const;
    int32_t read_token();

    bool reset_device();

    // ------------------------------------------------------------------------
    // High-level convenience API
    // ------------------------------------------------------------------------

    // Fire-and-forget: run a prompt and get a text completion.
    std::string infer(const std::string& prompt,
                      const LlamaSequenceConfig& seq_cfg);

    // Streaming variant: callback per token-like chunk.
    using TokenCallback = std::function<void(const std::string&, int32_t, bool)>;

    bool infer_streaming(const std::string&         prompt,
                         const LlamaSequenceConfig& seq_cfg,
                         TokenCallback              on_token);

    // ------------------------------------------------------------------------
    // Next Steps: Advanced Features
    // ------------------------------------------------------------------------
    
    // KV-Cache Management
    bool enable_kv_cache();
    bool disable_kv_cache();
    bool is_kv_cache_enabled() const { return kv_cache_allocated_; }
    size_t get_kv_cache_size() const { return kv_cache_size_; }
    uint64_t get_kv_cache_addr() const { return kv_cache_addr_; }
    
    // GPU Offloading Control
    bool set_gpu_layers(int32_t n_layers);
    int32_t get_gpu_layers() const;
    bool is_gpu_active() const;
    
    // Interrupt Support
    bool enable_token_interrupts(int vector_index);
    bool disable_token_interrupts();
    bool are_interrupts_enabled() const { return llm_irq_vector >= 0; }
    
    // Batch Inference
    bool add_batch_request(const std::string& prompt, const LlamaSequenceConfig& cfg);
    bool process_batch();
    std::vector<std::string> get_batch_results();
    void clear_batch();
    size_t get_batch_queue_size() const { return batch_queue_.size(); }

    // ------------------------------------------------------------------------
    // Telemetry / diagnostics
    // ------------------------------------------------------------------------

    LlamaTelemetry get_telemetry() const {
        return telemetry;
    }

    std::string get_device_status_string() const;
    std::string get_hardware_diagnostics();
    bool run_self_test();
};

} // namespace vdev
} // namespace ggnucash
