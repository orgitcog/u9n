#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <queue>
#include <mutex>
#include <functional>
#include <chrono>
#include <cstdint>

namespace ggnucash {
namespace vdev {

// ============================================================================
// Forward declarations
// ============================================================================

class VirtualPCB;
class DeviceDriver;

// ============================================================================
// Memory Region
// ============================================================================

struct MemoryRegion {
    uint64_t base_address;
    uint64_t size;
    std::string name;
    std::vector<uint8_t> data;
    bool is_writable;
    
    // Note: Constructor allocates memory eagerly to simulate physical memory.
    // For very large regions, consider implementing demand-paging or sparse allocation.
    MemoryRegion(uint64_t addr, uint64_t sz, const std::string& n)
        : base_address(addr), size(sz), name(n), is_writable(true) {
        data.resize(size, 0);
    }
    
    uint8_t read_byte(uint64_t offset);
    uint16_t read_word(uint64_t offset);
    uint32_t read_dword(uint64_t offset);
    
    void write_byte(uint64_t offset, uint8_t value);
    void write_word(uint64_t offset, uint16_t value);
    void write_dword(uint64_t offset, uint32_t value);
};

// ============================================================================
// DMA Controller
// ============================================================================

struct DMATransfer {
    uint64_t source_addr;
    uint64_t dest_addr;
    size_t length;
    bool completed;
    std::chrono::steady_clock::time_point start_time;
};

class DMAController {
private:
    std::vector<DMATransfer> active_transfers;
    size_t max_channels;
    std::mutex transfer_mutex;
    
public:
    DMAController(size_t channels = 8) : max_channels(channels) {}
    
    int start_transfer(uint64_t src, uint64_t dst, size_t len);
    bool is_transfer_complete(int channel);
    void abort_transfer(int channel);
    void process_transfers();
};

// ============================================================================
// Interrupt Controller
// ============================================================================

enum class InterruptType {
    EDGE_RISING,
    EDGE_FALLING,
    LEVEL_HIGH,
    LEVEL_LOW
};

struct InterruptDescriptor {
    uint32_t vector;
    InterruptType type;
    uint8_t priority;
    bool enabled;
    std::function<void()> handler;
    
    InterruptDescriptor() : vector(0), type(InterruptType::EDGE_RISING),
                           priority(0), enabled(false) {}
};

class InterruptController {
private:
    std::vector<InterruptDescriptor> interrupts;
    std::queue<uint32_t> pending_interrupts;
    bool global_interrupt_enable;
    std::mutex interrupt_mutex;
    
public:
    InterruptController(size_t num_vectors = 256) 
        : interrupts(num_vectors), global_interrupt_enable(true) {}
    
    bool register_interrupt(uint32_t vector, InterruptType type,
                           uint8_t priority, std::function<void()> handler);
    void trigger_interrupt(uint32_t vector);
    void enable_interrupt(uint32_t vector);
    void disable_interrupt(uint32_t vector);
    void enable_global_interrupts();
    void disable_global_interrupts();
    void process_interrupts();
};

// ============================================================================
// I/O Streams
// ============================================================================

class IOStream {
protected:
    std::string stream_name;
    bool is_open;
    
public:
    IOStream(const std::string& name) : stream_name(name), is_open(false) {}
    virtual ~IOStream() = default;
    
    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual size_t write(const uint8_t* data, size_t length) = 0;
    virtual size_t read(uint8_t* data, size_t length) = 0;
    virtual size_t available() const = 0;
    virtual bool flush() = 0;
    
    bool is_stream_open() const { return is_open; }
    std::string get_name() const { return stream_name; }
};

class UARTStream : public IOStream {
private:
    uint32_t baud_rate;
    std::queue<uint8_t> rx_buffer;
    std::queue<uint8_t> tx_buffer;
    size_t rx_buffer_size;
    size_t tx_buffer_size;
    mutable std::mutex buffer_mutex;
    
public:
    UARTStream(const std::string& name, uint32_t baud = 115200)
        : IOStream(name), baud_rate(baud), 
          rx_buffer_size(1024), tx_buffer_size(1024) {}
    
    bool open() override;
    bool close() override;
    size_t write(const uint8_t* data, size_t length) override;
    size_t read(uint8_t* data, size_t length) override;
    size_t available() const override;
    bool flush() override;
    
    void set_baud_rate(uint32_t baud) { baud_rate = baud; }
    uint32_t get_baud_rate() const { return baud_rate; }
};

class SPIStream : public IOStream {
private:
    uint32_t clock_speed;
    uint8_t mode;
    std::queue<uint8_t> rx_buffer;
    std::queue<uint8_t> tx_buffer;
    size_t rx_buffer_size;
    size_t tx_buffer_size;
    mutable std::mutex buffer_mutex;
    
public:
    SPIStream(const std::string& name, uint32_t speed = 1000000, uint8_t spi_mode = 0)
        : IOStream(name), clock_speed(speed), mode(spi_mode),
          rx_buffer_size(1024), tx_buffer_size(1024) {}
    
    bool open() override;
    bool close() override;
    size_t write(const uint8_t* data, size_t length) override;
    size_t read(uint8_t* data, size_t length) override;
    size_t available() const override;
    bool flush() override;
};

class I2CStream : public IOStream {
private:
    uint8_t device_address;
    uint32_t clock_speed;
    std::queue<uint8_t> rx_buffer;
    std::queue<uint8_t> tx_buffer;
    size_t rx_buffer_size;
    size_t tx_buffer_size;
    mutable std::mutex buffer_mutex;
    
public:
    I2CStream(const std::string& name, uint8_t addr = 0x00, uint32_t speed = 100000)
        : IOStream(name), device_address(addr), clock_speed(speed),
          rx_buffer_size(1024), tx_buffer_size(1024) {}
    
    bool open() override;
    bool close() override;
    size_t write(const uint8_t* data, size_t length) override;
    size_t read(uint8_t* data, size_t length) override;
    size_t available() const override;
    bool flush() override;
    
    bool write_register(uint8_t reg, uint8_t value);
    uint8_t read_register(uint8_t reg);
};

// ============================================================================
// GPIO Pin Configuration
// ============================================================================

enum class PinMode {
    INPUT,
    OUTPUT,
    ANALOG,
    ALTERNATE
};

enum class PinState {
    LOW,
    HIGH,
    FLOATING
};

struct GPIOPin {
    uint32_t number;
    std::string name;
    PinMode mode;
    PinState state;
    uint16_t analog_value;  // 12-bit ADC value
    bool interrupt_enabled;
    std::function<void()> interrupt_handler;
    
    GPIOPin() : number(0), mode(PinMode::INPUT), state(PinState::FLOATING),
                analog_value(0), interrupt_enabled(false) {}
};

// ============================================================================
// Device State
// ============================================================================

enum class DeviceState {
    UNINITIALIZED,
    INITIALIZING,
    READY,
    RUNNING,
    SLEEPING,
    ERROR,
    SHUTDOWN
};

// ============================================================================
// Virtual PCB (Printed Circuit Board)
// ============================================================================

class VirtualPCB {
private:
    std::string device_id;
    std::string model_number;
    std::string firmware_version;
    DeviceState state;
    
    // Hardware components
    std::unique_ptr<DMAController> dma_controller;
    std::unique_ptr<InterruptController> interrupt_controller;
    std::map<uint32_t, GPIOPin> pins;
    std::vector<std::shared_ptr<MemoryRegion>> memory_regions;
    std::map<std::string, std::shared_ptr<IOStream>> io_streams;
    
    // Power and thermal
    float voltage_3v3;
    float voltage_5v;
    float current_consumption_ma;
    float temperature_celsius;
    
    // Timing
    std::chrono::steady_clock::time_point boot_time;
    std::chrono::steady_clock::time_point last_update;
    uint64_t system_ticks;
    
    mutable std::mutex device_mutex;
    
public:
    VirtualPCB(const std::string& id, const std::string& model);
    ~VirtualPCB();
    
    // Lifecycle management
    bool initialize();
    bool start();
    bool stop();
    bool reset();
    bool shutdown();
    
    // Pin operations
    bool configure_pin(uint32_t pin_num, PinMode mode);
    bool set_pin_state(uint32_t pin_num, PinState state);
    PinState get_pin_state(uint32_t pin_num);
    bool set_analog_value(uint32_t pin_num, uint16_t value);
    uint16_t get_analog_value(uint32_t pin_num);
    
    // I/O stream management
    bool add_io_stream(const std::string& name, std::shared_ptr<IOStream> stream);
    std::shared_ptr<IOStream> get_io_stream(const std::string& name);
    
    // Memory management
    bool add_memory_region(const std::string& name, uint64_t addr, uint64_t size);
    MemoryRegion* get_memory_region(const std::string& name);
    uint8_t read_memory(uint64_t address);
    void write_memory(uint64_t address, uint8_t value);
    
    // Hardware access
    DMAController* get_dma_controller() { return dma_controller.get(); }
    InterruptController* get_interrupt_controller() { return interrupt_controller.get(); }
    
    // Status and monitoring
    DeviceState get_state() const { return state; }
    std::string get_device_id() const { return device_id; }
    std::string get_model() const { return model_number; }
    std::string get_firmware_version() const { return firmware_version; }
    uint64_t get_uptime_ms() const;
    
    float get_voltage_3v3() const { return voltage_3v3; }
    float get_voltage_5v() const { return voltage_5v; }
    float get_current() const { return current_consumption_ma; }
    float get_temperature() const { return temperature_celsius; }
    
    void update();
    std::string get_status_report() const;
};

// ============================================================================
// Device Driver Base Class
// ============================================================================

class DeviceDriver {
protected:
    std::string driver_name;
    std::string driver_version;
    bool loaded;
    bool initialized;
    
public:
    DeviceDriver(const std::string& name, const std::string& version)
        : driver_name(name), driver_version(version), loaded(false), initialized(false) {}
    
    virtual ~DeviceDriver() = default;
    
    // Driver lifecycle
    virtual bool load(VirtualPCB* pcb) = 0;
    virtual bool initialize() = 0;
    virtual bool probe() = 0;
    virtual bool remove() = 0;
    
    // Driver information
    std::string get_name() const { return driver_name; }
    std::string get_version() const { return driver_version; }
    bool is_driver_loaded() const { return loaded; }
    bool is_driver_initialized() const { return initialized; }
};

// ============================================================================
// Firmware Controller
// ============================================================================

class FirmwareController {
private:
    VirtualPCB* pcb;
    std::string firmware_path;
    bool is_loaded;
    std::vector<std::function<void()>> boot_sequence;
    std::map<std::string, std::string> config_params;
    
public:
    FirmwareController(VirtualPCB* device = nullptr) 
        : pcb(device), is_loaded(false) {}
    
    bool load_firmware(const std::string& path);
    bool execute_boot_sequence();
    bool configure_parameter(const std::string& param, const std::string& value);
    std::string get_parameter(const std::string& param);
    
    void handle_watchdog_timer();
    void handle_power_state_change(DeviceState new_state);
    void handle_error_condition(const std::string& error);
};

// ============================================================================
// Device Registry (Singleton)
// ============================================================================

class DeviceRegistry {
private:
    std::map<std::string, std::shared_ptr<VirtualPCB>> devices;
    std::map<std::string, std::shared_ptr<DeviceDriver>> drivers;
    std::mutex registry_mutex;
    
    static DeviceRegistry* instance;
    
    DeviceRegistry() = default;
    DeviceRegistry(const DeviceRegistry&) = delete;
    DeviceRegistry& operator=(const DeviceRegistry&) = delete;
    
public:
    static DeviceRegistry* get_instance();
    
    bool register_device(const std::string& id, std::shared_ptr<VirtualPCB> device);
    bool unregister_device(const std::string& id);
    std::shared_ptr<VirtualPCB> get_device(const std::string& id);
    std::vector<std::string> list_devices();
    
    bool register_driver(const std::string& name, std::shared_ptr<DeviceDriver> driver);
    bool unregister_driver(const std::string& name);
    std::shared_ptr<DeviceDriver> get_driver(const std::string& name);
    std::vector<std::string> list_drivers();
};

} // namespace vdev
} // namespace ggnucash
