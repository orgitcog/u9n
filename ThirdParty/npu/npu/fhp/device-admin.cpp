#include "virtual-device.h"
#include "financial-device-driver.h"
#include "llama-coprocessor-driver.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace ggnucash::vdev;

// ============================================================================
// Admin Configuration Utility for GGNuCash Virtual Device
// ============================================================================

class AdminUtility {
private:
    std::shared_ptr<VirtualPCB> device;
    std::shared_ptr<FinancialDeviceDriver> driver;
    std::shared_ptr<LlamaCoprocessorDriver> llm_driver;
    std::unique_ptr<TelemetrySystem> telemetry;
    std::unique_ptr<DiagnosticSystem> diagnostics;
    std::unique_ptr<ConfigurationManager> config_manager;
    std::unique_ptr<DashboardSystem> dashboard;
    
    bool running;
    
public:
    AdminUtility() : running(false) {}
    
    bool initialize() {
        std::cout << "╔═══════════════════════════════════════════════════════╗\n";
        std::cout << "║  GGNuCash Financial Hardware - Admin Utility v1.0   ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";
        
        // Create virtual device
        std::cout << "Initializing virtual hardware...\n";
        device = std::make_shared<VirtualPCB>("GGNC-FIN-001", "GGNuCash-X1");
        
        if (!device->initialize()) {
            std::cerr << "Failed to initialize virtual device\n";
            return false;
        }
        
        // Create and load driver
        std::cout << "Loading financial device driver...\n";
        driver = std::make_shared<FinancialDeviceDriver>();
        
        if (!driver->load(device.get())) {
            std::cerr << "Failed to load driver\n";
            return false;
        }
        
        if (!driver->initialize()) {
            std::cerr << "Failed to initialize driver\n";
            return false;
        }
        
        // Create and load LLM NPU coprocessor driver
        std::cout << "Loading LLM NPU coprocessor driver...\n";
        llm_driver = std::make_shared<LlamaCoprocessorDriver>();
        
        if (!llm_driver->load(device.get())) {
            std::cerr << "Failed to load LLM driver\n";
            return false;
        }
        
        if (!llm_driver->initialize()) {
            std::cerr << "Failed to initialize LLM driver\n";
            return false;
        }
        
        // Configure LLM model (stub)
        LlamaModelConfig llm_cfg;
        llm_cfg.model_path = "models/finance-llm.gguf";
        llm_cfg.model_name = "FinanceGPT";
        llm_cfg.n_ctx = 4096;
        llm_cfg.n_threads = 4;
        llm_driver->set_model_config(llm_cfg);
        llm_driver->load_model();
        
        // Register device
        auto* registry = DeviceRegistry::get_instance();
        registry->register_device(device->get_device_id(), device);
        registry->register_driver(driver->get_name(), driver);
        registry->register_driver(llm_driver->get_name(), llm_driver);
        
        // Create management systems
        telemetry = std::make_unique<TelemetrySystem>();
        diagnostics = std::make_unique<DiagnosticSystem>();
        config_manager = std::make_unique<ConfigurationManager>();
        dashboard = std::make_unique<DashboardSystem>(device.get(), telemetry.get(), 
                                                       diagnostics.get());
        
        // Initialize telemetry metrics
        setup_telemetry();
        
        // Start the device
        if (!device->start()) {
            std::cerr << "Failed to start device\n";
            return false;
        }
        
        diagnostics->log_event(DiagnosticLevel::INFO, "AdminUtil", 
                              "System initialized successfully");
        
        running = true;
        std::cout << "\nInitialization complete!\n\n";
        
        return true;
    }
    
    void setup_telemetry() {
        telemetry->add_metric("voltage_3v3", "V");
        telemetry->add_metric("voltage_5v", "V");
        telemetry->add_metric("current", "mA");
        telemetry->add_metric("temperature", "°C");
        telemetry->add_metric("uptime", "s");
        telemetry->add_metric("transaction_count", "count");
        
        telemetry->start_collection();
    }
    
    void update_telemetry() {
        if (!device || !telemetry) return;
        
        telemetry->update_metric("voltage_3v3", device->get_voltage_3v3());
        telemetry->update_metric("voltage_5v", device->get_voltage_5v());
        telemetry->update_metric("current", device->get_current());
        telemetry->update_metric("temperature", device->get_temperature());
        telemetry->update_metric("uptime", device->get_uptime_ms() / 1000.0);
    }
    
    void run_interactive() {
        if (!running) {
            std::cerr << "System not initialized\n";
            return;
        }
        
        print_help();
        
        std::string input;
        while (running) {
            std::cout << "\nGGNuCash Admin> ";
            std::getline(std::cin, input);
            
            if (input.empty()) continue;
            
            std::istringstream iss(input);
            std::string command;
            iss >> command;
            
            if (command == "quit" || command == "exit") {
                break;
            } else if (command == "help") {
                print_help();
            } else if (command == "status") {
                show_device_status();
            } else if (command == "dashboard") {
                show_dashboard();
            } else if (command == "telemetry") {
                show_telemetry();
            } else if (command == "diagnostics") {
                show_diagnostics();
            } else if (command == "config") {
                configure_device(iss);
            } else if (command == "driver") {
                driver_operations(iss);
            } else if (command == "health") {
                run_health_check();
            } else if (command == "selftest") {
                run_self_test();
            } else if (command == "reset") {
                reset_device();
            } else if (command == "devices") {
                list_devices();
            } else if (command == "io") {
                io_operations(iss);
            } else if (command == "memory") {
                memory_operations(iss);
            } else if (command == "pins") {
                pin_operations(iss);
            } else if (command == "llm" || command == "npu") {
                llm_operations(iss);
            } else {
                std::cout << "Unknown command: " << command << "\n";
                std::cout << "Type 'help' for available commands.\n";
            }
            
            // Update telemetry after each command
            update_telemetry();
            device->update();
        }
        
        shutdown();
    }
    
    void print_help() {
        std::cout << "\n=== Available Commands ===\n";
        std::cout << "System Management:\n";
        std::cout << "  status         - Show device status\n";
        std::cout << "  dashboard      - Display telemetry dashboard\n";
        std::cout << "  telemetry      - Show telemetry metrics\n";
        std::cout << "  diagnostics    - Show diagnostic events\n";
        std::cout << "  health         - Run health check\n";
        std::cout << "  selftest       - Run hardware self-test\n";
        std::cout << "  reset          - Reset device\n";
        std::cout << "  devices        - List registered devices\n";
        std::cout << "\nConfiguration:\n";
        std::cout << "  config list    - List configurations\n";
        std::cout << "  config load    - Load configuration\n";
        std::cout << "  config save    - Save configuration\n";
        std::cout << "\nDriver Operations:\n";
        std::cout << "  driver info    - Show driver information\n";
        std::cout << "  driver test    - Run driver tests\n";
        std::cout << "  driver probe   - Probe hardware\n";
        std::cout << "\nHardware Operations:\n";
        std::cout << "  io list        - List I/O streams\n";
        std::cout << "  io status      - Show I/O status\n";
        std::cout << "  memory dump    - Dump memory region\n";
        std::cout << "  pins show      - Show pin configuration\n";
        std::cout << "\nLLM NPU Coprocessor:\n";
        std::cout << "  llm status     - Show LLM NPU status\n";
        std::cout << "  llm infer      - Run inference test\n";
        std::cout << "  llm diag       - Show LLM diagnostics\n";
        std::cout << "  llm selftest   - Run LLM self-test\n";
        std::cout << "\nGeneral:\n";
        std::cout << "  help           - Show this help\n";
        std::cout << "  quit/exit      - Exit utility\n";
    }
    
    void show_device_status() {
        std::cout << "\n" << device->get_status_report() << "\n";
    }
    
    void show_dashboard() {
        std::cout << "\nRefreshing dashboard...\n";
        update_telemetry();
        std::cout << dashboard->render_dashboard();
    }
    
    void show_telemetry() {
        update_telemetry();
        std::cout << "\n" << telemetry->generate_report();
    }
    
    void show_diagnostics() {
        std::cout << "\n" << diagnostics->generate_diagnostic_report();
    }
    
    void configure_device(std::istringstream& iss) {
        std::string subcommand;
        iss >> subcommand;
        
        if (subcommand == "list") {
            auto configs = config_manager->list_configurations();
            std::cout << "\nConfigurations:\n";
            for (const auto& name : configs) {
                std::cout << "  - " << name << "\n";
            }
        } else if (subcommand == "load") {
            std::string name;
            iss >> name;
            if (config_manager->load_configuration(name)) {
                std::cout << "Configuration '" << name << "' loaded\n";
            } else {
                std::cout << "Failed to load configuration\n";
            }
        } else if (subcommand == "save") {
            std::string name;
            iss >> name;
            if (config_manager->save_configuration(name)) {
                std::cout << "Configuration saved\n";
            } else {
                std::cout << "Failed to save configuration\n";
            }
        } else {
            std::cout << "Usage: config [list|load|save]\n";
        }
    }
    
    void driver_operations(std::istringstream& iss) {
        std::string subcommand;
        iss >> subcommand;
        
        if (subcommand == "info") {
            std::cout << "\n=== Driver Information ===\n";
            std::cout << "Name: " << driver->get_name() << "\n";
            std::cout << "Version: " << driver->get_version() << "\n";
            std::cout << "Loaded: " << (driver->is_driver_loaded() ? "Yes" : "No") << "\n";
            std::cout << "Initialized: " << (driver->is_driver_initialized() ? "Yes" : "No") << "\n";
        } else if (subcommand == "test") {
            std::cout << "\n" << driver->get_hardware_diagnostics();
        } else if (subcommand == "probe") {
            std::cout << "\nProbing hardware...\n";
            if (driver->probe()) {
                std::cout << "Probe successful\n";
            } else {
                std::cout << "Probe failed\n";
            }
        } else {
            std::cout << "Usage: driver [info|test|probe]\n";
        }
    }
    
    void run_health_check() {
        std::cout << "\n=== Running Health Check ===\n";
        
        bool device_ok = diagnostics->check_device_health(device.get());
        bool memory_ok = diagnostics->check_memory_integrity(device.get());
        bool io_ok = diagnostics->check_io_health(device.get());
        
        std::cout << "\nHealth Check Results:\n";
        std::cout << "  Device Health: " << (device_ok ? "PASS" : "FAIL") << "\n";
        std::cout << "  Memory Integrity: " << (memory_ok ? "PASS" : "FAIL") << "\n";
        std::cout << "  I/O Health: " << (io_ok ? "PASS" : "FAIL") << "\n";
        
        if (device_ok && memory_ok && io_ok) {
            std::cout << "\n✓ All checks passed\n";
        } else {
            std::cout << "\n✗ Some checks failed\n";
        }
    }
    
    void run_self_test() {
        std::cout << "\n=== Running Hardware Self-Test ===\n";
        driver->run_self_test();
    }
    
    void reset_device() {
        std::cout << "\nResetting device...\n";
        if (device->reset()) {
            diagnostics->log_event(DiagnosticLevel::INFO, "AdminUtil", "Device reset");
            std::cout << "Device reset successful\n";
        } else {
            std::cout << "Device reset failed\n";
        }
    }
    
    void list_devices() {
        auto* registry = DeviceRegistry::get_instance();
        auto devices = registry->list_devices();
        auto drivers = registry->list_drivers();
        
        std::cout << "\n=== Registered Devices ===\n";
        for (const auto& id : devices) {
            auto dev = registry->get_device(id);
            if (dev) {
                std::cout << "  " << id << " (" << dev->get_model() << ")\n";
            }
        }
        
        std::cout << "\n=== Registered Drivers ===\n";
        for (const auto& name : drivers) {
            auto drv = registry->get_driver(name);
            if (drv) {
                std::cout << "  " << name << " v" << drv->get_version() << "\n";
            }
        }
    }
    
    void io_operations(std::istringstream& iss) {
        std::string subcommand;
        iss >> subcommand;
        
        if (subcommand == "list") {
            std::cout << "\n=== I/O Streams ===\n";
            std::cout << "No active streams\n";
        } else if (subcommand == "status") {
            std::cout << "\n=== I/O Status ===\n";
            std::cout << "All I/O streams operational\n";
        } else {
            std::cout << "Usage: io [list|status]\n";
        }
    }
    
    void memory_operations(std::istringstream& iss) {
        std::string subcommand;
        iss >> subcommand;
        
        if (subcommand == "dump") {
            std::string region_name;
            iss >> region_name;
            
            if (region_name.empty()) {
                std::cout << "Usage: memory dump <region_name>\n";
                std::cout << "Available regions: SRAM, FLASH, PERIPH\n";
                return;
            }
            
            auto* region = device->get_memory_region(region_name);
            if (region) {
                std::cout << "\n=== Memory Region: " << region_name << " ===\n";
                std::cout << "Base Address: 0x" << std::hex << region->base_address << "\n";
                std::cout << "Size: " << std::dec << region->size << " bytes\n";
                std::cout << "Writable: " << (region->is_writable ? "Yes" : "No") << "\n";
                
                // Dump first 256 bytes
                std::cout << "\nFirst 256 bytes:\n";
                for (size_t i = 0; i < std::min(size_t(256), region->size); i += 16) {
                    std::cout << std::hex << std::setw(8) << std::setfill('0') 
                             << (region->base_address + i) << ": ";
                    
                    for (size_t j = 0; j < 16 && (i + j) < region->size; j++) {
                        std::cout << std::hex << std::setw(2) << std::setfill('0')
                                 << static_cast<int>(region->read_byte(i + j)) << " ";
                    }
                    std::cout << "\n";
                }
            } else {
                std::cout << "Memory region '" << region_name << "' not found\n";
            }
        } else {
            std::cout << "Usage: memory dump <region_name>\n";
        }
    }
    
    void pin_operations(std::istringstream& iss) {
        std::string subcommand;
        iss >> subcommand;
        
        if (subcommand == "show" || subcommand.empty()) {
            std::cout << "\n=== GPIO Pin Configuration ===\n";
            std::cout << "Financial Account Pin Mapping:\n";
            std::cout << "  Assets (Pins 0-15):\n";
            std::cout << "    GPIO0  - Cash (1101)\n";
            std::cout << "    GPIO1  - Accounts Receivable (1102)\n";
            std::cout << "    GPIO2  - Inventory (1103)\n";
            std::cout << "    GPIO3  - Equipment (1201)\n";
            std::cout << "    GPIO4  - Buildings (1202)\n";
            std::cout << "\n  Liabilities (Pins 16-31):\n";
            std::cout << "    GPIO16 - Accounts Payable (2101)\n";
            std::cout << "    GPIO17 - Short-term Loans (2102)\n";
            std::cout << "    GPIO18 - Long-term Loans (2201)\n";
            std::cout << "\n  Equity (Pins 32-39):\n";
            std::cout << "    GPIO32 - Owner's Equity (3100)\n";
            std::cout << "    GPIO33 - Retained Earnings (3200)\n";
            std::cout << "\n  Revenue (Pins 40-47):\n";
            std::cout << "    GPIO40 - Sales Revenue (4100)\n";
            std::cout << "    GPIO41 - Service Revenue (4200)\n";
            std::cout << "\n  Expenses (Pins 48-63):\n";
            std::cout << "    GPIO48 - Salaries (5101)\n";
            std::cout << "    GPIO49 - Rent (5102)\n";
            std::cout << "    GPIO50 - Utilities (5103)\n";
        } else {
            std::cout << "Usage: pins show\n";
        }
    }
    
    void llm_operations(std::istringstream& iss) {
        std::string subcommand;
        iss >> subcommand;
        
        if (!llm_driver) {
            std::cout << "LLM driver not available\n";
            return;
        }
        
        if (subcommand == "status") {
            std::cout << "\n" << llm_driver->get_device_status_string() << "\n";
        } else if (subcommand == "infer") {
            std::cout << "\n=== Running LLM Inference Test ===\n";
            LlamaSequenceConfig seq;
            seq.n_predict = 64;
            seq.echo_prompt = true;
            
            std::string result = llm_driver->infer("Explain the balance sheet like a circuit board", seq);
            std::cout << result << "\n";
        } else if (subcommand == "diag") {
            std::cout << "\n" << llm_driver->get_hardware_diagnostics() << "\n";
        } else if (subcommand == "selftest") {
            std::cout << "\n=== Running LLM Self-Test ===\n";
            if (llm_driver->run_self_test()) {
                std::cout << "✓ LLM self-test passed\n";
            } else {
                std::cout << "✗ LLM self-test failed\n";
            }
        } else if (subcommand == "streaming") {
            std::cout << "\n=== Running LLM Streaming Test ===\n";
            LlamaSequenceConfig seq;
            seq.n_predict = 32;
            seq.stream_tokens = true;
            
            llm_driver->infer_streaming("Test streaming inference", seq,
                [](const std::string& token, int32_t token_id, bool is_last) {
                    std::cout << token << " " << std::flush;
                    if (is_last) {
                        std::cout << "\n";
                    }
                });
        } else {
            std::cout << "Usage: llm [status|infer|diag|selftest|streaming]\n";
        }
    }
    
    void shutdown() {
        std::cout << "\nShutting down...\n";
        
        if (device) {
            device->stop();
            device->shutdown();
        }
        
        if (driver) {
            driver->remove();
        }
        
        diagnostics->log_event(DiagnosticLevel::INFO, "AdminUtil", 
                              "System shutdown complete");
        
        running = false;
        std::cout << "Shutdown complete.\n";
    }
};

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    AdminUtility admin;
    
    if (!admin.initialize()) {
        std::cerr << "Failed to initialize admin utility\n";
        return 1;
    }
    
    admin.run_interactive();
    
    return 0;
}
