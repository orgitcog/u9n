#pragma once
// Vendored from ReZorg/elizaos-cpp llama_cpp.hpp (namespace elizaos->dte) — Gap A/C: LLM inference bridge for Deep Tree Echo.

/**
 * @file llama_cpp.hpp
 * @brief llama.cpp Integration for ElizaOS
 * 
 * This module provides comprehensive integration with llama.cpp for local LLM inference.
 * It supports model loading, tokenization, context management, and text generation
 * with advanced features like batched inference, grammar-constrained generation,
 * and model quantization support.
 * 
 * Task 10.1.1: llama.cpp integration - Phase 1 Implementation
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <optional>
#include <chrono>
#include <future>
#include <queue>
#include <thread>
#include <condition_variable>

namespace dte {
namespace llama {

// Forward declarations
class LlamaModel;
class LlamaContext;
class LlamaSession;
class LlamaBatch;
class LlamaTokenizer;
class LlamaGrammar;
class LlamaModelManager;

/**
 * Token type for llama.cpp
 */
using LlamaToken = int32_t;
using TokenSequence = std::vector<LlamaToken>;

/**
 * Model quantization types
 */
enum class QuantizationType {
    F32,        // Full precision float32
    F16,        // Half precision float16
    Q8_0,       // 8-bit quantization
    Q6_K,       // 6-bit k-quant
    Q5_K_M,     // 5-bit k-quant medium
    Q5_K_S,     // 5-bit k-quant small
    Q4_K_M,     // 4-bit k-quant medium
    Q4_K_S,     // 4-bit k-quant small
    Q4_0,       // 4-bit legacy
    Q4_1,       // 4-bit legacy variant
    Q3_K_M,     // 3-bit k-quant medium
    Q3_K_S,     // 3-bit k-quant small
    Q2_K,       // 2-bit k-quant
    IQ4_NL,     // imatrix 4-bit
    IQ3_XXS,    // imatrix 3-bit extra small
    IQ2_XXS,    // imatrix 2-bit extra small
    UNKNOWN
};

/**
 * Model architecture types
 */
enum class ModelArchitecture {
    LLAMA,
    LLAMA2,
    LLAMA3,
    MISTRAL,
    MIXTRAL,
    FALCON,
    GPT_NEOX,
    GPT2,
    STARCODER,
    PHI,
    QWEN,
    QWEN2,
    GEMMA,
    COMMAND_R,
    DEEPSEEK,
    UNKNOWN
};

/**
 * Sampling method types
 */
enum class SamplingMethod {
    GREEDY,         // Always pick highest probability token
    TOP_K,          // Sample from top-k tokens
    TOP_P,          // Nucleus sampling
    MIN_P,          // Minimum probability sampling
    TYPICAL_P,      // Typical probability sampling
    MIROSTAT,       // Mirostat v1 sampling
    MIROSTAT_V2,    // Mirostat v2 sampling
    TEMPERATURE     // Temperature-based sampling
};

/**
 * Generation stop reason
 */
enum class StopReason {
    MAX_TOKENS,     // Reached max token limit
    EOS_TOKEN,      // End-of-sequence token generated
    STOP_STRING,    // Stop string encountered
    GRAMMAR_COMPLETE,// Grammar rule completed
    USER_CANCELLED, // User cancelled generation
    ERROR           // Error occurred
};

/**
 * Model loading parameters
 */
struct ModelLoadParams {
    std::string modelPath;                  // Path to GGUF model file
    int32_t nGpuLayers = -1;               // Number of layers to offload to GPU (-1 = auto)
    bool useMemoryMap = true;               // Use memory-mapped I/O
    bool useLocking = false;                // Lock model in memory
    int32_t mainGpu = 0;                    // Main GPU device index
    std::vector<float> tensorSplit;         // GPU tensor split ratios
    bool vocabOnly = false;                 // Load vocabulary only
    QuantizationType quantization = QuantizationType::UNKNOWN; // Expected quantization
    size_t maxMemoryMB = 0;                 // Maximum memory usage (0 = unlimited)
    std::string loraPath;                   // Path to LoRA adapter
    float loraScale = 1.0f;                 // LoRA scaling factor
    std::vector<std::string> loraAdapters;  // Multiple LoRA adapters
    std::vector<float> loraScales;          // Corresponding scales for each LoRA
    bool flashAttention = true;             // Use flash attention if available
    std::string cacheTypeK = "f16";         // KV cache type for keys
    std::string cacheTypeV = "f16";         // KV cache type for values
};

/**
 * Context creation parameters
 */
struct ContextParams {
    uint32_t nCtx = 4096;                  // Context size (max tokens)
    uint32_t nBatch = 512;                 // Batch size for prompt processing
    uint32_t nUBatch = 512;                // Physical batch size for generation
    uint32_t nThreads = 0;                 // Number of threads (0 = auto)
    uint32_t nThreadsBatch = 0;            // Threads for batch processing (0 = auto)
    bool embeddings = false;               // Enable embedding mode
    bool flashAttention = true;            // Use flash attention
    bool offloadKqv = true;                // Offload K,Q,V matrices to GPU
    uint32_t nSequencesMax = 1;            // Max sequences for parallel decoding
    bool logitsAll = false;                // Compute logits for all tokens
    bool useRope = true;                   // Use RoPE positional encoding
    float ropeFreqBase = 0.0f;             // RoPE frequency base (0 = auto)
    float ropeFreqScale = 0.0f;            // RoPE frequency scale (0 = auto)
    float yarnExtFactor = -1.0f;           // YaRN extrapolation factor
    float yarnAttnFactor = 1.0f;           // YaRN attention factor
    float yarnBetaFast = 32.0f;            // YaRN beta fast
    float yarnBetaSlow = 1.0f;             // YaRN beta slow
    uint32_t yarnOrigCtx = 0;              // YaRN original context size
    bool poolingLast = false;              // Use last token for pooling
    bool poolingMean = false;              // Use mean pooling for embeddings
};

/**
 * Sampling parameters for text generation
 */
struct SamplingParams {
    // Basic parameters
    float temperature = 0.8f;              // Temperature for sampling
    int32_t topK = 40;                     // Top-k sampling
    float topP = 0.95f;                    // Top-p (nucleus) sampling
    float minP = 0.05f;                    // Minimum probability threshold
    float typicalP = 1.0f;                 // Typical probability (1.0 = disabled)
    float repeatPenalty = 1.1f;            // Repetition penalty
    float frequencyPenalty = 0.0f;         // Frequency penalty
    float presencePenalty = 0.0f;          // Presence penalty
    
    // Advanced sampling
    int32_t repeatLastN = 64;              // Tokens to consider for repetition
    int32_t mirostatMode = 0;              // Mirostat mode (0 = disabled)
    float mirostatTau = 5.0f;              // Mirostat target entropy
    float mirostatEta = 0.1f;              // Mirostat learning rate
    
    // Token penalties
    std::vector<LlamaToken> penaltyTokens; // Tokens to penalize
    std::unordered_map<LlamaToken, float> logitBias; // Token-specific logit bias
    
    // Generation limits
    int32_t maxTokens = 256;               // Maximum tokens to generate
    std::vector<std::string> stopStrings; // Stop generation on these strings
    std::vector<LlamaToken> stopTokens;   // Stop generation on these tokens
    bool ignoreEos = false;               // Don't stop on EOS token
    
    // Sampling method
    SamplingMethod method = SamplingMethod::TOP_P;
    
    // Grammar constraints
    std::string grammarStr;               // BNF grammar string
    std::string jsonSchema;               // JSON schema for structured output
    
    // Random seed
    uint32_t seed = 0;                    // Random seed (0 = random)
};

/**
 * Generation result containing output tokens and metadata
 */
struct GenerationResult {
    std::string text;                      // Generated text
    TokenSequence tokens;                  // Generated tokens
    StopReason stopReason = StopReason::MAX_TOKENS;
    std::string stopString;                // Stop string that triggered stop (if any)
    
    // Performance metrics
    size_t promptTokens = 0;              // Number of prompt tokens processed
    size_t completionTokens = 0;          // Number of completion tokens generated
    std::chrono::milliseconds promptTime{0};  // Time to process prompt
    std::chrono::milliseconds completionTime{0}; // Time for completion
    double tokensPerSecond = 0.0;         // Completion tokens per second
    
    // Logits and probabilities (optional)
    std::vector<float> lastLogits;        // Logits of last generation step
    std::vector<std::pair<LlamaToken, float>> topKTokens; // Top-k alternatives
    
    bool success = true;
    std::string errorMessage;
};

/**
 * Embedding result from model
 */
struct EmbeddingResult {
    std::vector<float> embedding;          // Embedding vector
    size_t dimensions = 0;                 // Embedding dimensions
    std::string text;                      // Input text
    size_t tokenCount = 0;                 // Number of tokens
    std::chrono::milliseconds computeTime{0};
    bool success = true;
    std::string errorMessage;
};

/**
 * Model information and statistics
 */
struct ModelInfo {
    std::string name;                      // Model name
    std::string path;                      // Model file path
    ModelArchitecture architecture = ModelArchitecture::UNKNOWN;
    QuantizationType quantization = QuantizationType::UNKNOWN;
    
    // Model dimensions
    uint32_t nVocab = 0;                  // Vocabulary size
    uint32_t nCtxTrain = 0;               // Training context size
    uint32_t nEmbd = 0;                   // Embedding dimensions
    uint32_t nHead = 0;                   // Number of attention heads
    uint32_t nHeadKV = 0;                 // Number of KV heads
    uint32_t nLayer = 0;                  // Number of layers
    uint32_t nFF = 0;                     // Feed-forward size
    
    // Memory usage
    size_t sizeModel = 0;                 // Model size in bytes
    size_t sizeContext = 0;               // Per-context size
    size_t sizeKvCache = 0;               // KV cache size per token
    
    // Special tokens
    LlamaToken bosToken = -1;             // Beginning of sequence
    LlamaToken eosToken = -1;             // End of sequence
    LlamaToken nlToken = -1;              // Newline token
    LlamaToken padToken = -1;             // Padding token
    
    // Chat template
    std::string chatTemplate;             // Jinja2 chat template
    bool hasChatTemplate = false;
};

/**
 * Token callback for streaming generation
 */
using TokenCallback = std::function<bool(LlamaToken token, const std::string& piece)>;

/**
 * Progress callback for long operations
 */
using ProgressCallback = std::function<void(float progress, const std::string& status)>;

/**
 * Llama tokenizer for text-to-token conversion
 */
class LlamaTokenizer {
public:
    LlamaTokenizer(std::shared_ptr<LlamaModel> model);
    ~LlamaTokenizer();
    
    /**
     * Tokenize text into token IDs
     * @param text Input text
     * @param addBos Add beginning-of-sequence token
     * @param addEos Add end-of-sequence token
     * @return Token sequence
     */
    TokenSequence tokenize(const std::string& text, bool addBos = true, bool addEos = false) const;
    
    /**
     * Detokenize token IDs back to text
     * @param tokens Token sequence
     * @return Decoded text
     */
    std::string detokenize(const TokenSequence& tokens) const;
    
    /**
     * Get text piece for a single token
     * @param token Token ID
     * @return Text piece
     */
    std::string tokenToPiece(LlamaToken token) const;
    
    /**
     * Get vocabulary size
     */
    size_t vocabSize() const;
    
    /**
     * Get special tokens
     */
    LlamaToken getBosToken() const;
    LlamaToken getEosToken() const;
    LlamaToken getNlToken() const;
    LlamaToken getPadToken() const;
    
    /**
     * Check if token is special
     */
    bool isSpecialToken(LlamaToken token) const;
    
    /**
     * Get token text for display (handles special tokens)
     */
    std::string tokenToString(LlamaToken token, bool showSpecial = false) const;
    
    /**
     * Apply chat template
     * @param messages Vector of {role, content} pairs
     * @param addGenerationPrompt Add assistant prompt
     * @return Formatted prompt string
     */
    std::string applyChatTemplate(
        const std::vector<std::pair<std::string, std::string>>& messages,
        bool addGenerationPrompt = true
    ) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<LlamaModel> model_;
};

/**
 * Grammar constraint for structured generation
 */
class LlamaGrammar {
public:
    LlamaGrammar();
    ~LlamaGrammar();
    
    /**
     * Create grammar from BNF string
     * @param bnf BNF grammar definition
     * @return true if grammar is valid
     */
    bool loadFromBNF(const std::string& bnf);
    
    /**
     * Create grammar from JSON schema
     * @param schema JSON schema string
     * @return true if grammar is valid
     */
    bool loadFromJsonSchema(const std::string& schema);
    
    /**
     * Check if token is allowed by grammar
     * @param token Token to check
     * @return true if allowed
     */
    bool isTokenAllowed(LlamaToken token) const;
    
    /**
     * Accept token and advance grammar state
     * @param token Token to accept
     */
    void acceptToken(LlamaToken token);
    
    /**
     * Reset grammar to initial state
     */
    void reset();
    
    /**
     * Check if grammar is valid
     */
    bool isValid() const;
    
    /**
     * Get grammar string
     */
    const std::string& getGrammarString() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string grammarStr_;
    bool valid_ = false;
};

/**
 * Batch container for parallel inference
 */
class LlamaBatch {
public:
    LlamaBatch(size_t maxTokens, int32_t maxSeq = 1);
    ~LlamaBatch();
    
    /**
     * Add tokens to the batch
     * @param tokens Token sequence to add
     * @param seqId Sequence ID for this batch
     * @param startPos Starting position in the sequence
     * @return true if tokens were added
     */
    bool addTokens(const TokenSequence& tokens, int32_t seqId = 0, int32_t startPos = 0);
    
    /**
     * Add single token to the batch
     */
    bool addToken(LlamaToken token, int32_t seqId = 0, int32_t pos = 0, bool computeLogits = true);
    
    /**
     * Clear the batch
     */
    void clear();
    
    /**
     * Get number of tokens in batch
     */
    size_t size() const;
    
    /**
     * Check if batch is empty
     */
    bool empty() const;
    
    /**
     * Check if batch is full
     */
    bool isFull() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    size_t maxTokens_;
    int32_t maxSeq_;
    friend class LlamaContext;
};

/**
 * Llama model wrapper for loading and managing GGUF models
 */
class LlamaModel {
public:
    LlamaModel();
    ~LlamaModel();
    
    /**
     * Load model from file
     * @param params Model loading parameters
     * @param progress Progress callback
     * @return true if model loaded successfully
     */
    bool load(const ModelLoadParams& params, ProgressCallback progress = nullptr);
    
    /**
     * Unload the model
     */
    void unload();
    
    /**
     * Check if model is loaded
     */
    bool isLoaded() const;
    
    /**
     * Get model information
     */
    ModelInfo getInfo() const;
    
    /**
     * Get model path
     */
    const std::string& getPath() const;
    
    /**
     * Create context for this model
     * @param params Context parameters
     * @return Shared pointer to context
     */
    std::shared_ptr<LlamaContext> createContext(const ContextParams& params = ContextParams());
    
    /**
     * Get tokenizer for this model
     * @return Shared pointer to tokenizer
     */
    std::shared_ptr<LlamaTokenizer> getTokenizer();
    
    /**
     * Apply LoRA adapter
     * @param loraPath Path to LoRA file
     * @param scale Scaling factor
     * @return true if successful
     */
    bool applyLoRA(const std::string& loraPath, float scale = 1.0f);
    
    /**
     * Remove LoRA adapter
     */
    void removeLoRA();
    
    /**
     * Get native llama_model handle (for advanced use)
     */
    void* getNativeHandle() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string path_;
    bool loaded_ = false;
    mutable std::mutex mutex_;
};

/**
 * Llama context for inference operations
 */
class LlamaContext {
public:
    LlamaContext(std::shared_ptr<LlamaModel> model, const ContextParams& params);
    ~LlamaContext();
    
    /**
     * Process tokens (prompt ingestion)
     * @param tokens Tokens to process
     * @param computeLogits Compute logits for last token
     * @return true if successful
     */
    bool process(const TokenSequence& tokens, bool computeLogits = true);
    
    /**
     * Process batch
     * @param batch Batch to process
     * @return true if successful
     */
    bool process(LlamaBatch& batch);
    
    /**
     * Generate text from prompt
     * @param prompt Input prompt text
     * @param params Sampling parameters
     * @param callback Token callback for streaming
     * @return Generation result
     */
    GenerationResult generate(
        const std::string& prompt,
        const SamplingParams& params = SamplingParams(),
        TokenCallback callback = nullptr
    );
    
    /**
     * Continue generation from current state
     * @param params Sampling parameters
     * @param callback Token callback for streaming
     * @return Generation result
     */
    GenerationResult continueGeneration(
        const SamplingParams& params = SamplingParams(),
        TokenCallback callback = nullptr
    );
    
    /**
     * Generate embeddings for text
     * @param text Input text
     * @param normalize Normalize embedding vector
     * @return Embedding result
     */
    EmbeddingResult getEmbedding(const std::string& text, bool normalize = true);
    
    /**
     * Sample next token based on current state
     * @param params Sampling parameters
     * @return Sampled token
     */
    LlamaToken sampleToken(const SamplingParams& params);
    
    /**
     * Get logits for vocabulary
     * @param tokenIndex Token index in batch (default: last)
     * @return Logits vector
     */
    std::vector<float> getLogits(int32_t tokenIndex = -1) const;
    
    /**
     * Get top-k tokens with probabilities
     * @param k Number of tokens
     * @return Vector of (token, probability) pairs
     */
    std::vector<std::pair<LlamaToken, float>> getTopKTokens(int32_t k = 10) const;
    
    /**
     * Reset context state
     */
    void reset();
    
    /**
     * Get number of tokens in context
     */
    size_t getTokenCount() const;
    
    /**
     * Get remaining context capacity
     */
    size_t getRemainingCapacity() const;
    
    /**
     * Get context size
     */
    size_t getContextSize() const;
    
    /**
     * Save context state to file
     * @param path File path
     * @return true if successful
     */
    bool saveState(const std::string& path) const;
    
    /**
     * Load context state from file
     * @param path File path
     * @return true if successful
     */
    bool loadState(const std::string& path);
    
    /**
     * Set grammar for constrained generation
     * @param grammar Grammar object
     */
    void setGrammar(std::shared_ptr<LlamaGrammar> grammar);
    
    /**
     * Clear grammar
     */
    void clearGrammar();
    
    /**
     * Get native llama_context handle (for advanced use)
     */
    void* getNativeHandle() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<LlamaModel> model_;
    ContextParams params_;
    size_t tokenCount_ = 0;
    mutable std::mutex mutex_;
};

/**
 * Session manager for multi-turn conversations
 */
class LlamaSession {
public:
    LlamaSession(std::shared_ptr<LlamaContext> context);
    ~LlamaSession();
    
    /**
     * Add message to conversation
     * @param role Message role (system, user, assistant)
     * @param content Message content
     */
    void addMessage(const std::string& role, const std::string& content);
    
    /**
     * Generate response to current conversation
     * @param params Sampling parameters
     * @param callback Token callback for streaming
     * @return Generation result
     */
    GenerationResult generateResponse(
        const SamplingParams& params = SamplingParams(),
        TokenCallback callback = nullptr
    );
    
    /**
     * Get conversation history
     * @return Vector of (role, content) pairs
     */
    std::vector<std::pair<std::string, std::string>> getHistory() const;
    
    /**
     * Clear conversation history
     */
    void clearHistory();
    
    /**
     * Set system prompt
     * @param prompt System prompt text
     */
    void setSystemPrompt(const std::string& prompt);
    
    /**
     * Get system prompt
     */
    const std::string& getSystemPrompt() const;
    
    /**
     * Save session to file
     * @param path File path
     * @return true if successful
     */
    bool save(const std::string& path) const;
    
    /**
     * Load session from file
     * @param path File path
     * @return true if successful
     */
    bool load(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<LlamaContext> context_;
    std::vector<std::pair<std::string, std::string>> history_;
    std::string systemPrompt_;
    mutable std::mutex mutex_;
};

/**
 * Async generation task
 */
class LlamaAsyncTask {
public:
    LlamaAsyncTask(
        std::shared_ptr<LlamaContext> context,
        const std::string& prompt,
        const SamplingParams& params,
        TokenCallback callback = nullptr
    );
    ~LlamaAsyncTask();
    
    /**
     * Start async generation
     */
    void start();
    
    /**
     * Cancel generation
     */
    void cancel();
    
    /**
     * Wait for completion
     * @param timeout Maximum wait time (0 = infinite)
     * @return true if completed
     */
    bool wait(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
    
    /**
     * Check if completed
     */
    bool isComplete() const;
    
    /**
     * Check if cancelled
     */
    bool isCancelled() const;
    
    /**
     * Get result (blocks if not complete)
     */
    GenerationResult getResult();
    
    /**
     * Get progress (0.0 - 1.0)
     */
    float getProgress() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<LlamaContext> context_;
    std::string prompt_;
    SamplingParams params_;
    TokenCallback callback_;
    std::atomic<bool> complete_{false};
    std::atomic<bool> cancelled_{false};
    std::future<GenerationResult> future_;
};

/**
 * Model manager for loading and caching models
 */
class LlamaModelManager {
public:
    static LlamaModelManager& getInstance();
    
    /**
     * Load or get cached model
     * @param modelPath Path to model file
     * @param params Loading parameters
     * @param progress Progress callback
     * @return Shared pointer to model
     */
    std::shared_ptr<LlamaModel> getModel(
        const std::string& modelPath,
        const ModelLoadParams& params = ModelLoadParams(),
        ProgressCallback progress = nullptr
    );
    
    /**
     * Preload model in background
     * @param modelPath Path to model file
     * @param params Loading parameters
     */
    void preloadModel(
        const std::string& modelPath,
        const ModelLoadParams& params = ModelLoadParams()
    );
    
    /**
     * Unload model from cache
     * @param modelPath Path to model file
     */
    void unloadModel(const std::string& modelPath);
    
    /**
     * Clear all cached models
     */
    void clearCache();
    
    /**
     * Get list of loaded models
     */
    std::vector<std::string> getLoadedModels() const;
    
    /**
     * Get total memory usage
     */
    size_t getTotalMemoryUsage() const;
    
    /**
     * Set maximum cache size in bytes
     * @param maxBytes Maximum memory for cached models
     */
    void setMaxCacheSize(size_t maxBytes);

private:
    LlamaModelManager();
    ~LlamaModelManager();
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::unordered_map<std::string, std::shared_ptr<LlamaModel>> cache_;
    size_t maxCacheSize_ = 0;
    mutable std::mutex mutex_;
};

/**
 * Utility functions
 */
namespace utils {

/**
 * Check if CUDA/Metal GPU acceleration is available
 */
bool isGpuAvailable();

/**
 * Get available GPU memory
 */
size_t getGpuMemory();

/**
 * Get number of available GPU devices
 */
int32_t getGpuDeviceCount();

/**
 * Get optimal thread count for the system
 */
uint32_t getOptimalThreadCount();

/**
 * Parse quantization type from string
 */
QuantizationType parseQuantization(const std::string& str);

/**
 * Convert quantization type to string
 */
std::string quantizationToString(QuantizationType quant);

/**
 * Parse model architecture from string
 */
ModelArchitecture parseArchitecture(const std::string& str);

/**
 * Convert model architecture to string
 */
std::string architectureToString(ModelArchitecture arch);

/**
 * Estimate model memory requirements
 * @param modelPath Path to model file
 * @param contextSize Context size
 * @param batchSize Batch size
 * @return Estimated memory in bytes
 */
size_t estimateMemoryUsage(
    const std::string& modelPath,
    uint32_t contextSize = 4096,
    uint32_t batchSize = 512
);

/**
 * Validate GGUF model file
 * @param modelPath Path to model file
 * @return Empty string if valid, error message otherwise
 */
std::string validateModelFile(const std::string& modelPath);

} // namespace utils

} // namespace llama
} // namespace dte
