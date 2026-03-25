#include "virtual-device.h"
#include "financial-device-driver.h"
#include "llama-coprocessor-driver.h"
#include <iostream>

using namespace ggnucash::vdev;

int main() {
    std::cout << "=== Virtual Device Demo ===\n\n";
    
    // Create and initialize device
    auto device = std::make_shared<VirtualPCB>("DEMO-001", "DemoBoard");
    device->initialize();
    device->start();
    
    std::cout << device->get_status_report() << "\n";
    
    std::cout << "\nDemo complete.\n";
    
    return 0;
}
