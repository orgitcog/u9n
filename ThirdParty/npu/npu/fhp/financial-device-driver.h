#pragma once

#include "virtual-device.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace ggnucash {
namespace vdev {

// ============================================================================
// GGNuCash Financial Device Driver
// ============================================================================
// This driver integrates the financial hardware simulation with the virtual
// device architecture, providing a complete hardware abstraction for the
// Chart of Accounts as a physical PCB
// ============================================================================

class FinancialDeviceDriver : public DeviceDriver {
private:
    // Financial circuit mapping
    struct AccountPin {
        std::string account_code;
        uint32_t pin_number;
        double balance_voltage;  // Balance mapped to voltage
        bool is_active;
    };
    
    std::map<std::string, AccountPin> account_pins;
    std::shared_ptr<MemoryRegion> transaction_buffer;
    
    // Hardware registers (memory-mapped)
    static constexpr uint64_t REG_BASE = 0x40000000;
    static constexpr uint64_t REG_TRANSACTION_STATUS = REG_BASE + 0x00;
    static constexpr uint64_t REG_BALANCE_TOTAL = REG_BASE + 0x04;
    static constexpr uint64_t REG_TRANSACTION_COUNT = REG_BASE + 0x08;
    static constexpr uint64_t REG_ERROR_CODE = REG_BASE + 0x0C;
    
public:
    FinancialDeviceDriver() 
        : DeviceDriver("GGNuCash-Financial-HW", "1.0.0") {}
    
    bool load(VirtualPCB* pcb) override;
    bool initialize() override;
    bool probe() override;
    bool remove() override;
    
    // Financial operations
    bool map_account_to_pin(const std::string& account_code, uint32_t pin);
    bool update_account_balance(const std::string& account_code, double balance);
    double read_account_balance(const std::string& account_code);
    bool process_hardware_transaction(const std::vector<uint8_t>& tx_data);
    
    // Hardware diagnostics
    std::string get_hardware_diagnostics();
    bool run_self_test();
};

// ============================================================================
// Telemetry System
// ============================================================================

struct TelemetryMetric {
    std::string name;
    std::string unit;
    double value;
    double min_value;
    double max_value;
    double avg_value;
    uint64_t sample_count;
    std::chrono::steady_clock::time_point last_update;
    
    TelemetryMetric() : name(""), unit(""), value(0.0), min_value(0.0), 
          max_value(0.0), avg_value(0.0), sample_count(0) {}
    
    TelemetryMetric(const std::string& n, const std::string& u)
        : name(n), unit(u), value(0.0), min_value(0.0), 
          max_value(0.0), avg_value(0.0), sample_count(0) {}
    
    void update(double new_value);
    void reset();
};

class TelemetrySystem {
private:
    std::map<std::string, TelemetryMetric> metrics;
    std::mutex metrics_mutex;
    bool is_collecting;
    
public:
    TelemetrySystem() : is_collecting(false) {}
    
    void start_collection();
    void stop_collection();
    bool add_metric(const std::string& name, const std::string& unit);
    bool update_metric(const std::string& name, double value);
    TelemetryMetric get_metric(const std::string& name);
    std::vector<std::string> list_metrics();
    std::string generate_report();
    void reset_all();
};

// ============================================================================
// Configuration Manager
// ============================================================================

struct DeviceConfiguration {
    std::string config_name;
    std::map<std::string, std::string> parameters;
    bool is_active;
    std::chrono::system_clock::time_point last_modified;
    
    DeviceConfiguration() : config_name(""), is_active(false) {
        last_modified = std::chrono::system_clock::now();
    }
    
    DeviceConfiguration(const std::string& name) 
        : config_name(name), is_active(false) {
        last_modified = std::chrono::system_clock::now();
    }
};

class ConfigurationManager {
private:
    std::map<std::string, DeviceConfiguration> configurations;
    std::string active_config_name;
    std::string config_file_path;
    std::mutex config_mutex;
    
public:
    ConfigurationManager() : active_config_name("default"), 
                            config_file_path("/etc/ggnucash/device.conf") {}
    
    bool create_configuration(const std::string& name);
    bool delete_configuration(const std::string& name);
    bool load_configuration(const std::string& name);
    bool save_configuration(const std::string& name);
    bool set_parameter(const std::string& param, const std::string& value);
    std::string get_parameter(const std::string& param);
    bool apply_configuration(VirtualPCB* device);
    std::vector<std::string> list_configurations();
    bool import_from_file(const std::string& filepath);
    bool export_to_file(const std::string& filepath);
};

// ============================================================================
// Diagnostic System
// ============================================================================

enum class DiagnosticLevel {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

struct DiagnosticEvent {
    DiagnosticLevel level;
    std::string source;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    uint64_t event_id;
    
    DiagnosticEvent() : level(DiagnosticLevel::INFO), event_id(0) {
        timestamp = std::chrono::system_clock::now();
    }
};

class DiagnosticSystem {
private:
    std::vector<DiagnosticEvent> event_log;
    size_t max_log_size;
    uint64_t next_event_id;
    std::mutex diagnostic_mutex;
    
public:
    DiagnosticSystem(size_t max_size = 10000) 
        : max_log_size(max_size), next_event_id(1) {}
    
    void log_event(DiagnosticLevel level, const std::string& source, 
                   const std::string& message);
    std::vector<DiagnosticEvent> get_events(DiagnosticLevel min_level = DiagnosticLevel::INFO);
    std::vector<DiagnosticEvent> get_recent_events(size_t count);
    void clear_log();
    std::string generate_diagnostic_report();
    
    // Health checks
    bool check_device_health(VirtualPCB* device);
    bool check_memory_integrity(VirtualPCB* device);
    bool check_io_health(VirtualPCB* device);
};

// ============================================================================
// Dashboard System
// ============================================================================

class DashboardSystem {
private:
    VirtualPCB* device;
    TelemetrySystem* telemetry;
    DiagnosticSystem* diagnostics;
    bool is_running;
    std::chrono::milliseconds update_interval;
    
public:
    DashboardSystem(VirtualPCB* dev, TelemetrySystem* telem, DiagnosticSystem* diag)
        : device(dev), telemetry(telem), diagnostics(diag), 
          is_running(false), update_interval(1000) {}
    
    void start_dashboard();
    void stop_dashboard();
    void set_update_interval(std::chrono::milliseconds interval);
    
    // Display functions
    std::string render_dashboard();
    std::string render_device_status();
    std::string render_telemetry_panel();
    std::string render_diagnostics_panel();
    std::string render_io_status();
    std::string render_memory_map();
    
    // Interactive mode
    void run_interactive_dashboard();
};

} // namespace vdev
} // namespace ggnucash
