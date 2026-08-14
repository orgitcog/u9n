#include "virtual-device.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ggnucash {
namespace vdev {

// ============================================================================
// Memory Region Implementation
// ============================================================================

uint8_t MemoryRegion::read_byte(uint64_t offset) {
    if (offset >= size) return 0;
    return data[offset];
}

uint16_t MemoryRegion::read_word(uint64_t offset) {
    if (offset + 1 >= size) return 0;
    return (data[offset + 1] << 8) | data[offset];
}

uint32_t MemoryRegion::read_dword(uint64_t offset) {
    if (offset + 3 >= size) return 0;
    return (data[offset + 3] << 24) | (data[offset + 2] << 16) | 
           (data[offset + 1] << 8) | data[offset];
}

void MemoryRegion::write_byte(uint64_t offset, uint8_t value) {
    if (offset < size && is_writable) {
        data[offset] = value;
    }
}

void MemoryRegion::write_word(uint64_t offset, uint16_t value) {
    if (offset + 1 < size && is_writable) {
        data[offset] = value & 0xFF;
        data[offset + 1] = (value >> 8) & 0xFF;
    }
}

void MemoryRegion::write_dword(uint64_t offset, uint32_t value) {
    if (offset + 3 < size && is_writable) {
        data[offset] = value & 0xFF;
        data[offset + 1] = (value >> 8) & 0xFF;
        data[offset + 2] = (value >> 16) & 0xFF;
        data[offset + 3] = (value >> 24) & 0xFF;
    }
}

// ============================================================================
// DMA Controller Implementation
// ============================================================================

int DMAController::start_transfer(uint64_t src, uint64_t dst, size_t len) {
    std::lock_guard<std::mutex> lock(transfer_mutex);
    
    if (active_transfers.size() >= max_channels) {
        return -1;  // No available channels
    }
    
    DMATransfer transfer;
    transfer.source_addr = src;
    transfer.dest_addr = dst;
    transfer.length = len;
    transfer.completed = false;
    transfer.start_time = std::chrono::steady_clock::now();
    
    active_transfers.push_back(transfer);
    return static_cast<int>(active_transfers.size() - 1);
}

bool DMAController::is_transfer_complete(int channel) {
    std::lock_guard<std::mutex> lock(transfer_mutex);
    
    if (channel < 0 || channel >= static_cast<int>(active_transfers.size())) {
        return false;
    }
    
    return active_transfers[channel].completed;
}

void DMAController::abort_transfer(int channel) {
    std::lock_guard<std::mutex> lock(transfer_mutex);
    
    if (channel >= 0 && channel < static_cast<int>(active_transfers.size())) {
        active_transfers.erase(active_transfers.begin() + channel);
    }
}

void DMAController::process_transfers() {
    std::lock_guard<std::mutex> lock(transfer_mutex);
    
    for (auto& transfer : active_transfers) {
        if (!transfer.completed) {
            // Simulate transfer completion based on time
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                now - transfer.start_time).count();
            
            // Assume 1GB/s transfer rate: 1 byte per nanosecond
            if (elapsed >= transfer.length) {
                transfer.completed = true;
            }
        }
    }
}

// ============================================================================
// Interrupt Controller Implementation
// ============================================================================

bool InterruptController::register_interrupt(uint32_t vector, InterruptType type,
                                            uint8_t priority, std::function<void()> handler) {
    std::lock_guard<std::mutex> lock(interrupt_mutex);
    
    if (vector >= interrupts.size()) {
        return false;
    }
    
    interrupts[vector].vector = vector;
    interrupts[vector].type = type;
    interrupts[vector].priority = priority;
    interrupts[vector].handler = handler;
    interrupts[vector].enabled = true;
    
    return true;
}

void InterruptController::trigger_interrupt(uint32_t vector) {
    std::lock_guard<std::mutex> lock(interrupt_mutex);
    
    if (vector < interrupts.size() && interrupts[vector].enabled && global_interrupt_enable) {
        pending_interrupts.push(vector);
    }
}

void InterruptController::enable_interrupt(uint32_t vector) {
    std::lock_guard<std::mutex> lock(interrupt_mutex);
    
    if (vector < interrupts.size()) {
        interrupts[vector].enabled = true;
    }
}

void InterruptController::disable_interrupt(uint32_t vector) {
    std::lock_guard<std::mutex> lock(interrupt_mutex);
    
    if (vector < interrupts.size()) {
        interrupts[vector].enabled = false;
    }
}

void InterruptController::enable_global_interrupts() {
    std::lock_guard<std::mutex> lock(interrupt_mutex);
    global_interrupt_enable = true;
}

void InterruptController::disable_global_interrupts() {
    std::lock_guard<std::mutex> lock(interrupt_mutex);
    global_interrupt_enable = false;
}

void InterruptController::process_interrupts() {
    std::lock_guard<std::mutex> lock(interrupt_mutex);
    
    while (!pending_interrupts.empty() && global_interrupt_enable) {
        uint32_t vector = pending_interrupts.front();
        pending_interrupts.pop();
        
        if (vector < interrupts.size() && interrupts[vector].handler) {
            interrupts[vector].handler();
        }
    }
}

// ============================================================================
// UART Stream Implementation
// ============================================================================

bool UARTStream::open() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    // Simulate UART opening
    is_open = true;
    return true;
}

bool UARTStream::close() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    is_open = false;
    while (!rx_buffer.empty()) rx_buffer.pop();
    while (!tx_buffer.empty()) tx_buffer.pop();
    return true;
}

size_t UARTStream::write(const uint8_t* data, size_t length) {
    if (!is_open) return 0;
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    size_t written = 0;
    
    for (size_t i = 0; i < length && tx_buffer.size() < tx_buffer_size; i++) {
        tx_buffer.push(data[i]);
        written++;
    }
    
    return written;
}

size_t UARTStream::read(uint8_t* data, size_t length) {
    if (!is_open) return 0;
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    size_t bytes_read = 0;
    
    while (bytes_read < length && !rx_buffer.empty()) {
        data[bytes_read++] = rx_buffer.front();
        rx_buffer.pop();
    }
    
    return bytes_read;
}

size_t UARTStream::available() const {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    return rx_buffer.size();
}

bool UARTStream::flush() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    while (!tx_buffer.empty()) tx_buffer.pop();
    return true;
}

// ============================================================================
// SPI Stream Implementation
// ============================================================================

bool SPIStream::open() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    is_open = true;
    return true;
}

bool SPIStream::close() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    is_open = false;
    while (!rx_buffer.empty()) rx_buffer.pop();
    while (!tx_buffer.empty()) tx_buffer.pop();
    return true;
}

size_t SPIStream::write(const uint8_t* data, size_t length) {
    if (!is_open) return 0;
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    size_t written = 0;
    
    for (size_t i = 0; i < length && tx_buffer.size() < tx_buffer_size; i++) {
        tx_buffer.push(data[i]);
        written++;
    }
    
    return written;
}

size_t SPIStream::read(uint8_t* data, size_t length) {
    if (!is_open) return 0;
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    size_t bytes_read = 0;
    
    while (bytes_read < length && !rx_buffer.empty()) {
        data[bytes_read++] = rx_buffer.front();
        rx_buffer.pop();
    }
    
    return bytes_read;
}

size_t SPIStream::available() const {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    return rx_buffer.size();
}

bool SPIStream::flush() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    while (!tx_buffer.empty()) tx_buffer.pop();
    return true;
}

// ============================================================================
// I2C Stream Implementation
// ============================================================================

bool I2CStream::open() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    is_open = true;
    return true;
}

bool I2CStream::close() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    is_open = false;
    while (!rx_buffer.empty()) rx_buffer.pop();
    while (!tx_buffer.empty()) tx_buffer.pop();
    return true;
}

size_t I2CStream::write(const uint8_t* data, size_t length) {
    if (!is_open) return 0;
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    size_t written = 0;
    
    for (size_t i = 0; i < length && tx_buffer.size() < tx_buffer_size; i++) {
        tx_buffer.push(data[i]);
        written++;
    }
    
    return written;
}

size_t I2CStream::read(uint8_t* data, size_t length) {
    if (!is_open) return 0;
    
    std::lock_guard<std::mutex> lock(buffer_mutex);
    size_t bytes_read = 0;
    
    while (bytes_read < length && !rx_buffer.empty()) {
        data[bytes_read++] = rx_buffer.front();
        rx_buffer.pop();
    }
    
    return bytes_read;
}

size_t I2CStream::available() const {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    return rx_buffer.size();
}

bool I2CStream::flush() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    while (!tx_buffer.empty()) tx_buffer.pop();
    return true;
}

bool I2CStream::write_register(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return write(data, 2) == 2;
}

uint8_t I2CStream::read_register(uint8_t reg) {
    write(&reg, 1);
    uint8_t value = 0;
    read(&value, 1);
    return value;
}

// ============================================================================
// Virtual PCB Implementation
// ============================================================================

VirtualPCB::VirtualPCB(const std::string& id, const std::string& model)
    : device_id(id), model_number(model), firmware_version("1.0.0"),
      state(DeviceState::UNINITIALIZED), system_ticks(0),
      voltage_3v3(3.3f), voltage_5v(5.0f), 
      current_consumption_ma(50.0f), temperature_celsius(25.0f) {
    
    // Initialize hardware components
    dma_controller = std::make_unique<DMAController>(8);
    interrupt_controller = std::make_unique<InterruptController>();
    
    // Initialize default pin configuration (64 GPIO pins)
    for (uint32_t i = 0; i < 64; i++) {
        pins[i].number = i;
        pins[i].name = "GPIO" + std::to_string(i);
        pins[i].mode = PinMode::INPUT;
        pins[i].state = PinState::FLOATING;
    }
}

VirtualPCB::~VirtualPCB() {
    shutdown();
}

bool VirtualPCB::initialize() {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    if (state != DeviceState::UNINITIALIZED) {
        return false;
    }
    
    state = DeviceState::INITIALIZING;
    
    // Add default memory regions (do this inline to avoid deadlock with add_memory_region)
    auto sram_region = std::make_shared<MemoryRegion>(0x20000000, 256 * 1024, "SRAM");
    memory_regions.push_back(sram_region);
    
    auto flash_region = std::make_shared<MemoryRegion>(0x08000000, 1024 * 1024, "FLASH");
    memory_regions.push_back(flash_region);
    
    auto periph_region = std::make_shared<MemoryRegion>(0x40000000, 64 * 1024, "PERIPH");
    memory_regions.push_back(periph_region);
    
    boot_time = std::chrono::steady_clock::now();
    last_update = boot_time;
    system_ticks = 0;
    
    state = DeviceState::READY;
    return true;
}

bool VirtualPCB::start() {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    if (state != DeviceState::READY) {
        return false;
    }
    
    state = DeviceState::RUNNING;
    return true;
}

bool VirtualPCB::stop() {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    if (state != DeviceState::RUNNING) {
        return false;
    }
    
    state = DeviceState::READY;
    return true;
}

bool VirtualPCB::reset() {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    // Reset all pins
    for (auto& pin_pair : pins) {
        pin_pair.second.state = PinState::FLOATING;
        pin_pair.second.analog_value = 0;
    }
    
    // Clear memory regions
    for (auto& region : memory_regions) {
        std::fill(region->data.begin(), region->data.end(), 0);
    }
    
    system_ticks = 0;
    boot_time = std::chrono::steady_clock::now();
    last_update = boot_time;
    
    state = DeviceState::READY;
    return true;
}

bool VirtualPCB::shutdown() {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    // Close all I/O streams
    for (auto& stream_pair : io_streams) {
        stream_pair.second->close();
    }
    
    state = DeviceState::SHUTDOWN;
    return true;
}

bool VirtualPCB::configure_pin(uint32_t pin_num, PinMode mode) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    auto it = pins.find(pin_num);
    if (it == pins.end()) {
        return false;
    }
    
    it->second.mode = mode;
    return true;
}

bool VirtualPCB::set_pin_state(uint32_t pin_num, PinState state_val) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    auto it = pins.find(pin_num);
    if (it == pins.end()) {
        return false;
    }
    
    it->second.state = state_val;
    
    // Trigger interrupt if enabled
    if (it->second.interrupt_enabled && it->second.interrupt_handler) {
        it->second.interrupt_handler();
    }
    
    return true;
}

PinState VirtualPCB::get_pin_state(uint32_t pin_num) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    auto it = pins.find(pin_num);
    if (it == pins.end()) {
        return PinState::FLOATING;
    }
    
    return it->second.state;
}

bool VirtualPCB::set_analog_value(uint32_t pin_num, uint16_t value) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    auto it = pins.find(pin_num);
    if (it == pins.end()) {
        return false;
    }
    
    it->second.analog_value = value & 0x0FFF;  // 12-bit ADC
    return true;
}

uint16_t VirtualPCB::get_analog_value(uint32_t pin_num) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    auto it = pins.find(pin_num);
    if (it == pins.end()) {
        return 0;
    }
    
    return it->second.analog_value;
}

bool VirtualPCB::add_io_stream(const std::string& name, std::shared_ptr<IOStream> stream) {
    std::lock_guard<std::mutex> lock(device_mutex);
    io_streams[name] = stream;
    return true;
}

std::shared_ptr<IOStream> VirtualPCB::get_io_stream(const std::string& name) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    auto it = io_streams.find(name);
    if (it == io_streams.end()) {
        return nullptr;
    }
    
    return it->second;
}

bool VirtualPCB::add_memory_region(const std::string& name, uint64_t addr, uint64_t size) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    auto region = std::make_shared<MemoryRegion>(addr, size, name);
    memory_regions.push_back(region);
    return true;
}

MemoryRegion* VirtualPCB::get_memory_region(const std::string& name) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    for (auto& region : memory_regions) {
        if (region->name == name) {
            return region.get();
        }
    }
    
    return nullptr;
}

uint8_t VirtualPCB::read_memory(uint64_t address) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    for (auto& region : memory_regions) {
        if (address >= region->base_address && 
            address < region->base_address + region->size) {
            return region->read_byte(address - region->base_address);
        }
    }
    
    return 0;
}

void VirtualPCB::write_memory(uint64_t address, uint8_t value) {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    for (auto& region : memory_regions) {
        if (address >= region->base_address && 
            address < region->base_address + region->size) {
            region->write_byte(address - region->base_address, value);
            return;
        }
    }
}

uint64_t VirtualPCB::get_uptime_ms() const {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - boot_time);
    return uptime.count();
}

void VirtualPCB::update() {
    std::lock_guard<std::mutex> lock(device_mutex);
    
    if (state != DeviceState::RUNNING) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
        now - last_update);
    
    system_ticks += delta.count();
    last_update = now;
    
    // Update DMA transfers
    dma_controller->process_transfers();
    
    // Process interrupts
    interrupt_controller->process_interrupts();
    
    // Simulate temperature variation
    temperature_celsius = 25.0f + (system_ticks % 10000) / 1000.0f;
}

std::string VirtualPCB::get_status_report() const {
    std::stringstream ss;
    
    ss << "=== Virtual Device Status Report ===\n";
    ss << "Device ID: " << device_id << "\n";
    ss << "Model: " << model_number << "\n";
    ss << "Firmware: " << firmware_version << "\n";
    ss << "State: ";
    
    switch (state) {
        case DeviceState::UNINITIALIZED: ss << "UNINITIALIZED"; break;
        case DeviceState::INITIALIZING: ss << "INITIALIZING"; break;
        case DeviceState::READY: ss << "READY"; break;
        case DeviceState::RUNNING: ss << "RUNNING"; break;
        case DeviceState::SLEEPING: ss << "SLEEPING"; break;
        case DeviceState::ERROR: ss << "ERROR"; break;
        case DeviceState::SHUTDOWN: ss << "SHUTDOWN"; break;
    }
    ss << "\n";
    
    ss << "Uptime: " << get_uptime_ms() << " ms\n";
    ss << "System Ticks: " << system_ticks << "\n";
    ss << "Power:\n";
    ss << "  3.3V: " << std::fixed << std::setprecision(2) << voltage_3v3 << " V\n";
    ss << "  5.0V: " << std::fixed << std::setprecision(2) << voltage_5v << " V\n";
    ss << "  Current: " << std::fixed << std::setprecision(2) << current_consumption_ma << " mA\n";
    ss << "Temperature: " << std::fixed << std::setprecision(1) << temperature_celsius << " °C\n";
    ss << "Memory Regions: " << memory_regions.size() << "\n";
    ss << "I/O Streams: " << io_streams.size() << "\n";
    ss << "GPIO Pins: " << pins.size() << "\n";
    
    return ss.str();
}

// ============================================================================
// Firmware Controller Implementation
// ============================================================================

bool FirmwareController::load_firmware(const std::string& path) {
    firmware_path = path;
    is_loaded = true;
    return true;
}

bool FirmwareController::execute_boot_sequence() {
    if (!pcb) return false;
    
    for (auto& boot_step : boot_sequence) {
        boot_step();
    }
    
    return true;
}

bool FirmwareController::configure_parameter(const std::string& param, const std::string& value) {
    config_params[param] = value;
    return true;
}

std::string FirmwareController::get_parameter(const std::string& param) {
    auto it = config_params.find(param);
    if (it != config_params.end()) {
        return it->second;
    }
    return "";
}

void FirmwareController::handle_watchdog_timer() {
    // Watchdog timer implementation
    if (pcb && pcb->get_state() == DeviceState::RUNNING) {
        // Reset watchdog
    }
}

void FirmwareController::handle_power_state_change(DeviceState new_state) {
    // Handle power state transitions
    if (pcb) {
        // Log state change
        // In a full implementation, this would handle power management
        // such as entering/exiting sleep modes, saving state, etc.
        (void)new_state;  // Future use
    }
}

void FirmwareController::handle_error_condition(const std::string& error) {
    // Log error and potentially trigger recovery
    // In a full implementation, this would handle error recovery,
    // such as resetting peripherals, logging to flash, etc.
    (void)error;  // Future use
}

// ============================================================================
// Device Registry Implementation
// ============================================================================

DeviceRegistry* DeviceRegistry::instance = nullptr;

DeviceRegistry* DeviceRegistry::get_instance() {
    // Thread-safe lazy initialization using C++11 magic statics
    static DeviceRegistry registry;
    return &registry;
}

bool DeviceRegistry::register_device(const std::string& id, std::shared_ptr<VirtualPCB> device) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    devices[id] = device;
    return true;
}

bool DeviceRegistry::unregister_device(const std::string& id) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    auto it = devices.find(id);
    if (it != devices.end()) {
        devices.erase(it);
        return true;
    }
    return false;
}

std::shared_ptr<VirtualPCB> DeviceRegistry::get_device(const std::string& id) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    auto it = devices.find(id);
    if (it != devices.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> DeviceRegistry::list_devices() {
    std::lock_guard<std::mutex> lock(registry_mutex);
    std::vector<std::string> device_list;
    for (const auto& pair : devices) {
        device_list.push_back(pair.first);
    }
    return device_list;
}

bool DeviceRegistry::register_driver(const std::string& name, std::shared_ptr<DeviceDriver> driver) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    drivers[name] = driver;
    return true;
}

bool DeviceRegistry::unregister_driver(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    auto it = drivers.find(name);
    if (it != drivers.end()) {
        drivers.erase(it);
        return true;
    }
    return false;
}

std::shared_ptr<DeviceDriver> DeviceRegistry::get_driver(const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    auto it = drivers.find(name);
    if (it != drivers.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> DeviceRegistry::list_drivers() {
    std::lock_guard<std::mutex> lock(registry_mutex);
    std::vector<std::string> driver_list;
    for (const auto& pair : drivers) {
        driver_list.push_back(pair.first);
    }
    return driver_list;
}

} // namespace vdev
} // namespace ggnucash
