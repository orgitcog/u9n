#pragma once

// ============================================================================
// Mock llama.cpp API - Stub Implementation for Testing
// ============================================================================
// This file provides a minimal mock of the llama.cpp API to demonstrate
// GGUF integration patterns without requiring the full llama.cpp dependency.
// 
// When llama.cpp is available, replace this with:
//   #include "llama.h"
//   #include "common.h"
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <map>
#include <sstream>

namespace llama_mock {

// Forward declarations - mirror llama.cpp types
struct llama_model;
struct llama_context;

// Model parameters
struct llama_model_params {
    int32_t n_gpu_layers = 0;
    bool use_mmap = true;
    bool use_mlock = false;
    bool vocab_only = false;
};

// Context parameters
struct llama_context_params {
    uint32_t n_ctx = 512;
    uint32_t n_batch = 512;
    uint32_t n_threads = 4;
    bool offload_kqv = false;
    bool flash_attn = false;
};

// Token type
using llama_token = int32_t;

// Special token IDs
constexpr llama_token LLAMA_TOKEN_BOS = 1;
constexpr llama_token LLAMA_TOKEN_EOS = 2;
constexpr llama_token LLAMA_TOKEN_UNK = 0;

// ============================================================================
// Mock Implementation - Simple String-Based Tokenizer
// ============================================================================

class MockLlamaBackend {
private:
    std::string model_path_;
    llama_model_params model_params_;
    llama_context_params ctx_params_;
    bool initialized_ = false;
    
    // Simple word-based tokenizer for demo
    std::vector<std::string> vocab_;
    std::map<std::string, llama_token> word_to_token_;
    std::map<llama_token, std::string> token_to_word_;
    llama_token next_token_id_ = 3;

public:
    MockLlamaBackend() {
        // Initialize with some common tokens
        add_token("<s>", LLAMA_TOKEN_BOS);
        add_token("</s>", LLAMA_TOKEN_EOS);
        add_token("<unk>", LLAMA_TOKEN_UNK);
    }

    bool load_model(const std::string& path, const llama_model_params& params) {
        model_path_ = path;
        model_params_ = params;
        initialized_ = true;
        return true;
    }

    bool create_context(const llama_context_params& params) {
        ctx_params_ = params;
        return initialized_;
    }

    // Tokenization - simple whitespace splitting
    std::vector<llama_token> tokenize(const std::string& text, bool add_bos) {
        std::vector<llama_token> tokens;
        
        if (add_bos) {
            tokens.push_back(LLAMA_TOKEN_BOS);
        }

        std::istringstream iss(text);
        std::string word;
        while (iss >> word) {
            if (word_to_token_.find(word) == word_to_token_.end()) {
                add_token(word, next_token_id_++);
            }
            tokens.push_back(word_to_token_[word]);
        }

        return tokens;
    }

    // Detokenization
    std::string detokenize(const std::vector<llama_token>& tokens) {
        std::ostringstream oss;
        bool first = true;
        
        for (auto token : tokens) {
            if (token == LLAMA_TOKEN_BOS || token == LLAMA_TOKEN_EOS) {
                continue;
            }
            
            if (!first) {
                oss << " ";
            }
            first = false;

            if (token_to_word_.find(token) != token_to_word_.end()) {
                oss << token_to_word_[token];
            } else {
                oss << "<unk>";
            }
        }

        return oss.str();
    }

    // Mock inference - generates completion based on prompt
    std::vector<llama_token> generate(const std::vector<llama_token>& prompt_tokens, 
                                     int32_t n_predict) {
        std::vector<llama_token> result;
        
        // Mock generation: simple echo + completion pattern
        std::vector<std::string> completion_words = {
            "This", "is", "a", "mock", "GGUF", "completion",
            "demonstrating", "hardware-style", "LLM", "integration",
            "with", "memory-mapped", "registers", "and", "telemetry"
        };

        for (int32_t i = 0; i < std::min(n_predict, (int32_t)completion_words.size()); ++i) {
            const auto& word = completion_words[i];
            if (word_to_token_.find(word) == word_to_token_.end()) {
                add_token(word, next_token_id_++);
            }
            result.push_back(word_to_token_[word]);
        }

        result.push_back(LLAMA_TOKEN_EOS);
        return result;
    }

    int32_t get_n_ctx() const {
        return ctx_params_.n_ctx;
    }

    bool is_initialized() const {
        return initialized_;
    }

    void free() {
        initialized_ = false;
        model_path_.clear();
    }

private:
    void add_token(const std::string& word, llama_token token) {
        word_to_token_[word] = token;
        token_to_word_[token] = word;
        vocab_.push_back(word);
    }
};

// ============================================================================
// C-style API wrappers (mirror llama.cpp interface)
// ============================================================================

inline llama_model_params llama_model_default_params() {
    return llama_model_params{};
}

inline llama_context_params llama_context_default_params() {
    return llama_context_params{};
}

// Global backend instance (in real llama.cpp, these would be proper objects)
inline MockLlamaBackend& get_backend() {
    static MockLlamaBackend backend;
    return backend;
}

// Special constants for mock implementation (use nullptr + offset for sentinel values)
#define MOCK_MODEL_PTR reinterpret_cast<llama_model*>(0x1000)
#define MOCK_CTX_PTR reinterpret_cast<llama_context*>(0x2000)

inline llama_model* llama_load_model_from_file(const char* path_model, llama_model_params params) {
    if (get_backend().load_model(path_model, params)) {
        return MOCK_MODEL_PTR;
    }
    return nullptr;
}

inline llama_context* llama_new_context_with_model(llama_model* model, llama_context_params params) {
    if (model && get_backend().create_context(params)) {
        return MOCK_CTX_PTR;
    }
    return nullptr;
}

inline void llama_free_model(llama_model* model) {
    (void)model;
    get_backend().free();
}

inline void llama_free(llama_context* ctx) {
    (void)ctx;
}

inline int32_t llama_n_ctx(const llama_context* ctx) {
    (void)ctx;
    return get_backend().get_n_ctx();
}

inline std::vector<llama_token> llama_tokenize(const llama_context* ctx, const std::string& text, bool add_bos) {
    (void)ctx;
    return get_backend().tokenize(text, add_bos);
}

inline std::string llama_detokenize(const llama_context* ctx, const std::vector<llama_token>& tokens) {
    (void)ctx;
    return get_backend().detokenize(tokens);
}

inline llama_token llama_token_bos(const llama_model* model) {
    (void)model;
    return LLAMA_TOKEN_BOS;
}

inline llama_token llama_token_eos(const llama_model* model) {
    (void)model;
    return LLAMA_TOKEN_EOS;
}

} // namespace llama_mock

// Namespace alias for easy switching to real llama.cpp
namespace llama = llama_mock;
