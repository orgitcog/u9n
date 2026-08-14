#include "virtual-device.h"
#include "financial-device-driver.h"
#include "llama-coprocessor-driver.h"
#include <iostream>

using namespace ggnucash::vdev;

int main() {
    std::cout << "=== Virtual Device Test ===\n\n";
    
    // Create virtual device
    auto device = std::make_shared<VirtualPCB>("TEST-001", "TestBoard");
    
    if (!device->initialize()) {
        std::cerr << "Failed to initialize device\n";
        return 1;
    }
    
    std::cout << "Device initialized\n";
    
    // Test financial driver
    auto fin_driver = std::make_shared<FinancialDeviceDriver>();
    if (!fin_driver->load(device.get()) || !fin_driver->initialize()) {
        std::cerr << "Failed to initialize financial driver\n";
        return 1;
    }
    
    std::cout << "Financial driver loaded\n";
    
    // Test LLM driver
    auto llm_driver = std::make_shared<LlamaCoprocessorDriver>();
    if (!llm_driver->load(device.get()) || !llm_driver->initialize()) {
        std::cerr << "Failed to initialize LLM driver\n";
        return 1;
    }
    
    std::cout << "LLM driver loaded\n";
    
    // Configure and load model (stub)
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    cfg.n_ctx = 2048;
    llm_driver->set_model_config(cfg);
    
    if (!llm_driver->load_model()) {
        std::cerr << "Failed to load model\n";
        return 1;
    }
    
    std::cout << "\n" << llm_driver->get_device_status_string() << "\n\n";
    
    // Run self-test
    std::cout << "Running self-test...\n";
    if (!llm_driver->run_self_test()) {
        std::cerr << "Self-test failed\n";
        return 1;
    }
    
    // Run test inference
    std::cout << "\nRunning test inference...\n";
    LlamaSequenceConfig seq;
    seq.n_predict = 16;
    seq.echo_prompt = true;
    
    std::string result = llm_driver->infer("Test prompt for NPU", seq);
    std::cout << "\nInference result:\n" << result << "\n";
    
    // Get telemetry
    auto telemetry = llm_driver->get_telemetry();
    std::cout << "\nTelemetry:\n";
    std::cout << "  Total prompts: " << telemetry.total_prompts << "\n";
    std::cout << "  Total tokens: " << telemetry.total_tokens_generated << "\n";
    std::cout << "  Tokens/sec: " << telemetry.tokens_per_second << "\n";
    
    // Cleanup
    llm_driver->remove();
    fin_driver->remove();
    device->shutdown();
    
    std::cout << "\n✓ All tests passed\n";
    
    return 0;
}
