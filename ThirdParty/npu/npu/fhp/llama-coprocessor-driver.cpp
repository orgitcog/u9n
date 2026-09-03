#include "llama-coprocessor-driver.h"
#include "llama-mock.h"  // Mock llama.cpp API - replace with real llama.h when available
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>

namespace ggnucash {
namespace vdev {

// Placeholder address used in stubbed implementation before SRAM buffer allocation
static constexpr uint64_t STUB_PROMPT_ADDR = 0;

// ============================================================================
// GGUF Runtime Wrapper
// ============================================================================
// This struct encapsulates the llama.cpp runtime state, allowing us to keep
// the header clean and swap implementations easily.
// ============================================================================

struct LlamaCoprocessorDriver::LlamaRuntime {
    llama::llama_model* model = nullptr;
    llama::llama_context* ctx = nullptr;
    llama::llama_model_params model_params;
    llama::llama_context_params ctx_params;
    bool initialized = false;

    LlamaRuntime() {
        model_params = llama::llama_model_default_params();
        ctx_params = llama::llama_context_default_params();
    }

    ~LlamaRuntime() {
        free();
    }

    bool load(const std::string& model_path, const LlamaModelConfig& cfg) {
        // Configure model parameters from our config
        model_params.n_gpu_layers = cfg.n_gpu_layers;
        model_params.use_mmap = true;
        model_params.use_mlock = false;

        // Load the model
        model = llama::llama_load_model_from_file(model_path.c_str(), model_params);
        if (!model) {
            std::cerr << "[GGUF-RT] Failed to load model: " << model_path << "\n";
            return false;
        }

        // Configure context parameters
        ctx_params.n_ctx = cfg.n_ctx;
        ctx_params.n_batch = cfg.batch_size;
        ctx_params.n_threads = cfg.n_threads;
        ctx_params.offload_kqv = cfg.offload_kv_cache;

        // Create context
        ctx = llama::llama_new_context_with_model(model, ctx_params);
        if (!ctx) {
            std::cerr << "[GGUF-RT] Failed to create context\n";
            llama::llama_free_model(model);
            model = nullptr;
            return false;
        }

        initialized = true;
        std::cout << "[GGUF-RT] Model loaded successfully: " << model_path << "\n";
        std::cout << "[GGUF-RT] Context size: " << llama::llama_n_ctx(ctx) << " tokens\n";
        return true;
    }

    void free() {
        if (ctx) {
            llama::llama_free(ctx);
            ctx = nullptr;
        }
        if (model) {
            llama::llama_free_model(model);
            model = nullptr;
        }
        initialized = false;
    }
};

// ============================================================================
// Internal helpers
// ============================================================================

bool LlamaCoprocessorDriver::map_register_region() {
    if (!pcb) return false;

    // Just ensure PERIPH region exists. We don't need a direct pointer because
    // we talk via VirtualPCB::read_memory/write_memory with absolute addrs.
    auto* periph = pcb->get_memory_region("PERIPH");
    return (periph != nullptr);
}

bool LlamaCoprocessorDriver::allocate_llm_sram() {
    if (!pcb) return false;

    // For now we simply verify that SRAM exists. We don't carve a dedicated
    // region yet; high-level infer() uses internal buffers.
    auto* sram = pcb->get_memory_region("SRAM");
    return (sram != nullptr);
}

void LlamaCoprocessorDriver::write_reg32(uint64_t addr, uint32_t value) {
    if (!pcb) return;

    for (int i = 0; i < 4; ++i) {
        uint8_t byte = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
        pcb->write_memory(addr + i, byte);
    }
}

uint32_t LlamaCoprocessorDriver::read_reg32(uint64_t addr) const {
    if (!pcb) return 0;

    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = pcb->read_memory(addr + i);
        value |= static_cast<uint32_t>(byte) << (8 * i);
    }
    return value;
}

void LlamaCoprocessorDriver::update_telemetry_from_hw() {
    // This function is reserved for future use when reading hardware performance
    // counters. In the current stub implementation, telemetry is computed from
    // timestamps rather than hardware registers.
}

// ============================================================================
// GGUF Runtime Integration Helpers
// ============================================================================

bool LlamaCoprocessorDriver::init_llama_runtime() {
    if (!llama_runtime_) {
        // Note: Using raw pointer to avoid incomplete type issues with forward declaration
        // Memory is properly managed: allocated here, freed in free_llama_runtime()
        // which is called in both destructor and remove()
        llama_runtime_ = new LlamaRuntime();
    }
    return llama_runtime_ != nullptr;
}

void LlamaCoprocessorDriver::free_llama_runtime() {
    if (llama_runtime_) {
        llama_runtime_->free();
        delete llama_runtime_;
        llama_runtime_ = nullptr;
    }
}

std::vector<int32_t> LlamaCoprocessorDriver::tokenize_prompt(const std::string& text) {
    if (!llama_runtime_ || !llama_runtime_->initialized) {
        std::cerr << "[GGUF-RT] Cannot tokenize: runtime not initialized\n";
        return {};
    }

    // Tokenize using llama.cpp
    auto tokens = llama::llama_tokenize(llama_runtime_->ctx, text, true);
    
    std::cout << "[GGUF-RT] Tokenized \"" << text.substr(0, 50) 
              << (text.length() > 50 ? "..." : "") 
              << "\" -> " << tokens.size() << " tokens\n";
    
    return tokens;
}

std::string LlamaCoprocessorDriver::detokenize_tokens(const std::vector<int32_t>& tokens) {
    if (!llama_runtime_ || !llama_runtime_->initialized) {
        std::cerr << "[GGUF-RT] Cannot detokenize: runtime not initialized\n";
        return "";
    }

    // Convert tokens back to text
    return llama::llama_detokenize(llama_runtime_->ctx, tokens);
}

std::vector<int32_t> LlamaCoprocessorDriver::generate_tokens(
    const std::vector<int32_t>& prompt_tokens, 
    int32_t n_predict) {
    
    if (!llama_runtime_ || !llama_runtime_->initialized) {
        std::cerr << "[GGUF-RT] Cannot generate: runtime not initialized\n";
        return {};
    }

    std::cout << "[GGUF-RT] Generating " << n_predict << " tokens...\n";
    
    // In real llama.cpp, this would use proper API calls:
    // 1. llama_decode() for prompt processing
    // 2. llama_get_logits() for token prediction
    // 3. Proper sampling with llama_sample_*() functions
    //
    // For the mock, we use the backend's generate function
    // which demonstrates the integration pattern
    auto result = llama::get_backend().generate(prompt_tokens, n_predict);
    
    std::cout << "[GGUF-RT] Generated " << result.size() << " tokens\n";
    return result;
}

// ============================================================================
// DeviceDriver interface
// ============================================================================

LlamaCoprocessorDriver::~LlamaCoprocessorDriver() {
    // Cleanup is handled by unique_ptr and destructor of LlamaRuntime
    free_llama_runtime();
}

bool LlamaCoprocessorDriver::load(VirtualPCB* pcb_) {
    pcb = pcb_;
    if (!pcb) return false;

    if (!map_register_region()) {
        std::cerr << "[LLM-DRV] PERIPH region not available\n";
        return false;
    }

    if (!allocate_llm_sram()) {
        std::cerr << "[LLM-DRV] SRAM region not available\n";
        return false;
    }

    // Reset our "hardware" registers to sane defaults.
    write_reg32(REG_CMD,    CMD_NONE);
    write_reg32(REG_STATUS, STATUS_IDLE);
    write_reg32(REG_MODEL_ID, model_id);
    write_reg32(REG_ERROR_CODE, 0);
    write_reg32(REG_PERF_TOKENS_SEC, 0);
    
    // Initialize Next Steps registers
    write_reg32(REG_GPU_LAYERS, 0);
    write_reg32(REG_KV_CACHE_ADDR, 0);
    write_reg32(REG_KV_CACHE_SIZE, 0);
    write_reg32(REG_BATCH_SIZE, 0);
    write_reg32(REG_BATCH_IDX, 0);

    model_loaded  = false;
    device_online = false;
    loaded = true;
    
    // Initialize Next Steps state
    kv_cache_allocated_ = false;
    kv_cache_addr_ = 0;
    kv_cache_size_ = 0;
    current_batch_idx_ = 0;
    batch_queue_.clear();

    telemetry = LlamaTelemetry{};  // reset struct
    telemetry.last_inference_start = std::chrono::steady_clock::now();
    telemetry.last_inference_end   = telemetry.last_inference_start;

    return true;
}

bool LlamaCoprocessorDriver::initialize() {
    if (!pcb) return false;

    // In a real device you might perform hardware self-test here.
    device_online = true;
    initialized = true;

    uint32_t status = read_reg32(REG_STATUS);
    status &= ~(STATUS_ERROR | STATUS_BUSY);
    status |= STATUS_IDLE;
    write_reg32(REG_STATUS, status);

    return true;
}

bool LlamaCoprocessorDriver::probe() {
    if (!pcb) return false;

    // Simple probe: check that PERIPH exists and base register is readable.
    auto* periph = pcb->get_memory_region("PERIPH");
    if (!periph) return false;

    (void)read_reg32(REG_STATUS);
    return true;
}

bool LlamaCoprocessorDriver::remove() {
    // Clean up GGUF runtime first
    free_llama_runtime();
    
    // Clean up Next Steps resources
    free_kv_cache();
    clear_batch();
    disable_token_interrupts();
    
    // Tear down state; we leave PCB memory as-is.
    model_loaded  = false;
    device_online = false;
    loaded = false;
    initialized = false;
    pcb           = nullptr;
    return true;
}

// ============================================================================
// Model / configuration operations
// ============================================================================

bool LlamaCoprocessorDriver::load_model() {
    if (!pcb || !device_online) {
        std::cerr << "[LLM-DRV] Cannot load model: device not ready\n";
        return false;
    }

    if (model_cfg.model_path.empty()) {
        std::cerr << "[LLM-DRV] Cannot load model: model_path is empty\n";
        write_reg32(REG_ERROR_CODE, 1); // arbitrary error
        return false;
    }

    // Initialize GGUF runtime if not already done
    if (!init_llama_runtime()) {
        std::cerr << "[LLM-DRV] Failed to initialize llama runtime\n";
        write_reg32(REG_ERROR_CODE, 2);
        return false;
    }

    // Load the actual GGUF model
    std::cout << "[LLM-DRV] Loading GGUF model: " << model_cfg.model_path << "\n";
    if (!llama_runtime_->load(model_cfg.model_path, model_cfg)) {
        std::cerr << "[LLM-DRV] Failed to load GGUF model\n";
        write_reg32(REG_ERROR_CODE, 3);
        
        uint32_t status = read_reg32(REG_STATUS);
        status |= STATUS_ERROR;
        status &= ~STATUS_MODEL_READY;
        write_reg32(REG_STATUS, status);
        
        return false;
    }

    model_loaded = true;

    // Update hardware registers to reflect model state
    write_reg32(REG_MODEL_ID, model_id);
    write_reg32(REG_CTX_USED, llama::llama_n_ctx(llama_runtime_->ctx));

    uint32_t status = read_reg32(REG_STATUS);
    status |= STATUS_MODEL_READY;
    status &= ~STATUS_ERROR;
    write_reg32(REG_STATUS, status);

    std::cout << "[LLM-DRV] Model loaded successfully (GGUF runtime active)\n";
    std::cout << "[LLM-DRV] Context window: " << llama::llama_n_ctx(llama_runtime_->ctx) << " tokens\n";
    return true;
}

// ============================================================================
// Low-level hardware-style API
// ============================================================================

bool LlamaCoprocessorDriver::configure_inference(uint64_t prompt_addr,
                                                 uint32_t prompt_len,
                                                 const LlamaSequenceConfig& seq_cfg) {
    if (!pcb || !device_online || !model_loaded) {
        std::cerr << "[LLM-DRV] configure_inference: device/model not ready\n";
        return false;
    }

    write_reg32(REG_PROMPT_ADDR, static_cast<uint32_t>(prompt_addr));
    write_reg32(REG_PROMPT_LEN,  prompt_len);
    write_reg32(REG_N_PREDICT,   static_cast<uint32_t>(seq_cfg.n_predict));

    // Very rough: remember how many "tokens" we think we saw
    telemetry.last_prompt_tokens = prompt_len;
    return true;
}

bool LlamaCoprocessorDriver::start_inference() {
    if (!pcb || !device_online || !model_loaded) {
        std::cerr << "[LLM-DRV] start_inference: device/model not ready\n";
        return false;
    }

    uint32_t cmd = read_reg32(REG_CMD);
    cmd |= CMD_START_INF;
    write_reg32(REG_CMD, cmd);

    uint32_t status = read_reg32(REG_STATUS);
    status |= STATUS_BUSY;
    status &= ~STATUS_EOG;
    status &= ~STATUS_ERROR;
    write_reg32(REG_STATUS, status);

    telemetry.last_inference_start = std::chrono::steady_clock::now();
    return true;
}

uint32_t LlamaCoprocessorDriver::read_status() const {
    return read_reg32(REG_STATUS);
}

bool LlamaCoprocessorDriver::is_busy() const {
    return (read_status() & STATUS_BUSY) != 0;
}

bool LlamaCoprocessorDriver::has_error() const {
    return (read_status() & STATUS_ERROR) != 0;
}

uint32_t LlamaCoprocessorDriver::get_error_code() const {
    return read_reg32(REG_ERROR_CODE);
}

bool LlamaCoprocessorDriver::token_available() const {
    // In a full implementation this would check STATUS_TOKEN_READY or a FIFO.
    // Stub: no MMIO token path is implemented yet.
    return false;
}

int32_t LlamaCoprocessorDriver::read_token() {
    // Check if a token is available
    if (!token_available()) {
        return -1;
    }

    // Read the token from the output register
    int32_t token = static_cast<int32_t>(read_reg32(REG_TOKEN_OUT));

    // Clear the TOKEN_READY flag to indicate token has been consumed
    uint32_t status = read_reg32(REG_STATUS);
    status &= ~STATUS_TOKEN_READY;
    write_reg32(REG_STATUS, status);

    return token;
}

bool LlamaCoprocessorDriver::reset_device() {
    if (!pcb) return false;

    write_reg32(REG_CMD, CMD_RESET);
    write_reg32(REG_ERROR_CODE, 0);
    write_reg32(REG_PERF_TOKENS_SEC, 0);

    uint32_t status = STATUS_IDLE;
    if (model_loaded) {
        status |= STATUS_MODEL_READY;
    }
    write_reg32(REG_STATUS, status);

    return true;
}

// ============================================================================
// High-level convenience API
// ============================================================================

std::string LlamaCoprocessorDriver::infer(const std::string&         prompt,
                                          const LlamaSequenceConfig& seq_cfg) {
    if (!pcb || !device_online) {
        return "[LLM-DRV] Device not online";
    }
    if (!model_loaded) {
        return "[LLM-DRV] Model not loaded";
    }
    if (!llama_runtime_ || !llama_runtime_->initialized) {
        return "[LLM-DRV] GGUF runtime not initialized";
    }

    // === STEP 1: TOKENIZATION ===
    std::cout << "[LLM-DRV] Tokenizing prompt...\n";
    auto prompt_tokens = tokenize_prompt(prompt);
    if (prompt_tokens.empty()) {
        write_reg32(REG_ERROR_CODE, 4); // Tokenization error
        return "[LLM-DRV] Failed to tokenize prompt";
    }

    last_prompt_tokens = prompt_tokens;
    telemetry.last_prompt_tokens = prompt_tokens.size();

    // Configure hardware registers with actual token count
    configure_inference(STUB_PROMPT_ADDR, static_cast<uint32_t>(prompt_tokens.size()), seq_cfg);
    start_inference();

    // === STEP 2: INFERENCE LOOP ===
    std::cout << "[LLM-DRV] Running inference (n_predict=" << seq_cfg.n_predict << ")...\n";
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Generate tokens using GGUF runtime
    auto generated_tokens = generate_tokens(prompt_tokens, seq_cfg.n_predict);
    
    if (generated_tokens.empty()) {
        write_reg32(REG_ERROR_CODE, 5); // Generation error
        uint32_t status = read_reg32(REG_STATUS);
        status |= STATUS_ERROR;
        status &= ~STATUS_BUSY;
        write_reg32(REG_STATUS, status);
        return "[LLM-DRV] Failed to generate tokens";
    }

    last_output_tokens = generated_tokens;
    telemetry.last_completion_tokens = generated_tokens.size();
    telemetry.total_tokens_generated += generated_tokens.size();

    // === STEP 3: DETOKENIZATION ===
    std::cout << "[LLM-DRV] Detokenizing output...\n";
    std::string completion = detokenize_tokens(generated_tokens);

    // === STEP 4: UPDATE TELEMETRY ===
    auto end_time = std::chrono::steady_clock::now();
    telemetry.last_inference_start = start_time;
    telemetry.last_inference_end = end_time;
    telemetry.total_prompts++;

    auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    if (dt_ms > 0) {
        telemetry.tokens_per_second =
            static_cast<double>(generated_tokens.size()) / (static_cast<double>(dt_ms) / 1000.0);
    } else {
        telemetry.tokens_per_second = 0.0;
    }

    // === STEP 5: UPDATE HARDWARE REGISTERS ===
    // Reflect completion status in hardware
    uint32_t status = read_reg32(REG_STATUS);
    status &= ~STATUS_BUSY;
    status |= STATUS_EOG;
    write_reg32(REG_STATUS, status);

    // Write performance counter (tokens/sec as milli-units)
    uint32_t perf_milli = static_cast<uint32_t>(telemetry.tokens_per_second * 1000.0);
    write_reg32(REG_PERF_TOKENS_SEC, perf_milli);
    
    // Write context usage
    write_reg32(REG_CTX_USED, prompt_tokens.size() + generated_tokens.size());

    std::cout << "[LLM-DRV] Inference complete (" << generated_tokens.size() 
              << " tokens, " << telemetry.tokens_per_second << " tok/s)\n";

    // Format output
    std::ostringstream oss;
    if (!seq_cfg.system_prompt.empty()) {
        oss << "System: " << seq_cfg.system_prompt << "\n";
    }
    if (seq_cfg.echo_prompt) {
        oss << "Prompt: " << prompt << "\n";
        oss << "---\n";
    }
    oss << completion;

    return oss.str();
}

bool LlamaCoprocessorDriver::infer_streaming(const std::string&         prompt,
                                             const LlamaSequenceConfig& seq_cfg,
                                             TokenCallback              on_token) {
    if (!on_token) {
        return false;
    }

    // Use the non-streaming infer() and then break output into "tokens".
    std::string full = infer(prompt, seq_cfg);

    std::istringstream iss(full);
    std::string token;
    std::vector<std::string> tokens;
    while (iss >> token) {
        tokens.push_back(token);
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
        bool is_last = (i + 1 == tokens.size());
        on_token(tokens[i], static_cast<int32_t>(i), is_last);

        if (seq_cfg.stream_tokens && !is_last) {
            // Configurable delay to simulate streaming
            std::this_thread::sleep_for(std::chrono::milliseconds(seq_cfg.stream_delay_ms));
        }
    }

    return true;
}

// ============================================================================
// Telemetry / diagnostics
// ============================================================================

std::string LlamaCoprocessorDriver::get_device_status_string() const {
    std::ostringstream oss;

    uint32_t status = read_status();

    oss << "LLM-Coprocessor[model_id=0x"
        << std::hex << model_id << std::dec << "] ";

    if (!device_online) {
        oss << "OFFLINE";
        return oss.str();
    }

    oss << "Status: ";
    if (status & STATUS_ERROR) {
        oss << "ERROR ";
    } else if (status & STATUS_BUSY) {
        oss << "BUSY ";
    } else if (status & STATUS_MODEL_READY) {
        oss << "READY ";
    } else {
        oss << "IDLE ";
    }

    if (status & STATUS_EOG) {
        oss << "(EOG) ";
    }

    oss << "| tokens/s=" << telemetry.tokens_per_second
        << " total_tokens=" << telemetry.total_tokens_generated
        << " total_prompts=" << telemetry.total_prompts;

    return oss.str();
}

std::string LlamaCoprocessorDriver::get_hardware_diagnostics() {
    std::ostringstream oss;

    oss << "LLM Co-Processor Diagnostics\n";
    oss << "----------------------------\n";
    oss << "Device online    : " << (device_online ? "yes" : "no") << "\n";
    oss << "Model loaded     : " << (model_loaded ? "yes" : "no") << "\n";
    oss << "Model path       : " << model_cfg.model_path << "\n";
    oss << "Context (n_ctx)  : " << model_cfg.n_ctx << "\n";
    oss << "Threads          : " << model_cfg.n_threads << "\n";
    oss << "GPU layers       : " << model_cfg.n_gpu_layers << "\n";

    uint32_t status = read_status();
    uint32_t err    = get_error_code();

    oss << "Raw status reg   : 0x" << std::hex << status << std::dec << "\n";
    oss << "Error code       : " << err << "\n";

    oss << "Telemetry:\n";
    oss << "  total_prompts        : " << telemetry.total_prompts << "\n";
    oss << "  total_tokens_generated: " << telemetry.total_tokens_generated << "\n";
    oss << "  last_prompt_tokens   : " << telemetry.last_prompt_tokens << "\n";
    oss << "  last_completion_tokens: " << telemetry.last_completion_tokens << "\n";
    oss << "  tokens_per_second    : " << telemetry.tokens_per_second << "\n";

    return oss.str();
}

bool LlamaCoprocessorDriver::run_self_test() {
    if (!pcb) return false;

    std::cout << "[LLM-DRV] Running self-test...\n";

    LlamaSequenceConfig seq;
    seq.n_predict   = 16;
    seq.echo_prompt = true;

    std::string result = infer("LLM self-test prompt", seq);
    if (result.empty()) {
        write_reg32(REG_ERROR_CODE, 2);
        return false;
    }

    uint32_t status = read_status();
    if (status & STATUS_ERROR) {
        return false;
    }

    std::cout << "[LLM-DRV] Self-test completed\n";
    return true;
}

// ============================================================================
// Next Steps: Model Hot-Swapping
// ============================================================================

bool LlamaCoprocessorDriver::unload_model() {
    if (!pcb || !device_online) {
        std::cerr << "[LLM-DRV] unload_model: device not ready\n";
        return false;
    }
    
    if (!model_loaded) {
        std::cerr << "[LLM-DRV] No model to unload\n";
        return false;
    }
    
    std::cout << "[LLM-DRV] Unloading model: " << model_cfg.model_path << "\n";
    
    // Free GGUF runtime
    free_llama_runtime();
    
    // Free KV-cache if allocated
    if (kv_cache_allocated_) {
        free_kv_cache();
    }
    
    // Update state
    model_loaded = false;
    
    // Update hardware registers
    write_reg32(REG_CMD, CMD_UNLOAD_MODEL);
    uint32_t status = read_reg32(REG_STATUS);
    status &= ~STATUS_MODEL_READY;
    status &= ~STATUS_KV_CACHED;
    status &= ~STATUS_GPU_ACTIVE;
    write_reg32(REG_STATUS, status);
    
    std::cout << "[LLM-DRV] Model unloaded successfully\n";
    return true;
}

bool LlamaCoprocessorDriver::switch_model(const LlamaModelConfig& new_cfg) {
    std::cout << "[LLM-DRV] Switching model from " << model_cfg.model_path 
              << " to " << new_cfg.model_path << "\n";
    
    // Unload current model
    if (!unload_model()) {
        std::cerr << "[LLM-DRV] Failed to unload current model\n";
        return false;
    }
    
    // Set new configuration
    set_model_config(new_cfg);
    
    // Load new model
    if (!load_model()) {
        std::cerr << "[LLM-DRV] Failed to load new model\n";
        write_reg32(REG_ERROR_CODE, 3);
        return false;
    }
    
    std::cout << "[LLM-DRV] Model switched successfully\n";
    return true;
}

// ============================================================================
// Next Steps: KV-Cache Management
// ============================================================================

size_t LlamaCoprocessorDriver::calculate_kv_cache_size() const {
    // Calculate KV-cache size based on context and model parameters
    // Formula: n_ctx * n_layers * (key_size + value_size)
    // For mock: approximate size based on context
    size_t base_size = model_cfg.n_ctx * BYTES_PER_CONTEXT_TOKEN;
    return base_size;
}

bool LlamaCoprocessorDriver::allocate_kv_cache(size_t size) {
    if (!pcb) return false;
    
    auto* sram = pcb->get_memory_region("SRAM");
    if (!sram) {
        std::cerr << "[LLM-DRV] SRAM region not available for KV-cache\n";
        return false;
    }
    
    // Allocate KV-cache in SRAM region at defined offset
    kv_cache_addr_ = KV_CACHE_SRAM_OFFSET;
    kv_cache_size_ = size;
    kv_cache_allocated_ = true;
    
    // Update hardware registers
    write_reg32(REG_KV_CACHE_ADDR, static_cast<uint32_t>(kv_cache_addr_));
    write_reg32(REG_KV_CACHE_SIZE, static_cast<uint32_t>(kv_cache_size_));
    write_reg32(REG_CMD, CMD_ALLOC_KVCACHE);
    
    uint32_t status = read_reg32(REG_STATUS);
    status |= STATUS_KV_CACHED;
    write_reg32(REG_STATUS, status);
    
    std::cout << "[LLM-DRV] KV-cache allocated: " << kv_cache_size_ 
              << " bytes at 0x" << std::hex << kv_cache_addr_ << std::dec << "\n";
    
    return true;
}

void LlamaCoprocessorDriver::free_kv_cache() {
    if (!kv_cache_allocated_) return;
    
    std::cout << "[LLM-DRV] Freeing KV-cache\n";
    
    kv_cache_addr_ = 0;
    kv_cache_size_ = 0;
    kv_cache_allocated_ = false;
    
    // Update hardware registers
    write_reg32(REG_KV_CACHE_ADDR, 0);
    write_reg32(REG_KV_CACHE_SIZE, 0);
    
    uint32_t status = read_reg32(REG_STATUS);
    status &= ~STATUS_KV_CACHED;
    write_reg32(REG_STATUS, status);
}

bool LlamaCoprocessorDriver::enable_kv_cache() {
    if (!model_loaded) {
        std::cerr << "[LLM-DRV] Cannot enable KV-cache: no model loaded\n";
        return false;
    }
    
    if (kv_cache_allocated_) {
        std::cout << "[LLM-DRV] KV-cache already enabled\n";
        return true;
    }
    
    size_t cache_size = calculate_kv_cache_size();
    return allocate_kv_cache(cache_size);
}

bool LlamaCoprocessorDriver::disable_kv_cache() {
    free_kv_cache();
    return true;
}

// ============================================================================
// Next Steps: GPU Offloading Control
// ============================================================================

bool LlamaCoprocessorDriver::set_gpu_layers(int32_t n_layers) {
    if (n_layers < 0) {
        std::cerr << "[LLM-DRV] Invalid GPU layer count: " << n_layers << "\n";
        return false;
    }
    
    std::cout << "[LLM-DRV] Setting GPU layers: " << n_layers << "\n";
    
    // Update model configuration
    model_cfg.n_gpu_layers = n_layers;
    
    // Update hardware register
    write_reg32(REG_GPU_LAYERS, static_cast<uint32_t>(n_layers));
    
    // Update status
    uint32_t status = read_reg32(REG_STATUS);
    if (n_layers > 0) {
        status |= STATUS_GPU_ACTIVE;
    } else {
        status &= ~STATUS_GPU_ACTIVE;
    }
    write_reg32(REG_STATUS, status);
    
    // If model is loaded, may need to reload with new GPU settings
    if (model_loaded) {
        std::cout << "[LLM-DRV] Note: GPU layer change will take effect on next model load\n";
    }
    
    return true;
}

int32_t LlamaCoprocessorDriver::get_gpu_layers() const {
    return static_cast<int32_t>(read_reg32(REG_GPU_LAYERS));
}

bool LlamaCoprocessorDriver::is_gpu_active() const {
    return (read_status() & STATUS_GPU_ACTIVE) != 0;
}

// ============================================================================
// Next Steps: Interrupt Support
// ============================================================================

void LlamaCoprocessorDriver::configure_interrupt_vector(int vector_index) {
    llm_irq_vector = vector_index;
    std::cout << "[LLM-DRV] Interrupt vector configured: " << vector_index << "\n";
}

void LlamaCoprocessorDriver::raise_token_ready_interrupt() {
    if (llm_irq_vector < 0 || !pcb) return;
    
    // Set TOKEN_READY flag
    uint32_t status = read_reg32(REG_STATUS);
    status |= STATUS_TOKEN_READY;
    write_reg32(REG_STATUS, status);
    
    // Raise interrupt via the PCB's interrupt controller
    auto* irq_ctrl = pcb->get_interrupt_controller();
    if (irq_ctrl) {
        irq_ctrl->trigger_interrupt(static_cast<uint32_t>(llm_irq_vector));
    }
    
    // Note: interrupt flag will be cleared when token is read
}

bool LlamaCoprocessorDriver::enable_token_interrupts(int vector_index) {
    if (!pcb || !device_online) {
        std::cerr << "[LLM-DRV] Cannot enable interrupts: device not ready\n";
        return false;
    }
    
    configure_interrupt_vector(vector_index);
    std::cout << "[LLM-DRV] Token interrupts enabled on vector " << vector_index << "\n";
    return true;
}

bool LlamaCoprocessorDriver::disable_token_interrupts() {
    llm_irq_vector = -1;
    std::cout << "[LLM-DRV] Token interrupts disabled\n";
    return true;
}

// ============================================================================
// Next Steps: Batch Inference
// ============================================================================

bool LlamaCoprocessorDriver::add_batch_request(const std::string& prompt, 
                                               const LlamaSequenceConfig& cfg) {
    if (!model_loaded) {
        std::cerr << "[LLM-DRV] Cannot add batch request: no model loaded\n";
        return false;
    }
    
    BatchRequest req;
    req.prompt = prompt;
    req.config = cfg;
    req.completed = false;
    
    batch_queue_.push_back(req);
    
    // Update hardware register
    write_reg32(REG_BATCH_SIZE, static_cast<uint32_t>(batch_queue_.size()));
    
    std::cout << "[LLM-DRV] Added batch request (queue size: " 
              << batch_queue_.size() << ")\n";
    
    return true;
}

bool LlamaCoprocessorDriver::process_batch() {
    if (batch_queue_.empty()) {
        std::cout << "[LLM-DRV] No batch requests to process\n";
        return true;
    }
    
    std::cout << "[LLM-DRV] Processing batch: " << batch_queue_.size() 
              << " requests\n";
    
    current_batch_idx_ = 0;
    for (auto& req : batch_queue_) {
        write_reg32(REG_BATCH_IDX, static_cast<uint32_t>(current_batch_idx_));
        
        std::cout << "[LLM-DRV] Processing batch item " << current_batch_idx_ 
                  << "/" << batch_queue_.size() << "\n";
        
        // Run inference
        req.result = infer(req.prompt, req.config);
        req.completed = true;
        
        current_batch_idx_++;
    }
    
    std::cout << "[LLM-DRV] Batch processing complete\n";
    return true;
}

std::vector<std::string> LlamaCoprocessorDriver::get_batch_results() {
    std::vector<std::string> results;
    for (const auto& req : batch_queue_) {
        if (req.completed) {
            results.push_back(req.result);
        }
    }
    return results;
}

void LlamaCoprocessorDriver::clear_batch() {
    batch_queue_.clear();
    current_batch_idx_ = 0;
    write_reg32(REG_BATCH_SIZE, 0);
    write_reg32(REG_BATCH_IDX, 0);
    std::cout << "[LLM-DRV] Batch queue cleared\n";
}

} // namespace vdev
} // namespace ggnucash
