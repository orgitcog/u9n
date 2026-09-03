# GGNuCash Virtual Device Architecture

## Overview

The GGNuCash Virtual Device Architecture provides a complete simulation of a physical hardware device for the Financial Hardware Platform. It models a Printed Circuit Board (PCB) with GPIO pins, memory regions, I/O interfaces, and firmware as if it were a real physical device.

## Architecture Components

### 1. Virtual PCB (Printed Circuit Board)

The `VirtualPCB` class represents the main hardware device with the following components:

#### Hardware Features
- **64 GPIO Pins**: Configurable as input, output, analog, PWM, or interrupt pins
- **Memory Regions**:
  - SRAM: 256KB at 0x20000000
  - FLASH: 1MB at 0x08000000
  - PERIPHERAL: 64KB at 0x40000000
- **DMA Controller**: 8-channel Direct Memory Access for high-speed data transfer
- **Interrupt Controller**: 256 interrupt vectors with priority-based handling
- **Power Management**: Voltage rails (3.3V, 5V) and current monitoring
- **Temperature Monitoring**: Simulated thermal sensor

#### Device States
- `UNINITIALIZED`: Device not yet initialized
- `INITIALIZING`: Boot sequence in progress
- `READY`: Device initialized and ready to start
- `RUNNING`: Device actively processing
- `SLEEPING`: Low-power sleep mode
- `ERROR`: Error state requiring recovery
- `SHUTDOWN`: Device powered down

### 2. I/O Streams

Multiple communication protocols are supported:

#### UART (Universal Asynchronous Receiver/Transmitter)
```cpp
auto uart = std::make_shared<UARTStream>("/dev/ttyUSB0");
UARTConfig config;
config.baud_rate = 115200;
config.data_bits = 8;
config.parity = 'N';
uart->configure(config);
uart->open();
```

#### SPI (Serial Peripheral Interface)
```cpp
auto spi = std::make_shared<SPIStream>("/dev/spidev0.0");
SPIConfig config;
config.clock_speed = 1000000; // 1 MHz
config.mode = 0;
spi->configure(config);
spi->open();
```

#### I2C (Inter-Integrated Circuit)
```cpp
auto i2c = std::make_shared<I2CStream>("/dev/i2c-1");
I2CConfig config;
config.address = 0x3C;
config.clock_speed = 100000; // 100 kHz
i2c->configure(config);
i2c->open();
```

### 3. Financial Device Driver

The `FinancialDeviceDriver` maps financial accounts to hardware pins:

#### Account-to-Pin Mapping
- **Assets (Pins 0-15)**
  - GPIO0: Cash (Account 1101)
  - GPIO1: Accounts Receivable (1102)
  - GPIO2: Inventory (1103)
  - GPIO3: Equipment (1201)
  - GPIO4: Buildings (1202)

- **Liabilities (Pins 16-31)**
  - GPIO16: Accounts Payable (2101)
  - GPIO17: Short-term Loans (2102)
  - GPIO18: Long-term Loans (2201)

- **Equity (Pins 32-39)**
  - GPIO32: Owner's Equity (3100)
  - GPIO33: Retained Earnings (3200)

- **Revenue (Pins 40-47)**
  - GPIO40: Sales Revenue (4100)
  - GPIO41: Service Revenue (4200)

- **Expenses (Pins 48-63)**
  - GPIO48: Salaries (5101)
  - GPIO49: Rent (5102)
  - GPIO50: Utilities (5103)

#### Balance-to-Voltage Mapping
Account balances are represented as analog voltages on GPIO pins using a logarithmic scale:
- 0-3.3V range (12-bit ADC: 0-4095)
- Logarithmic mapping for large balance ranges
- Real-time balance updates reflected in pin voltages

### 4. Admin Configuration Utility

The admin utility (`llama-device-admin`) provides comprehensive device management:

#### Features
1. **Device Status Monitoring**
   - Real-time device state
   - Uptime tracking
   - Power consumption monitoring
   - Temperature monitoring

2. **Telemetry System**
   - Automatic metric collection
   - Statistical analysis (min, max, average)
   - Customizable metrics

3. **Diagnostic System**
   - Event logging with severity levels (INFO, WARNING, ERROR, CRITICAL)
   - Health checks
   - System integrity validation

4. **Configuration Management**
   - Multiple configuration profiles
   - Parameter storage and retrieval
   - Import/export capabilities

5. **Dashboard**
   - Real-time telemetry display
   - Device status overview
   - Recent diagnostic events
   - I/O stream status

## Usage Examples

### Basic Device Initialization

```cpp
#include "virtual-device.h"
#include "financial-device-driver.h"

using namespace ggnucash::vdev;

int main() {
    // Create virtual device
    auto device = std::make_shared<VirtualPCB>("GGNC-001", "GGNuCash-X1");
    
    // Initialize and start
    device->initialize();
    device->start();
    
    // Create and load driver
    auto driver = std::make_shared<FinancialDeviceDriver>();
    driver->load(device.get());
    driver->initialize();
    
    // Update account balance
    driver->update_account_balance("1101", 50000.0); // Cash: $50,000
    
    // Read balance back
    double balance = driver->read_account_balance("1101");
    
    return 0;
}
```

### Using the Admin Utility

Run the interactive admin utility:

```bash
./build/bin/llama-device-admin
```

Common commands:
```
status          # Show device status
dashboard       # Display telemetry dashboard
telemetry       # Show all telemetry metrics
health          # Run health check
selftest        # Run hardware self-test
pins show       # Show GPIO pin configuration
memory dump     # Dump memory region
driver info     # Show driver information
quit            # Exit utility
```

### Device Registry

Multiple devices can be registered and managed:

```cpp
auto registry = DeviceRegistry::get_instance();

// Register devices
registry->register_device("device1", device1);
registry->register_device("device2", device2);

// List all devices
auto devices = registry->list_devices();

// Get specific device
auto dev = registry->get_device("device1");
```

## Memory-Mapped I/O

The device uses memory-mapped registers for hardware control:

### Hardware Registers

| Address | Name | Description |
|---------|------|-------------|
| 0x40000000 | REG_TRANSACTION_STATUS | Transaction processing status |
| 0x40000004 | REG_BALANCE_TOTAL | Total balance accumulator |
| 0x40000008 | REG_TRANSACTION_COUNT | Number of processed transactions |
| 0x4000000C | REG_ERROR_CODE | Last error code |

### Memory Access

```cpp
// Write to register
device->write_memory(0x40000000, 0x01);

// Read from register
uint8_t value = device->read_memory(0x40000000);

// Access memory region
auto* sram = device->get_memory_region("SRAM");
sram->write_dword(0x100, 0xDEADBEEF);
uint32_t data = sram->read_dword(0x100);
```

## DMA (Direct Memory Access)

High-speed data transfers without CPU intervention:

```cpp
auto* dma = device->get_dma_controller();

// Start DMA transfer
int channel = dma->start_transfer(
    0x20000000,  // Source address (SRAM)
    0x40001000,  // Destination address (Peripheral)
    1024         // Length in bytes
);

// Check completion
while (!dma->is_transfer_complete(channel)) {
    device->update(); // Process DMA transfers
}
```

## Interrupt System

Hardware and software interrupt handling:

```cpp
auto* irq = device->get_interrupt_controller();

// Register interrupt handler
irq->register_interrupt(16, InterruptType::HARDWARE, 5, []() {
    std::cout << "Interrupt triggered!\n";
});

// Enable interrupt
irq->enable_interrupt(16);

// Trigger interrupt
irq->trigger_interrupt(16);

// Process pending interrupts
irq->process_interrupts();
```

## Firmware Controller

Manages device firmware and configuration:

```cpp
FirmwareController firmware(device.get());

// Load firmware
firmware.load_firmware("/path/to/firmware.bin");

// Configure parameters
firmware.configure_parameter("clock_speed", "100MHz");
firmware.configure_parameter("voltage", "3.3V");

// Execute boot sequence
firmware.execute_boot_sequence();
```

## Telemetry and Monitoring

### Adding Custom Metrics

```cpp
TelemetrySystem telemetry;

// Add metrics
telemetry.add_metric("transactions_per_sec", "TPS");
telemetry.add_metric("balance_total", "USD");

// Start collection
telemetry.start_collection();

// Update metrics
telemetry.update_metric("transactions_per_sec", 1500.0);
telemetry.update_metric("balance_total", 1000000.0);

// Generate report
std::cout << telemetry.generate_report();
```

### Diagnostic Events

```cpp
DiagnosticSystem diagnostics;

// Log events
diagnostics.log_event(DiagnosticLevel::INFO, "System", "Device started");
diagnostics.log_event(DiagnosticLevel::WARNING, "Thermal", "Temperature rising");
diagnostics.log_event(DiagnosticLevel::ERROR, "Memory", "SRAM test failed");

// Get recent events
auto events = diagnostics.get_recent_events(10);

// Generate report
std::cout << diagnostics.generate_diagnostic_report();
```

## Building

The virtual device architecture is built as part of the financial-sim example:

```bash
# Configure CMake
cmake -B build

# Build admin utility
cmake --build build --target llama-device-admin

# Build test program
cmake --build build --target test-virtual-device

# Run admin utility
./build/bin/llama-device-admin

# Run tests
./build/bin/test-virtual-device
```

## Testing

### Hardware Self-Test

The driver includes comprehensive self-tests:

```bash
./build/bin/llama-device-admin
> selftest
```

Tests include:
1. Pin accessibility test
2. Memory region test
3. Register access test

### Health Checks

```bash
./build/bin/llama-device-admin
> health
```

Checks include:
- Device state validation
- Voltage rail verification
- Temperature monitoring
- Memory integrity
- I/O health

## Architecture Diagrams

### Device Hierarchy
```
VirtualPCB
├── GPIO Pins (64)
├── Memory Regions
│   ├── SRAM (256KB)
│   ├── FLASH (1MB)
│   └── PERIPHERAL (64KB)
├── DMA Controller (8 channels)
├── Interrupt Controller (256 vectors)
└── I/O Streams
    ├── UART
    ├── SPI
    └── I2C
```

### Data Flow
```
Financial Transaction
        ↓
Financial Device Driver
        ↓
Account-to-Pin Mapping
        ↓
GPIO Pin (Analog Voltage)
        ↓
Hardware Register
        ↓
Virtual PCB
```

## Performance Characteristics

- **GPIO Pin Operations**: Sub-microsecond latency
- **Memory Access**: ~10ns per operation
- **DMA Transfer Rate**: 1GB/s simulated
- **Interrupt Latency**: <1µs
- **Device Update Cycle**: Configurable, typically 1ms

## Security Considerations

1. **Memory Protection**: Configurable read/write permissions per region
2. **Register Access Control**: Protected register ranges
3. **Interrupt Security**: Priority-based interrupt handling
4. **Event Logging**: Comprehensive audit trail

## Future Enhancements

1. **Additional I/O Protocols**: USB, CAN, Ethernet
2. **Advanced Power Management**: Dynamic voltage/frequency scaling
3. **Thermal Modeling**: Realistic temperature simulation
4. **Hardware Acceleration**: GPU-based pin simulation
5. **Network Interface**: Remote device management
6. **Persistent Storage**: Flash memory simulation
7. **Bootloader**: Firmware update mechanism

## Troubleshooting

### Device Won't Initialize
- Check device state is UNINITIALIZED
- Verify memory regions don't overlap
- Ensure sufficient system resources

### Telemetry Not Updating
- Verify telemetry collection is started
- Check metric names match
- Ensure device->update() is called regularly

### DMA Transfers Slow
- Call device->update() frequently
- Check transfer sizes are appropriate
- Verify no mutex contention

## License

MIT License - See LICENSE file for details

## Contributing

Contributions are welcome! Please ensure:
- Code follows existing style
- All tests pass
- Documentation is updated
- Changes are minimal and focused
