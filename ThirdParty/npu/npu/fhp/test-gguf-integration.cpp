// ============================================================================
// Test GGUF Integration
// ============================================================================
// This test validates the GGUF runtime integration with the LLM coprocessor
// driver, including tokenization, inference, and detokenization.
// ============================================================================

#include "virtual-device.h"
#include "llama-coprocessor-driver.h"
#include <iostream>
#include <cassert>
#include <chrono>

using namespace ggnucash::vdev;

void test_model_loading() {
    std::cout << "\n=== Test 1: Model Loading ===\n";
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "Test-Device");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    assert(llm->load(device.get()));
    assert(llm->initialize());
    
    // Configure model
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    cfg.n_ctx = 2048;
    cfg.n_threads = 2;
    llm->set_model_config(cfg);
    
    // Load model
    assert(llm->load_model());
    assert(llm->is_model_loaded());
    
    std::cout << "✓ Model loaded successfully\n";
    std::cout << "✓ Status: " << llm->get_device_status_string() << "\n";
}

void test_basic_inference() {
    std::cout << "\n=== Test 2: Basic Inference ===\n";
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "Test-Device");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    cfg.n_ctx = 2048;
    llm->set_model_config(cfg);
    llm->load_model();
    
    // Run inference
    LlamaSequenceConfig seq;
    seq.n_predict = 32;
    seq.echo_prompt = true;
    
    std::string result = llm->infer("Test prompt for GGUF integration", seq);
    
    assert(!result.empty());
    std::cout << "✓ Inference completed\n";
    std::cout << "Result:\n" << result << "\n";
    
    // Check telemetry
    auto telem = llm->get_telemetry();
    assert(telem.total_prompts == 1);
    assert(telem.total_tokens_generated > 0);
    
    std::cout << "✓ Telemetry: " << telem.total_tokens_generated 
              << " tokens generated\n";
}

void test_streaming_inference() {
    std::cout << "\n=== Test 3: Streaming Inference ===\n";
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "Test-Device");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    llm->set_model_config(cfg);
    llm->load_model();
    
    // Run streaming inference
    LlamaSequenceConfig seq;
    seq.n_predict = 16;
    seq.stream_tokens = true;
    
    int token_count = 0;
    bool success = llm->infer_streaming("Streaming test", seq,
        [&token_count](const std::string& text, int32_t token_id, bool is_last) {
            token_count++;
            std::cout << "[Token " << token_count << "] " << text;
            if (is_last) {
                std::cout << " (EOS)";
            }
            std::cout << "\n";
        }
    );
    
    assert(success);
    assert(token_count > 0);
    std::cout << "✓ Streaming completed with " << token_count << " tokens\n";
}

void test_multiple_inferences() {
    std::cout << "\n=== Test 4: Multiple Sequential Inferences ===\n";
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "Test-Device");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    llm->set_model_config(cfg);
    llm->load_model();
    
    LlamaSequenceConfig seq;
    seq.n_predict = 16;
    
    // Run multiple inferences
    for (int i = 0; i < 3; i++) {
        std::string prompt = "Prompt number " + std::to_string(i + 1);
        std::string result = llm->infer(prompt, seq);
        assert(!result.empty());
        std::cout << "✓ Inference " << (i + 1) << " completed\n";
    }
    
    auto telem = llm->get_telemetry();
    assert(telem.total_prompts == 3);
    std::cout << "✓ All inferences completed successfully\n";
    std::cout << "  Total prompts: " << telem.total_prompts << "\n";
    std::cout << "  Total tokens: " << telem.total_tokens_generated << "\n";
}

void test_hardware_registers() {
    std::cout << "\n=== Test 5: Hardware Register Access ===\n";
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "Test-Device");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    llm->set_model_config(cfg);
    llm->load_model();
    
    // Check status register
    uint32_t status = llm->read_status();
    std::cout << "Status register: 0x" << std::hex << status << std::dec << "\n";
    assert(status & LlamaCoprocessorDriver::STATUS_MODEL_READY);
    assert(!(status & LlamaCoprocessorDriver::STATUS_ERROR));
    std::cout << "✓ Model ready flag set\n";
    
    // Run inference and check busy flag
    LlamaSequenceConfig seq;
    seq.n_predict = 8;
    llm->infer("Test", seq);
    
    // After inference, should be EOG
    status = llm->read_status();
    assert(status & LlamaCoprocessorDriver::STATUS_EOG);
    assert(!(status & LlamaCoprocessorDriver::STATUS_BUSY));
    std::cout << "✓ EOG flag set after inference\n";
    
    // Check error code
    uint32_t error = llm->get_error_code();
    assert(error == 0);
    std::cout << "✓ No errors reported\n";
}

void test_self_test() {
    std::cout << "\n=== Test 6: Hardware Self-Test ===\n";
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "Test-Device");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    llm->set_model_config(cfg);
    llm->load_model();
    
    // Run self-test
    bool result = llm->run_self_test();
    assert(result);
    std::cout << "✓ Self-test passed\n";
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║       GGUF Integration Test Suite                    ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n";
    
    try {
        test_model_loading();
        test_basic_inference();
        test_streaming_inference();
        test_multiple_inferences();
        test_hardware_registers();
        test_self_test();
        
        std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✓ All GGUF Integration Tests Passed                 ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════╝\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test failed with unknown exception\n";
        return 1;
    }
}
