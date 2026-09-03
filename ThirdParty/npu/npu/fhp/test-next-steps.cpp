// test-next-steps.cpp - Test suite for NPU "Next Steps" features
//
// This test suite validates the production-ready features:
// - KV-Cache Management
// - GPU Offloading Control
// - Interrupt Support
// - Model Hot-Swapping
// - Batch Inference

#include "llama-coprocessor-driver.h"
#include "virtual-device.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace ggnucash::vdev;

void print_test_header(const std::string& test_name) {
    std::cout << "\n=== Test: " << test_name << " ===\n";
}

void print_test_result(bool passed) {
    if (passed) {
        std::cout << "✓ Test PASSED\n";
    } else {
        std::cout << "✗ Test FAILED\n";
    }
}

bool test_kv_cache_management() {
    print_test_header("KV-Cache Management");
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "NPU-X1");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    // Load model first
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    cfg.n_ctx = 2048;
    llm->set_model_config(cfg);
    llm->load_model();
    
    // Test 1: Enable KV-cache
    std::cout << "  Testing KV-cache enable...\n";
    bool enabled = llm->enable_kv_cache();
    assert(enabled && "Failed to enable KV-cache");
    assert(llm->is_kv_cache_enabled() && "KV-cache not enabled");
    assert(llm->get_kv_cache_size() > 0 && "Invalid KV-cache size");
    std::cout << "    KV-cache size: " << llm->get_kv_cache_size() << " bytes\n";
    std::cout << "    KV-cache addr: 0x" << std::hex << llm->get_kv_cache_addr() << std::dec << "\n";
    
    // Test 2: Check STATUS_KV_CACHED flag
    uint32_t status = llm->read_status();
    assert((status & LlamaCoprocessorDriver::STATUS_KV_CACHED) && "KV_CACHED flag not set");
    std::cout << "    STATUS_KV_CACHED flag set\n";
    
    // Test 3: Disable KV-cache
    std::cout << "  Testing KV-cache disable...\n";
    llm->disable_kv_cache();
    assert(!llm->is_kv_cache_enabled() && "KV-cache still enabled");
    
    status = llm->read_status();
    assert(!(status & LlamaCoprocessorDriver::STATUS_KV_CACHED) && "KV_CACHED flag still set");
    std::cout << "    KV-cache disabled successfully\n";
    
    print_test_result(true);
    return true;
}

bool test_gpu_offloading() {
    print_test_header("GPU Offloading Control");
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "NPU-X1");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    // Test 1: Set GPU layers
    std::cout << "  Setting GPU layers to 32...\n";
    bool success = llm->set_gpu_layers(32);
    assert(success && "Failed to set GPU layers");
    assert(llm->get_gpu_layers() == 32 && "GPU layers not set correctly");
    std::cout << "    GPU layers: " << llm->get_gpu_layers() << "\n";
    
    // Test 2: Check STATUS_GPU_ACTIVE flag
    uint32_t status = llm->read_status();
    assert((status & LlamaCoprocessorDriver::STATUS_GPU_ACTIVE) && "GPU_ACTIVE flag not set");
    assert(llm->is_gpu_active() && "GPU not reported as active");
    std::cout << "    STATUS_GPU_ACTIVE flag set\n";
    
    // Test 3: Disable GPU (set to 0 layers)
    std::cout << "  Disabling GPU...\n";
    llm->set_gpu_layers(0);
    assert(llm->get_gpu_layers() == 0 && "GPU layers not cleared");
    assert(!llm->is_gpu_active() && "GPU still active");
    
    status = llm->read_status();
    assert(!(status & LlamaCoprocessorDriver::STATUS_GPU_ACTIVE) && "GPU_ACTIVE flag still set");
    std::cout << "    GPU disabled successfully\n";
    
    print_test_result(true);
    return true;
}

bool test_interrupt_support() {
    print_test_header("Interrupt Support");
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "NPU-X1");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    // Test 1: Enable interrupts
    std::cout << "  Enabling token interrupts on vector 10...\n";
    bool enabled = llm->enable_token_interrupts(10);
    assert(enabled && "Failed to enable interrupts");
    assert(llm->are_interrupts_enabled() && "Interrupts not enabled");
    std::cout << "    Interrupts enabled\n";
    
    // Test 2: Disable interrupts
    std::cout << "  Disabling interrupts...\n";
    llm->disable_token_interrupts();
    assert(!llm->are_interrupts_enabled() && "Interrupts still enabled");
    std::cout << "    Interrupts disabled\n";
    
    print_test_result(true);
    return true;
}

bool test_model_hot_swapping() {
    print_test_header("Model Hot-Swapping");
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "NPU-X1");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    // Test 1: Load initial model
    std::cout << "  Loading initial model...\n";
    LlamaModelConfig cfg1;
    cfg1.model_path = "model1.gguf";
    cfg1.n_ctx = 2048;
    llm->set_model_config(cfg1);
    llm->load_model();
    assert(llm->is_model_loaded() && "Model not loaded");
    std::cout << "    Model 1 loaded\n";
    
    // Test 2: Unload model
    std::cout << "  Unloading model...\n";
    bool unloaded = llm->unload_model();
    assert(unloaded && "Failed to unload model");
    assert(!llm->is_model_loaded() && "Model still loaded");
    
    uint32_t status = llm->read_status();
    assert(!(status & LlamaCoprocessorDriver::STATUS_MODEL_READY) && "MODEL_READY still set");
    std::cout << "    Model 1 unloaded\n";
    
    // Test 3: Load new model
    std::cout << "  Loading new model...\n";
    LlamaModelConfig cfg2;
    cfg2.model_path = "model2.gguf";
    cfg2.n_ctx = 4096;
    llm->set_model_config(cfg2);
    llm->load_model();
    assert(llm->is_model_loaded() && "New model not loaded");
    std::cout << "    Model 2 loaded\n";
    
    // Test 4: Switch model directly
    std::cout << "  Switching to model3.gguf...\n";
    LlamaModelConfig cfg3;
    cfg3.model_path = "model3.gguf";
    cfg3.n_ctx = 8192;
    bool switched = llm->switch_model(cfg3);
    assert(switched && "Failed to switch model");
    assert(llm->is_model_loaded() && "Model not loaded after switch");
    std::cout << "    Model switched successfully\n";
    
    print_test_result(true);
    return true;
}

bool test_batch_inference() {
    print_test_header("Batch Inference");
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "NPU-X1");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    // Load model
    LlamaModelConfig cfg;
    cfg.model_path = "test-model.gguf";
    cfg.n_ctx = 2048;
    llm->set_model_config(cfg);
    llm->load_model();
    
    // Test 1: Add batch requests
    std::cout << "  Adding batch requests...\n";
    LlamaSequenceConfig seq;
    seq.n_predict = 16;
    
    bool added1 = llm->add_batch_request("Batch prompt 1", seq);
    bool added2 = llm->add_batch_request("Batch prompt 2", seq);
    bool added3 = llm->add_batch_request("Batch prompt 3", seq);
    
    assert(added1 && added2 && added3 && "Failed to add batch requests");
    assert(llm->get_batch_queue_size() == 3 && "Incorrect batch queue size");
    std::cout << "    Added 3 batch requests\n";
    
    // Test 2: Process batch
    std::cout << "  Processing batch...\n";
    bool processed = llm->process_batch();
    assert(processed && "Failed to process batch");
    std::cout << "    Batch processed\n";
    
    // Test 3: Get results
    std::cout << "  Retrieving batch results...\n";
    auto results = llm->get_batch_results();
    assert(results.size() == 3 && "Incorrect number of results");
    std::cout << "    Retrieved " << results.size() << " results\n";
    
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << "    Result " << (i+1) << ": " << results[i].substr(0, 50) << "...\n";
    }
    
    // Test 4: Clear batch
    std::cout << "  Clearing batch...\n";
    llm->clear_batch();
    assert(llm->get_batch_queue_size() == 0 && "Batch not cleared");
    std::cout << "    Batch cleared\n";
    
    print_test_result(true);
    return true;
}

bool test_combined_features() {
    print_test_header("Combined Features Integration");
    
    auto device = std::make_shared<VirtualPCB>("NPU-TEST", "NPU-X1");
    device->initialize();
    device->start();
    
    auto llm = std::make_shared<LlamaCoprocessorDriver>();
    llm->load(device.get());
    llm->initialize();
    
    // Configure and load model with all features
    std::cout << "  Configuring model with GPU + KV-cache + interrupts...\n";
    LlamaModelConfig cfg;
    cfg.model_path = "combined-test.gguf";
    cfg.n_ctx = 4096;
    cfg.n_gpu_layers = 16;
    llm->set_model_config(cfg);
    llm->load_model();
    
    // Enable all features
    llm->set_gpu_layers(16);
    llm->enable_kv_cache();
    llm->enable_token_interrupts(5);
    
    // Verify all features are active
    assert(llm->is_gpu_active() && "GPU not active");
    assert(llm->is_kv_cache_enabled() && "KV-cache not enabled");
    assert(llm->are_interrupts_enabled() && "Interrupts not enabled");
    std::cout << "    All features enabled\n";
    
    // Run inference with all features
    std::cout << "  Running inference with all features...\n";
    LlamaSequenceConfig seq;
    seq.n_predict = 32;
    std::string result = llm->infer("Test with all features", seq);
    assert(!result.empty() && "Inference failed");
    std::cout << "    Inference successful\n";
    
    // Verify status
    uint32_t status = llm->read_status();
    assert((status & LlamaCoprocessorDriver::STATUS_MODEL_READY) && "Model not ready");
    assert((status & LlamaCoprocessorDriver::STATUS_GPU_ACTIVE) && "GPU not active");
    assert((status & LlamaCoprocessorDriver::STATUS_KV_CACHED) && "KV-cache not active");
    std::cout << "    All status flags correct\n";
    
    print_test_result(true);
    return true;
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║       NPU Next Steps Feature Test Suite              ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n";
    
    int passed = 0;
    int total = 0;
    
    total++; if (test_kv_cache_management()) passed++;
    total++; if (test_gpu_offloading()) passed++;
    total++; if (test_interrupt_support()) passed++;
    total++; if (test_model_hot_swapping()) passed++;
    total++; if (test_batch_inference()) passed++;
    total++; if (test_combined_features()) passed++;
    
    std::cout << "\n╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test Results: " << passed << "/" << total << " passed";
    if (passed == total) {
        std::cout << "                              ║\n";
        std::cout << "║  ✓ All Next Steps Features Working!                  ║\n";
    } else {
        std::cout << "                                ║\n";
        std::cout << "║  ✗ Some tests failed                                  ║\n";
    }
    std::cout << "╚═══════════════════════════════════════════════════════╝\n";
    
    return (passed == total) ? 0 : 1;
}
