Alright, let’s actually bolt this thing onto the board.

I’m going to give you:

1. A slightly cleaned-up **`llama-coprocessor-driver.h`** (very close to the previous sketch, just removing unused `MemoryRegion` members and adding missing includes).
2. A **`llama-coprocessor-driver.cpp`** that:

   * Talks to the virtual PERIPH space via `VirtualPCB::read_memory` / `write_memory`
   * Checks for `SRAM`/`PERIPH` regions so it feels like real hardware
   * Implements a **stubbed** “LLM” path (`infer()` just echoes + annotates the prompt) so you can plug it in without needing llama.cpp yet

You’ll get a compilable skeleton that **pretends there’s a GGUF NPU** hanging off PERIPH. Later you swap the stubbed inference with real llama.cpp / GGUF calls.

---

## 1) `llama-coprocessor-driver.h`

Drop this next to `financial-device-driver.h`.

```cpp
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

    // Command bits
    enum CommandBits : uint32_t {
        CMD_NONE        = 0,
        CMD_RESET       = 1u << 0,
        CMD_LOAD_MODEL  = 1u << 1,
        CMD_START_INF   = 1u << 2,
        CMD_SOFT_STOP   = 1u << 3,
    };

    // Status bits
    enum StatusBits : uint32_t {
        STATUS_IDLE        = 0,
        STATUS_BUSY        = 1u << 0,
        STATUS_EOG         = 1u << 1,  // End-of-generation
        STATUS_ERROR       = 1u << 2,
        STATUS_MODEL_READY = 1u << 3,
        STATUS_TOKEN_READY = 1u << 4,
    };

private:
    VirtualPCB* pcb = nullptr;

    // Bookkeeping
    LlamaModelConfig model_cfg;
    LlamaTelemetry   telemetry;
    bool             model_loaded   = false;
    bool             device_online  = false;
    uint32_t         model_id       = 0xLLM0001; // arbitrary ID for now

    // Internal buffers for stubbed/demo behaviour
    std::vector<int32_t> last_prompt_tokens;
    std::vector<int32_t> last_output_tokens;

    // Interrupt vector index for "LLM_DONE" or "TOKEN_READY" (not wired yet)
    int llm_irq_vector = -1;

    // Internal helpers
    bool map_register_region();
    bool allocate_llm_sram();
    void update_telemetry_from_hw();
    void write_reg32(uint64_t addr, uint32_t value);
    uint32_t read_reg32(uint64_t addr) const;

public:
    // ------------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------------
    LlamaCoprocessorDriver()
        : DeviceDriver("GGNuCash-LLM-Coprocessor", "0.1.0") {}

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
```

---

## 2) `llama-coprocessor-driver.cpp`

This is the stubbed implementation. It:

* Verifies that `SRAM` and `PERIPH` regions exist on the `VirtualPCB`
* Reads/writes 32-bit “registers” via `VirtualPCB::read_memory`/`write_memory`
* Pretends to “load a model” by setting `STATUS_MODEL_READY`
* Implements `infer()` as an *echoing stub* that still updates telemetry and status bits

```cpp
#include "llama-coprocessor-driver.h"
#include <sstream>
#include <iostream>
#include <thread>

namespace ggnucash {
namespace vdev {

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
    // Optional: read REG_PERF_TOKENS_SEC and map to tokens_per_second.
    // In this stub implementation, we prefer to compute it from timestamps,
    // so this is effectively a no-op.
    (void)0;
}

// ============================================================================
// DeviceDriver interface
// ============================================================================

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

    model_loaded  = false;
    device_online = false;

    telemetry = LlamaTelemetry{};  // reset struct
    telemetry.last_inference_start = std::chrono::steady_clock::now();
    telemetry.last_inference_end   = telemetry.last_inference_start;

    return true;
}

bool LlamaCoprocessorDriver::initialize() {
    if (!pcb) return false;

    // In a real device you might perform hardware self-test here.
    device_online = true;

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
    // Tear down state; we leave PCB memory as-is.
    model_loaded  = false;
    device_online = false;
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

    // Real implementation would call into llama.cpp / GGUF runtime here.
    // For now, we just mark as loaded and set STATUS_MODEL_READY.
    model_loaded = true;

    write_reg32(REG_MODEL_ID, model_id);

    uint32_t status = read_reg32(REG_STATUS);
    status |= STATUS_MODEL_READY;
    status &= ~STATUS_ERROR;
    write_reg32(REG_STATUS, status);

    std::cout << "[LLM-DRV] Model loaded (stub): " << model_cfg.model_path << "\n";
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
    // Stub: no real token stream. Return sentinel.
    if (!token_available()) {
        return -1;
    }
    return -1;
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

    // Kick the low-level path for realism (even though we don't consume SRAM).
    configure_inference(0 /*dummy addr*/, static_cast<uint32_t>(prompt.size()), seq_cfg);
    start_inference();

    last_prompt_tokens.clear();
    last_output_tokens.clear();

    // --- STUB COMPLETION ---
    // Real implementation would:
    //   - tokenize prompt
    //   - stream tokens into HW
    //   - read tokens from TOKEN_OUT
    //   - de-tokenize to text
    //
    // For now, we construct a simple echo-style completion.
    std::ostringstream oss;
    oss << "[LLM-COPROC STUB] n_predict=" << seq_cfg.n_predict << "\n";
    if (!seq_cfg.system_prompt.empty()) {
        oss << "System: " << seq_cfg.system_prompt << "\n";
    }
    if (seq_cfg.echo_prompt) {
        oss << "Prompt: " << prompt << "\n";
    } else {
        oss << "Prompt length: " << prompt.size() << " bytes\n";
    }
    oss << "Completion: (stubbed – connect GGUF runtime here)\n";

    std::string completion = oss.str();

    // Update telemetry
    telemetry.last_inference_end = std::chrono::steady_clock::now();
    telemetry.total_prompts++;

    // Approximate "tokens" as words in completion
    std::istringstream wss(completion);
    std::string word;
    uint64_t token_count = 0;
    while (wss >> word) {
        token_count++;
    }

    telemetry.last_completion_tokens = token_count;
    telemetry.total_tokens_generated += token_count;

    auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        telemetry.last_inference_end - telemetry.last_inference_start).count();
    if (dt_ms > 0) {
        telemetry.tokens_per_second =
            static_cast<double>(token_count) / (static_cast<double>(dt_ms) / 1000.0);
    } else {
        telemetry.tokens_per_second = 0.0;
    }

    // Reflect status back into "hardware" regs.
    uint32_t status = read_reg32(REG_STATUS);
    status &= ~STATUS_BUSY;
    status |= STATUS_EOG;
    write_reg32(REG_STATUS, status);

    // Optionally mirror TPS into REG_PERF_TOKENS_SEC as milli-tokens/sec
    uint32_t perf_milli = static_cast<uint32_t>(telemetry.tokens_per_second * 1000.0);
    write_reg32(REG_PERF_TOKENS_SEC, perf_milli);

    return completion;
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

        if (seq_cfg.stream_tokens) {
            // Small delay to look like streaming, optional.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
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

} // namespace vdev
} // namespace ggnucash
```

---

## 3) How to hook it up next to the financial driver

Once you’ve got those two files in your project and added `llama-coprocessor-driver.cpp` to your CMake, you can do something like this in `device-admin.cpp` (or a new admin tool):

```cpp
#include "llama-coprocessor-driver.h"

// ...

auto llm_driver = std::make_shared<LlamaCoprocessorDriver>();

if (!llm_driver->load(device.get()) ||
    !llm_driver->initialize()) {
    std::cerr << "Failed to init LLM co-processor\n";
} else {
    LlamaModelConfig mcfg;
    mcfg.model_path = "models/demo.gguf";
    mcfg.n_ctx      = 4096;
    llm_driver->set_model_config(mcfg);
    llm_driver->load_model();

    LlamaSequenceConfig scfg;
    scfg.n_predict   = 64;
    scfg.echo_prompt = true;

    std::cout << llm_driver->infer("Test prompt via virtual NPU", scfg) << "\n";
    std::cout << llm_driver->get_device_status_string() << "\n";
}
```

Right now the “inference” is just a stubby echo, but the plumbing is **fully hardware-shaped**:

* MMIO registers in PERIPH
* Status + error registers
* Telemetry and diagnostics
* High-level `infer()` / `infer_streaming()` APIs

Next step, when you’re ready, we can swap out the stubbed `infer()` body with actual llama.cpp + GGUF wiring, and decide how much of the real kv-cache / tensor traffic you want to expose as “hardware” vs “magic black box”.
