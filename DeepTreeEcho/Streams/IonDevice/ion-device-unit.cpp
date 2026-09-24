// ═══════════════════════════════════════════════════════════════════════════
// ion-device-unit.cpp
// IonDeviceUnit — Auto-generated PE-to-NPU pipeline driver implementation.
//
// Implements the virtual hardware abstraction for the ion.exe cognitive binary.
// Maps 3 memory regions into the VirtualPCB address space:
//   ION-CTRL      : Control block registers (0x40002000 + 0x000)
//   ION-TELEMETRY : Telemetry counters      (0x40002000 + 0x180)
//   ION-FUNC      : Function table          (0x40002000 + 0x200)
//
// Pattern follows npu253-device-driver.cpp in ThirdParty/npu/npu/fhp/.
// SPDX-License-Identifier: MIT
// ═══════════════════════════════════════════════════════════════════════════

#include "ion-device-unit.h"
#include <sstream>
#include <iomanip>

namespace ggnucash::vdev {

// ═══════════════════════════════════════════════════════════════════════════
// Driver Lifecycle
// ═══════════════════════════════════════════════════════════════════════════

bool IonDeviceUnit::load(VirtualPCB* pcb) {
    if (!pcb) return false;
    pcb_ = pcb;

    // Map ION memory regions into the virtual PCB
    if (!pcb_->add_memory_region("ION-CTRL",      ION_BASE,         0x040)) return false;
    if (!pcb_->add_memory_region("ION-TELEMETRY", ION_BASE + 0x180, 0x040)) return false;
    // Function table: ION_FUNC_COUNT entries of uint32_t each
    const uint64_t func_table_size = static_cast<uint64_t>(ION_FUNC_COUNT) * sizeof(uint32_t);
    if (!pcb_->add_memory_region("ION-FUNC",      ION_BASE + 0x200, func_table_size)) return false;

    loaded = true;
    return true;
}

bool IonDeviceUnit::initialize() {
    if (!loaded || !pcb_) return false;

    // Reset device — clear all control registers
    write_reg32(REG_ION_CMD,    CMD_RESET);
    write_reg32(REG_ION_STATUS, STATUS_IDLE);
    write_reg32(REG_ION_FLAGS,  0);
    write_reg32(REG_ION_ERROR,  0);
    write_reg32(REG_ION_DEBUG,  0);

    // Zero telemetry counters
    write_reg32(REG_ION_DISPATCH_CNT, 0);
    write_reg32(REG_ION_CACHE_HITS,   0);
    write_reg32(REG_ION_CACHE_MISS,   0);
    write_reg32(REG_ION_LATENCY_NS,   0);
    write_reg32(REG_ION_ERROR_CNT,    0);
    write_reg32(REG_ION_UPTIME_SEC,   0);

    // Issue INIT command — device transitions to READY
    write_reg32(REG_ION_CMD, CMD_INIT);

    // Set device status to READY after successful initialization
    write_reg32(REG_ION_STATUS, STATUS_READY);

    // Clear cached telemetry snapshot
    telemetry_ = {};

    initialized = true;
    return true;
}

bool IonDeviceUnit::probe() {
    if (!initialized || !pcb_) return false;

    // Verify all three memory regions are accessible
    auto* ctrl_region = pcb_->get_memory_region("ION-CTRL");
    if (!ctrl_region) return false;

    auto* tel_region = pcb_->get_memory_region("ION-TELEMETRY");
    if (!tel_region) return false;

    auto* func_region = pcb_->get_memory_region("ION-FUNC");
    if (!func_region) return false;

    // Write and read back a sentinel to the debug register
    const uint32_t sentinel = 0xDE10U;
    write_reg32(REG_ION_DEBUG, sentinel);
    const uint32_t readback = read_reg32(REG_ION_DEBUG);
    write_reg32(REG_ION_DEBUG, 0);  // restore

    return (readback == sentinel);
}

bool IonDeviceUnit::remove() {
    if (pcb_) {
        // Graceful shutdown: issue SOFT_STOP then SHUTDOWN
        write_reg32(REG_ION_CMD, CMD_SOFT_STOP);
        write_reg32(REG_ION_CMD, CMD_SHUTDOWN);
    }

    telemetry_  = {};
    pcb_        = nullptr;
    loaded      = false;
    initialized = false;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Telemetry & Entelechy
// ═══════════════════════════════════════════════════════════════════════════

IonDeviceUnitTelemetry IonDeviceUnit::get_telemetry() const {
    if (!initialized || !pcb_) return telemetry_;

    IonDeviceUnitTelemetry t;
    t.total_dispatches = static_cast<uint64_t>(read_reg32(REG_ION_DISPATCH_CNT));
    t.cache_hits       = static_cast<uint64_t>(read_reg32(REG_ION_CACHE_HITS));
    t.cache_misses     = static_cast<uint64_t>(read_reg32(REG_ION_CACHE_MISS));
    t.avg_latency_ns   = static_cast<double>(read_reg32(REG_ION_LATENCY_NS));
    t.error_count      = static_cast<uint64_t>(read_reg32(REG_ION_ERROR_CNT));
    t.uptime_seconds   = static_cast<uint64_t>(read_reg32(REG_ION_UPTIME_SEC));
    return t;
}

IonDeviceUnitEntelechy IonDeviceUnit::get_entelechy() const {
    return entelechy_;
}

std::string IonDeviceUnit::get_status_string() const {
    if (!initialized) return "UNINITIALIZED";
    if (!pcb_)        return "NO_PCB";

    const uint32_t status = read_reg32(REG_ION_STATUS);
    switch (status) {
        case STATUS_IDLE:  return "IDLE";
        case STATUS_BUSY:  return "BUSY";
        case STATUS_INIT:  return "INITIALIZING";
        case STATUS_READY: return "READY";
        case STATUS_ERROR: return "ERROR";
        default: {
            std::ostringstream oss;
            oss << "UNKNOWN(0x"
                << std::hex << std::setw(8) << std::setfill('0') << status << ")";
            return oss.str();
        }
    }
}

std::string IonDeviceUnit::get_diagnostics() const {
    std::ostringstream oss;
    oss << "=== IonDeviceUnit Diagnostics ===\n";
    oss << "Driver:      " << driver_name  << " v" << driver_version << "\n";
    oss << "Loaded:      " << (loaded      ? "YES" : "NO") << "\n";
    oss << "Initialized: " << (initialized ? "YES" : "NO") << "\n";
    oss << "PCB:         " << (pcb_        ? "ATTACHED" : "DETACHED") << "\n";
    oss << "Status:      " << get_status_string() << "\n";

    if (initialized) {
        const auto t = get_telemetry();
        oss << "--- Telemetry ---\n";
        oss << "Dispatches:  " << t.total_dispatches << "\n";
        oss << "Cache hits:  " << t.cache_hits       << "\n";
        oss << "Cache miss:  " << t.cache_misses     << "\n";
        oss << "Avg latency: " << t.avg_latency_ns   << " ns\n";
        oss << "Errors:      " << t.error_count      << "\n";
        oss << "Uptime:      " << t.uptime_seconds   << " s\n";

        const uint32_t err = read_reg32(REG_ION_ERROR);
        if (err != 0) {
            oss << "Error reg:   0x"
                << std::hex << std::setw(8) << std::setfill('0') << err << "\n";
        }
    }

    oss << "--- Entelechy ---\n";
    oss << "Source:    " << entelechy_.source_binary  << "\n";
    oss << "API:       " << entelechy_.primary_api    << "\n";
    oss << "Domain:    " << entelechy_.primary_domain << "\n";
    oss << "Functions: " << entelechy_.total_functions << "\n";
    oss << "Exports:   " << entelechy_.export_count   << "\n";
    oss << "Imports:   " << entelechy_.import_count   << "\n";
    oss << "CFG:       " << (entelechy_.has_cfg  ? "YES" : "NO") << "\n";
    oss << "ASLR:      " << (entelechy_.has_aslr ? "YES" : "NO") << "\n";
    oss << "DEP:       " << (entelechy_.has_dep  ? "YES" : "NO") << "\n";
    return oss.str();
}

bool IonDeviceUnit::run_self_test() {
    if (!initialized || !pcb_) return false;

    // Issue SELF_TEST command
    write_reg32(REG_ION_CMD, CMD_SELF_TEST);

    // In the virtual PCB model register reads return whatever was written.
    // A successful self-test leaves REG_ION_ERROR at 0.
    const uint32_t err = read_reg32(REG_ION_ERROR);

    // Return to NOP state
    write_reg32(REG_ION_CMD, CMD_NOP);

    return (err == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Private — Register Access
// ═══════════════════════════════════════════════════════════════════════════

void IonDeviceUnit::write_reg32(uint64_t addr, uint32_t value) {
    if (!pcb_) return;

    // Control block: 0x40002000 + [0x000 .. 0x03F]
    auto* ctrl = pcb_->get_memory_region("ION-CTRL");
    if (ctrl && addr >= ION_BASE && addr < ION_BASE + ctrl->size) {
        ctrl->write_dword(addr - ION_BASE, value);
        return;
    }

    // Telemetry block: 0x40002000 + [0x180 .. 0x1BF]
    static constexpr uint64_t TEL_BASE = ION_BASE + 0x180;
    auto* tel = pcb_->get_memory_region("ION-TELEMETRY");
    if (tel && addr >= TEL_BASE && addr < TEL_BASE + tel->size) {
        tel->write_dword(addr - TEL_BASE, value);
    }
}

uint32_t IonDeviceUnit::read_reg32(uint64_t addr) const {
    if (!pcb_) return 0;

    // Control block
    auto* ctrl = pcb_->get_memory_region("ION-CTRL");
    if (ctrl && addr >= ION_BASE && addr < ION_BASE + ctrl->size) {
        return ctrl->read_dword(addr - ION_BASE);
    }

    // Telemetry block
    static constexpr uint64_t TEL_BASE = ION_BASE + 0x180;
    auto* tel = pcb_->get_memory_region("ION-TELEMETRY");
    if (tel && addr >= TEL_BASE && addr < TEL_BASE + tel->size) {
        return tel->read_dword(addr - TEL_BASE);
    }

    return 0;
}

} // namespace ggnucash::vdev
