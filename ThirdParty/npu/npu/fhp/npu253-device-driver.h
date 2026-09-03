#pragma once

#include "virtual-device.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

namespace ggnucash {
namespace vdev {

// ============================================================================
// NPU-253 Pattern Coprocessor Driver
// ============================================================================
// Memory-mapped co-processor for the 253-pattern language system.
// Provides hardware-accelerated pattern lookup, text search, domain
// transformation, and sequence/category navigation.
//
// Register base: 0x50000000 (separate from LLM NPU at 0x40001000)
// Pattern memory: 0x50001000 (256KB)
// Archetypal memory: 0x50100000 (256KB)
// ============================================================================

// ----------------------------------------------------------------------------
// Data Structures
// ----------------------------------------------------------------------------

/// A single pattern stored in the coprocessor's memory
struct PatternRecord {
    uint32_t id;                       // Pattern number (0-253)
    std::string name;                  // Pattern name
    std::string problem_summary;       // Problem statement
    std::string solution;              // Solution description
    std::string context;               // Context/background
    uint8_t evidence_level;            // Confidence (1-3)
    uint8_t category;                  // 0=none, 1=towns, 2=buildings, 3=construction
    std::vector<uint32_t> preceding;   // Patterns that precede
    std::vector<uint32_t> following;   // Patterns that follow
    std::vector<uint32_t> sequence_ids; // Sequences containing this pattern
};

/// Telemetry counters
struct NPU253Telemetry {
    uint64_t total_queries      = 0;
    uint64_t total_transforms   = 0;
    uint64_t cache_hits         = 0;
    uint64_t cache_misses       = 0;
    double   avg_query_time_us  = 0.0;
};

/// Domain transformation modes
enum class DomainMode : uint32_t {
    NONE       = 0,
    PHYSICAL   = 1,
    SOCIAL     = 2,
    CONCEPTUAL = 3,
    PSYCHIC    = 4
};

// ----------------------------------------------------------------------------
// NPU253CoprocessorDriver
// ----------------------------------------------------------------------------

class NPU253CoprocessorDriver : public DeviceDriver {
public:
    // ========================================================================
    // Register Layout (0x50000000 base)
    // ========================================================================

    static constexpr uint64_t REG_BASE           = 0x50000000;

    // Control registers (17 × 4 bytes = 68 bytes)
    static constexpr uint64_t REG_CMD            = REG_BASE + 0x00;
    static constexpr uint64_t REG_STATUS         = REG_BASE + 0x04;
    static constexpr uint64_t REG_PATTERN_ID     = REG_BASE + 0x08;
    static constexpr uint64_t REG_PATTERN_COUNT  = REG_BASE + 0x0C;
    static constexpr uint64_t REG_QUERY_ADDR_LO  = REG_BASE + 0x10;
    static constexpr uint64_t REG_QUERY_ADDR_HI  = REG_BASE + 0x14;
    static constexpr uint64_t REG_QUERY_LEN      = REG_BASE + 0x18;
    static constexpr uint64_t REG_RESULT_ADDR_LO = REG_BASE + 0x1C;
    static constexpr uint64_t REG_RESULT_ADDR_HI = REG_BASE + 0x20;
    static constexpr uint64_t REG_RESULT_COUNT   = REG_BASE + 0x24;
    static constexpr uint64_t REG_DOMAIN_MODE    = REG_BASE + 0x28;
    static constexpr uint64_t REG_SEQUENCE_ID    = REG_BASE + 0x2C;
    static constexpr uint64_t REG_CATEGORY       = REG_BASE + 0x30;
    static constexpr uint64_t REG_ERROR_CODE     = REG_BASE + 0x34;
    static constexpr uint64_t REG_PERF_QUERIES   = REG_BASE + 0x38;
    static constexpr uint64_t REG_PERF_TRANSFORMS = REG_BASE + 0x3C;
    static constexpr uint64_t REG_PERF_AVG_US    = REG_BASE + 0x40;

    // Memory regions
    static constexpr uint64_t PATTERN_MEM_BASE   = 0x50001000;
    static constexpr uint64_t PATTERN_MEM_SIZE   = 256 * 1024;  // 256KB
    static constexpr uint64_t ARCH_MEM_BASE      = 0x50100000;
    static constexpr uint64_t ARCH_MEM_SIZE      = 256 * 1024;  // 256KB

    // ========================================================================
    // Command Codes (written to REG_CMD)
    // ========================================================================

    enum Command : uint32_t {
        CMD_RESET            = 0x00,
        CMD_LOAD_PATTERNS    = 0x01,
        CMD_QUERY_BY_ID      = 0x02,
        CMD_QUERY_BY_NAME    = 0x03,
        CMD_QUERY_BY_TEXT    = 0x04,
        CMD_TRANSFORM        = 0x05,
        CMD_GET_PRECEDING    = 0x06,
        CMD_GET_FOLLOWING    = 0x07,
        CMD_GET_SEQUENCE     = 0x08,
        CMD_GET_CATEGORY     = 0x09,
        CMD_SELF_TEST        = 0x0A,
    };

    // ========================================================================
    // Status Bits (read from REG_STATUS)
    // ========================================================================

    enum StatusBit : uint32_t {
        STATUS_IDLE            = 0x01,
        STATUS_BUSY            = 0x02,
        STATUS_READY           = 0x04,
        STATUS_ERROR           = 0x08,
        STATUS_PATTERNS_LOADED = 0x10,
        STATUS_CACHE_HOT       = 0x20,
        STATUS_SELF_TEST_OK    = 0x40,
    };

    // ========================================================================
    // Error Codes (read from REG_ERROR_CODE)
    // ========================================================================

    enum ErrorCode : uint32_t {
        ERR_NONE              = 0x00,
        ERR_INVALID_CMD       = 0x01,
        ERR_PATTERN_NOT_FOUND = 0x02,
        ERR_INVALID_DOMAIN    = 0x03,
        ERR_QUERY_TIMEOUT     = 0x04,
        ERR_TRANSFORM_FAIL    = 0x05,
        ERR_MEMORY_ERROR      = 0x06,
        ERR_NOT_LOADED        = 0x07,
    };

    // ========================================================================
    // Construction
    // ========================================================================

    NPU253CoprocessorDriver()
        : DeviceDriver("NPU-253-Pattern-Coprocessor", "0.1.0") {}

    ~NPU253CoprocessorDriver() override = default;

    // ========================================================================
    // DeviceDriver Interface
    // ========================================================================

    bool load(VirtualPCB* pcb) override;
    bool initialize() override;
    bool probe() override;
    bool remove() override;

    // ========================================================================
    // Pattern Management
    // ========================================================================

    /// Add a pattern to the coprocessor's memory
    void add_pattern(const PatternRecord& pattern);
    void add_pattern(PatternRecord&& pattern);

    /// Number of loaded patterns
    size_t pattern_count() const { return patterns_.size(); }

    /// Whether patterns have been loaded
    bool patterns_loaded() const { return patterns_loaded_; }

    /// Mark patterns as loaded (after bulk add)
    void finalize_load();

    // ========================================================================
    // High-Level Query API (bypasses MMIO for direct C++ access)
    // ========================================================================

    const PatternRecord* query_by_id(uint32_t id) const;
    std::vector<const PatternRecord*> query_by_name(const std::string& name) const;
    std::vector<const PatternRecord*> query_by_text(const std::string& text) const;
    std::vector<const PatternRecord*> get_preceding(uint32_t id) const;
    std::vector<const PatternRecord*> get_following(uint32_t id) const;
    std::vector<const PatternRecord*> get_sequence(uint32_t seq_id) const;
    std::vector<const PatternRecord*> get_category(uint8_t category) const;

    // ========================================================================
    // MMIO Command Dispatch
    // ========================================================================

    /// Process a command written to REG_CMD
    void dispatch_command(uint32_t cmd);

    // ========================================================================
    // Status & Telemetry
    // ========================================================================

    uint32_t read_status() const { return status_; }
    uint32_t read_error() const { return error_code_; }
    NPU253Telemetry get_telemetry() const { return telemetry_; }

    /// Run built-in diagnostics
    bool run_self_test();

    /// Human-readable status string
    std::string get_device_status_string() const;

private:
    VirtualPCB* pcb_ = nullptr;

    // Pattern storage
    std::vector<PatternRecord> patterns_;
    std::map<uint32_t, size_t> id_index_;  // pattern_id → vector index
    bool patterns_loaded_ = false;

    // Last query result buffer
    std::vector<uint32_t> result_ids_;

    // Device state
    uint32_t status_     = STATUS_IDLE;
    uint32_t error_code_ = ERR_NONE;

    // Telemetry
    NPU253Telemetry telemetry_;

    // MMIO helpers
    void write_reg32(uint64_t addr, uint32_t value);
    uint32_t read_reg32(uint64_t addr) const;
    bool map_register_regions();

    // Internal helpers
    void rebuild_index();
    static bool icontains(const std::string& haystack, const std::string& needle);
};

} // namespace vdev
} // namespace ggnucash
