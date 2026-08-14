Cool, let’s bolt this “LLM NPU” onto your board.

Below is a **header-only sketch** for `llama-coprocessor-driver.h` that:

* Lives next to `financial-device-driver.h`
* Uses the same style (`ggnucash::vdev`, `DeviceDriver` base)
* Pretends there’s a GGUF-backed NPU memory-mapped in `PERIPH`
* Gives you both a low-level MMIO-style API and a high-level `infer()` call

You can tune addresses / bits later; this is the scaffolding.

---

```cpp
#pragma once

#include "virtual-device.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <chrono>
#include <map>

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
//   - Prompts are written into SRAM / local buffers
//   - Inference is kicked by writing command registers
//   - Tokens are read out of TOKEN_OUT or buffered in software
//
// This header intentionally does NOT depend on llama.cpp types; the backing
// implementation can use any runtime to execute the GGUF graph.
// ============================================================================

// ----------------------------------------------------------------------------
// Model configuration and telemetry structs
// ----------------------------------------------------------------------------

struct LlamaModelConfig {
    std::string model_path;      // .gguf file path
    std::string model_name;      // Friendly name
    int32_t     n_ctx      = 4096;
    int32_t     n_threads  = 4;
    int32_t     n_gpu_layers = 0;   // 0 = CPU only
    int32_t     batch_size = 1;
    bool        offload_kv_cache = false;
    bool        low_vram_mode    = false;
};

struct LlamaTelemetry {
    double tokens_per_second      = 0.0;
    uint64_t total_tokens_generated = 0;
    uint64_t total_prompts          = 0;
    uint64_t last_prompt_tokens     = 0;
    uint64_t last_completion_tokens = 0;
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
    // Memory-mapped register layout
    // ------------------------------------------------------------------------
    // NOTE: The FinancialDeviceDriver already grabs 0x4000_0000 region.
    // This device is offset to 0x4000_1000 inside PERIPH.
    // Adjust if you want a different map.
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

    // Diagnostics / error handling
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

    // Backing memory for command/telemetry region (may be part of PERIPH)
    std::shared_ptr<MemoryRegion> llm_reg_region;

    // Optional dedicated SRAM region for prompts / kv-cache
    std::shared_ptr<MemoryRegion> llm_sram_region;

    // Bookkeeping
    LlamaModelConfig model_cfg;
    LlamaTelemetry   telemetry;
    bool             model_loaded   = false;
    bool             device_online  = false;
    uint32_t         model_id       = 0xLLM0001; // arbitrary ID for now

    // Optional: simple in-driver buffers for test/demo mode
    std::vector<int32_t> last_prompt_tokens;
    std::vector<int32_t> last_output_tokens;

    // Interrupt vector index for "LLM_DONE" or "TOKEN_READY" if used
    int llm_irq_vector = -1;

    // Internal helpers (implemented in .cpp)
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

    // Set up basic model configuration; does not necessarily load GGUF yet.
    void set_model_config(const LlamaModelConfig& cfg) {
        model_cfg = cfg;
    }

    const LlamaModelConfig& get_model_config() const {
        return model_cfg;
    }

    // Kick hardware-side model load (REG_CMD |= CMD_LOAD_MODEL).
    // Implementation may either:
    //  - actually load GGUF via llama.cpp, or
    //  - mark model as "dummy ready" for pure-virtual testing.
    bool load_model();

    bool is_model_loaded() const {
        return model_loaded;
    }

    // ------------------------------------------------------------------------
    // Low-level hardware-style API (MMIO abstraction)
    // ------------------------------------------------------------------------

    // Configure a single inference pass (prompt already in SRAM).
    bool configure_inference(uint64_t prompt_addr,
                             uint32_t prompt_len,
                             const LlamaSequenceConfig& seq_cfg);

    // Start inference by asserting CMD_START_INF.
    bool start_inference();

    // Check raw status flags.
    uint32_t read_status() const;
    bool is_busy() const;
    bool has_error() const;
    uint32_t get_error_code() const;

    // Poll token-ready flag and read a single token from TOKEN_OUT.
    bool token_available() const;
    int32_t read_token();

    // Clear EOG/ERROR/soft-stop state (typically via CMD_RESET).
    bool reset_device();

    // ------------------------------------------------------------------------
    // High-level convenience API
    // ------------------------------------------------------------------------

    // Simple "fire-and-forget" call: run a prompt and get a text completion.
    // Implementation will:
    //  - tokenize prompt (outside HW model)
    //  - write tokens into llm_sram_region or a host buffer
    //  - program registers
    //  - loop reading tokens until EOG or n_predict exhausted
    std::string infer(const std::string& prompt,
                      const LlamaSequenceConfig& seq_cfg);

    // Variant with per-token callback for streaming UX.
    // The callback receives (token_text, token_id, is_last).
    using TokenCallback = std::function<void(const std::string&, int32_t, bool)>;

    bool infer_streaming(const std::string&       prompt,
                         const LlamaSequenceConfig& seq_cfg,
                         TokenCallback             on_token);

    // ------------------------------------------------------------------------
    // Telemetry / diagnostics
    // ------------------------------------------------------------------------

    LlamaTelemetry get_telemetry() const {
        return telemetry;
    }

    // Convenience: formatted status line for admin dashboard.
    std::string get_device_status_string() const;

    // Human-readable diagnostics (self-test, model info, errors).
    std::string get_hardware_diagnostics();

    // Optional: run a synthetic self-test inference to validate the path.
    bool run_self_test();
};

} // namespace vdev
} // namespace ggnucash
```

---

### How this plugs into your existing world

Very briefly:

* **Location / naming**: drop this as `llama-coprocessor-driver.h` right next to `financial-device-driver.h`.
* **Base class**: `DeviceDriver` comes from `virtual-device.h`, same as the financial driver.
* **Address map**: the GGUF NPU is parked at `0x40001000` in PERIPH; financial HW is at `0x40000000`. So they happily coexist.
* **Usage from app side** (pseudo):

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

Next step (if you want) we can sketch the `.cpp` with stubbed-out implementations, including how you’d pretend to do MMIO reads/writes against `VirtualPCB` and where you’d actually call into llama.cpp / gguf loader.
