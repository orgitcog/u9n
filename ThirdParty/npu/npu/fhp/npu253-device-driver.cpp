/**
 * @file npu253-device-driver.cpp
 * @brief NPU-253 Pattern Coprocessor Driver implementation
 */

#include "npu253-device-driver.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace ggnucash {
namespace vdev {

// ============================================================================
// DeviceDriver Interface
// ============================================================================

bool NPU253CoprocessorDriver::load(VirtualPCB* pcb) {
    if (!pcb) return false;
    pcb_ = pcb;
    loaded = true;
    return true;
}

bool NPU253CoprocessorDriver::initialize() {
    if (!loaded || !pcb_) return false;

    // Map register and pattern memory regions into the VirtualPCB
    if (!map_register_regions()) return false;

    // Reset device state
    status_ = STATUS_IDLE;
    error_code_ = ERR_NONE;
    telemetry_ = {};
    result_ids_.clear();

    // Write initial register values
    write_reg32(REG_STATUS, status_);
    write_reg32(REG_ERROR_CODE, error_code_);
    write_reg32(REG_PATTERN_COUNT, 0);
    write_reg32(REG_RESULT_COUNT, 0);
    write_reg32(REG_PERF_QUERIES, 0);
    write_reg32(REG_PERF_TRANSFORMS, 0);
    write_reg32(REG_PERF_AVG_US, 0);

    initialized = true;
    return true;
}

bool NPU253CoprocessorDriver::probe() {
    if (!initialized || !pcb_) return false;

    // Verify register region is accessible
    auto* ctrl = pcb_->get_memory_region("NPU253-CTRL");
    if (!ctrl) return false;

    // Write and read back a test value
    write_reg32(REG_PATTERN_ID, 0xDEAD);
    uint32_t readback = read_reg32(REG_PATTERN_ID);
    write_reg32(REG_PATTERN_ID, 0); // restore
    return readback == 0xDEAD;
}

bool NPU253CoprocessorDriver::remove() {
    patterns_.clear();
    id_index_.clear();
    result_ids_.clear();
    patterns_loaded_ = false;
    status_ = STATUS_IDLE;
    error_code_ = ERR_NONE;
    telemetry_ = {};
    pcb_ = nullptr;
    loaded = false;
    initialized = false;
    return true;
}

// ============================================================================
// Memory Mapping
// ============================================================================

bool NPU253CoprocessorDriver::map_register_regions() {
    // Control registers: 68 bytes at 0x50000000
    if (!pcb_->add_memory_region("NPU253-CTRL", REG_BASE, 256)) return false;

    // Pattern memory: 256KB at 0x50001000
    if (!pcb_->add_memory_region("NPU253-PATTERNS", PATTERN_MEM_BASE, PATTERN_MEM_SIZE))
        return false;

    // Archetypal memory: 256KB at 0x50100000
    if (!pcb_->add_memory_region("NPU253-ARCH", ARCH_MEM_BASE, ARCH_MEM_SIZE))
        return false;

    return true;
}

void NPU253CoprocessorDriver::write_reg32(uint64_t addr, uint32_t value) {
    if (!pcb_) return;
    auto* region = pcb_->get_memory_region("NPU253-CTRL");
    if (region && addr >= REG_BASE && addr < REG_BASE + region->size) {
        region->write_dword(addr - REG_BASE, value);
    }
}

uint32_t NPU253CoprocessorDriver::read_reg32(uint64_t addr) const {
    if (!pcb_) return 0;
    auto* region = pcb_->get_memory_region("NPU253-CTRL");
    if (region && addr >= REG_BASE && addr < REG_BASE + region->size) {
        return region->read_dword(addr - REG_BASE);
    }
    return 0;
}

// ============================================================================
// Pattern Management
// ============================================================================

void NPU253CoprocessorDriver::add_pattern(const PatternRecord& pattern) {
    patterns_.push_back(pattern);
}

void NPU253CoprocessorDriver::add_pattern(PatternRecord&& pattern) {
    patterns_.push_back(std::move(pattern));
}

void NPU253CoprocessorDriver::finalize_load() {
    rebuild_index();
    patterns_loaded_ = true;
    status_ = STATUS_IDLE | STATUS_PATTERNS_LOADED;
    write_reg32(REG_STATUS, status_);
    write_reg32(REG_PATTERN_COUNT, static_cast<uint32_t>(patterns_.size()));
}

void NPU253CoprocessorDriver::rebuild_index() {
    id_index_.clear();
    for (size_t i = 0; i < patterns_.size(); ++i) {
        id_index_[patterns_[i].id] = i;
    }
}

// ============================================================================
// High-Level Query API
// ============================================================================

const PatternRecord* NPU253CoprocessorDriver::query_by_id(uint32_t id) const {
    auto it = id_index_.find(id);
    if (it != id_index_.end() && it->second < patterns_.size()) {
        return &patterns_[it->second];
    }
    return nullptr;
}

std::vector<const PatternRecord*> NPU253CoprocessorDriver::query_by_name(
    const std::string& name) const
{
    std::vector<const PatternRecord*> results;
    for (const auto& p : patterns_) {
        if (icontains(p.name, name)) {
            results.push_back(&p);
        }
    }
    return results;
}

std::vector<const PatternRecord*> NPU253CoprocessorDriver::query_by_text(
    const std::string& text) const
{
    std::vector<const PatternRecord*> results;
    for (const auto& p : patterns_) {
        if (icontains(p.name, text) ||
            icontains(p.problem_summary, text) ||
            icontains(p.solution, text) ||
            icontains(p.context, text))
        {
            results.push_back(&p);
        }
    }
    return results;
}

std::vector<const PatternRecord*> NPU253CoprocessorDriver::get_preceding(uint32_t id) const {
    std::vector<const PatternRecord*> results;
    const auto* pattern = query_by_id(id);
    if (!pattern) return results;

    for (uint32_t pid : pattern->preceding) {
        const auto* p = query_by_id(pid);
        if (p) results.push_back(p);
    }
    return results;
}

std::vector<const PatternRecord*> NPU253CoprocessorDriver::get_following(uint32_t id) const {
    std::vector<const PatternRecord*> results;
    const auto* pattern = query_by_id(id);
    if (!pattern) return results;

    for (uint32_t fid : pattern->following) {
        const auto* p = query_by_id(fid);
        if (p) results.push_back(p);
    }
    return results;
}

std::vector<const PatternRecord*> NPU253CoprocessorDriver::get_sequence(uint32_t seq_id) const {
    std::vector<const PatternRecord*> results;
    for (const auto& p : patterns_) {
        for (uint32_t sid : p.sequence_ids) {
            if (sid == seq_id) {
                results.push_back(&p);
                break;
            }
        }
    }
    return results;
}

std::vector<const PatternRecord*> NPU253CoprocessorDriver::get_category(uint8_t category) const {
    std::vector<const PatternRecord*> results;
    for (const auto& p : patterns_) {
        if (p.category == category) {
            results.push_back(&p);
        }
    }
    return results;
}

// ============================================================================
// MMIO Command Dispatch
// ============================================================================

void NPU253CoprocessorDriver::dispatch_command(uint32_t cmd) {
    if (!patterns_loaded_ && cmd != CMD_RESET && cmd != CMD_LOAD_PATTERNS &&
        cmd != CMD_SELF_TEST)
    {
        error_code_ = ERR_NOT_LOADED;
        status_ = (status_ & ~STATUS_BUSY) | STATUS_ERROR;
        write_reg32(REG_STATUS, status_);
        write_reg32(REG_ERROR_CODE, error_code_);
        return;
    }

    status_ = (status_ & ~(STATUS_IDLE | STATUS_ERROR)) | STATUS_BUSY;
    error_code_ = ERR_NONE;
    write_reg32(REG_STATUS, status_);

    switch (cmd) {
    case CMD_RESET:
        status_ = STATUS_IDLE;
        error_code_ = ERR_NONE;
        result_ids_.clear();
        break;

    case CMD_LOAD_PATTERNS:
        finalize_load();
        break;

    case CMD_QUERY_BY_ID: {
        uint32_t id = read_reg32(REG_PATTERN_ID);
        const auto* p = query_by_id(id);
        result_ids_.clear();
        if (p) {
            result_ids_.push_back(p->id);
        } else {
            error_code_ = ERR_PATTERN_NOT_FOUND;
        }
        telemetry_.total_queries++;
        break;
    }

    case CMD_QUERY_BY_TEXT: {
        // In a real MMIO implementation, the query string would be read
        // from the address in REG_QUERY_ADDR. For simulation, we store
        // results from the most recent high-level API call.
        telemetry_.total_queries++;
        break;
    }

    case CMD_TRANSFORM: {
        uint32_t domain = read_reg32(REG_DOMAIN_MODE);
        if (domain > static_cast<uint32_t>(DomainMode::PSYCHIC)) {
            error_code_ = ERR_INVALID_DOMAIN;
        } else {
            telemetry_.total_transforms++;
        }
        break;
    }

    case CMD_GET_PRECEDING: {
        uint32_t id = read_reg32(REG_PATTERN_ID);
        auto results = get_preceding(id);
        result_ids_.clear();
        for (const auto* p : results) result_ids_.push_back(p->id);
        if (results.empty()) error_code_ = ERR_PATTERN_NOT_FOUND;
        telemetry_.total_queries++;
        break;
    }

    case CMD_GET_FOLLOWING: {
        uint32_t id = read_reg32(REG_PATTERN_ID);
        auto results = get_following(id);
        result_ids_.clear();
        for (const auto* p : results) result_ids_.push_back(p->id);
        if (results.empty()) error_code_ = ERR_PATTERN_NOT_FOUND;
        telemetry_.total_queries++;
        break;
    }

    case CMD_GET_SEQUENCE: {
        uint32_t seq_id = read_reg32(REG_SEQUENCE_ID);
        auto results = get_sequence(seq_id);
        result_ids_.clear();
        for (const auto* p : results) result_ids_.push_back(p->id);
        telemetry_.total_queries++;
        break;
    }

    case CMD_GET_CATEGORY: {
        uint32_t cat = read_reg32(REG_CATEGORY);
        auto results = get_category(static_cast<uint8_t>(cat));
        result_ids_.clear();
        for (const auto* p : results) result_ids_.push_back(p->id);
        telemetry_.total_queries++;
        break;
    }

    case CMD_SELF_TEST:
        if (run_self_test()) {
            status_ |= STATUS_SELF_TEST_OK;
        } else {
            error_code_ = ERR_MEMORY_ERROR;
        }
        break;

    default:
        error_code_ = ERR_INVALID_CMD;
        break;
    }

    // Update status and result registers
    status_ = (status_ & ~STATUS_BUSY) | STATUS_IDLE;
    if (error_code_ != ERR_NONE) status_ |= STATUS_ERROR;
    if (!result_ids_.empty()) status_ |= STATUS_READY;

    write_reg32(REG_STATUS, status_);
    write_reg32(REG_ERROR_CODE, error_code_);
    write_reg32(REG_RESULT_COUNT, static_cast<uint32_t>(result_ids_.size()));
    write_reg32(REG_PERF_QUERIES, static_cast<uint32_t>(telemetry_.total_queries));
    write_reg32(REG_PERF_TRANSFORMS, static_cast<uint32_t>(telemetry_.total_transforms));
}

// ============================================================================
// Self Test
// ============================================================================

bool NPU253CoprocessorDriver::run_self_test() {
    if (!pcb_) return false;

    // Test 1: Register read/write
    write_reg32(REG_PATTERN_ID, 0xCAFE);
    if (read_reg32(REG_PATTERN_ID) != 0xCAFE) return false;
    write_reg32(REG_PATTERN_ID, 0);

    // Test 2: Memory region accessibility
    auto* ctrl = pcb_->get_memory_region("NPU253-CTRL");
    if (!ctrl) return false;

    auto* pat_mem = pcb_->get_memory_region("NPU253-PATTERNS");
    if (!pat_mem) return false;

    auto* arch_mem = pcb_->get_memory_region("NPU253-ARCH");
    if (!arch_mem) return false;

    // Test 3: Index consistency (if patterns loaded)
    if (patterns_loaded_) {
        for (const auto& [id, idx] : id_index_) {
            if (idx >= patterns_.size()) return false;
            if (patterns_[idx].id != id) return false;
        }
    }

    return true;
}

// ============================================================================
// Status String
// ============================================================================

std::string NPU253CoprocessorDriver::get_device_status_string() const {
    std::ostringstream ss;
    ss << "NPU-253 Pattern Coprocessor v" << driver_version << "\n";
    ss << "  State:    ";
    if (status_ & STATUS_BUSY) ss << "BUSY ";
    if (status_ & STATUS_IDLE) ss << "IDLE ";
    if (status_ & STATUS_READY) ss << "READY ";
    if (status_ & STATUS_ERROR) ss << "ERROR ";
    if (status_ & STATUS_PATTERNS_LOADED) ss << "LOADED ";
    if (status_ & STATUS_SELF_TEST_OK) ss << "TESTED ";
    ss << "\n";
    ss << "  Patterns: " << patterns_.size() << " loaded\n";
    ss << "  Queries:  " << telemetry_.total_queries << "\n";
    ss << "  Xforms:   " << telemetry_.total_transforms << "\n";
    if (error_code_ != ERR_NONE) {
        ss << "  Error:    0x" << std::hex << error_code_ << std::dec << "\n";
    }
    return ss.str();
}

// ============================================================================
// Utility
// ============================================================================

bool NPU253CoprocessorDriver::icontains(const std::string& haystack,
                                         const std::string& needle)
{
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;

    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        });

    return it != haystack.end();
}

} // namespace vdev
} // namespace ggnucash
