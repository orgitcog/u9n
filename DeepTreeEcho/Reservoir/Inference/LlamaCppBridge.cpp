/**
// Vendored from ReZorg/elizaos-cpp llama_cpp.cpp (namespace elizaos->dte). Production builds swap Impl structs for real llama.cpp.
 * @file llama_cpp.cpp
 * @brief API plumbing for the llama.cpp integration surface declared in
 *        LlamaCppBridge.h.
 *
 * This translation unit implements the non-inference parts of the API:
 * model metadata registration, a deterministic word-level tokenizer, grammar
 * bookkeeping, batches, chat sessions, the async task wrapper and the model
 * manager. It contains NO language model. Every text-producing entry point
 * (LlamaContext::generate / continueGeneration, LlamaSession::generateResponse,
 * LlamaAsyncTask) reports StopReason::ERROR with an operator-facing message and
 * returns empty text until a real llama.cpp backend is linked in place of the
 * Impl structs. getNativeHandle() returns nullptr so callers can detect that.
 *
 * Deep Tree Echo obtains words through DTELanguageModelBridge (an
 * OpenAI-compatible client for llama-server / Ollama); this file must never
 * stand in for a model.
 */

#include "LlamaCppBridge.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <sstream>

namespace dte {
namespace llama {

namespace {

constexpr uint32_t kReferenceVocabSize = 32000;
constexpr LlamaToken kBosToken = 1;
constexpr LlamaToken kEosToken = 2;
constexpr LlamaToken kNlToken = 13;
constexpr LlamaToken kPadToken = 0;
constexpr LlamaToken kByteBase = 256; // tokens 3..258 reserved wordpieces start after

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

// Operator-facing reason reported by every generation entry point while no
// inference backend is linked. Never character text.
constexpr const char* kNoBackendMessage =
    "no llama.cpp backend linked; serve the model with llama-server/Ollama "
    "and use DTELanguageModelBridge";

GenerationResult& failNoBackend(GenerationResult& result) {
    result.success = false;
    result.text.clear();
    result.tokens.clear();
    result.completionTokens = 0;
    result.stopReason = StopReason::ERROR;
    result.errorMessage = kNoBackendMessage;
    return result;
}

} // namespace

// ===========================================================================
// LlamaModel::Impl
// ===========================================================================

struct LlamaModel::Impl {
    bool loaded = false;
    std::string path;
    ModelLoadParams params;
    ModelInfo info;
    std::string loraPath;
    float loraScale = 0.0f;
    mutable std::mutex mutex;
};

LlamaModel::LlamaModel() : impl_(std::make_unique<Impl>()) {}

LlamaModel::~LlamaModel() {
    unload();
}

bool LlamaModel::load(const ModelLoadParams& params, ProgressCallback progress) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (params.modelPath.empty()) {
        return false;
    }
    if (progress) {
        progress(0.0f, "registering model metadata (no inference backend linked)");
    }

    impl_->params = params;
    impl_->path = params.modelPath;

    // Register model metadata. If the path exists on disk we record its
    // size. No weights are read and no inference backend is initialised:
    // generation on this model reports StopReason::ERROR until a real
    // llama.cpp backend replaces these Impl structs.
    ModelInfo info;
    info.path = params.modelPath;
    auto slash = params.modelPath.find_last_of("/\\");
    info.name = (slash == std::string::npos)
                    ? params.modelPath
                    : params.modelPath.substr(slash + 1);
    info.architecture = ModelArchitecture::LLAMA;
    info.quantization = (params.quantization != QuantizationType::UNKNOWN)
                            ? params.quantization
                            : QuantizationType::F16;
    info.nVocab = kReferenceVocabSize;
    info.nCtxTrain = 4096;
    info.nEmbd = 512;
    info.nHead = 8;
    info.nHeadKV = 8;
    info.nLayer = 8;
    info.nFF = 2048;
    std::ifstream f(params.modelPath, std::ios::binary | std::ios::ate);
    info.sizeModel = f.good() ? static_cast<size_t>(f.tellg()) : 0;
    info.sizeContext = static_cast<size_t>(info.nEmbd) * 4u;
    info.sizeKvCache = static_cast<size_t>(info.nEmbd) * 2u * sizeof(float);
    info.bosToken = kBosToken;
    info.eosToken = kEosToken;
    info.nlToken = kNlToken;
    info.padToken = kPadToken;
    info.chatTemplate =
        "{% for m in messages %}<|{{m.role}}|>{{m.content}}</s>{% endfor %}"
        "{% if add_generation_prompt %}<|assistant|>{% endif %}";
    info.hasChatTemplate = true;
    impl_->info = info;

    if (!params.loraPath.empty()) {
        impl_->loraPath = params.loraPath;
        impl_->loraScale = params.loraScale;
    }

    impl_->loaded = true;
    if (progress) {
        progress(1.0f, "model metadata registered; no inference backend linked");
    }
    return true;
}

void LlamaModel::unload() {
    if (!impl_) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->loaded = false;
}

bool LlamaModel::isLoaded() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->loaded;
}

ModelInfo LlamaModel::getInfo() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->info;
}

const std::string& LlamaModel::getPath() const {
    return impl_->path;
}

std::shared_ptr<LlamaContext> LlamaModel::createContext(const ContextParams& params) {
    if (!isLoaded()) {
        return nullptr;
    }
    // shared_from_this is not declared in the header, so contexts hold a
    // non-owning alias constructed from the manager/caller-held shared_ptr.
    // The public factory path is LlamaModelManager::getModel() →
    // model->createContext(); we recreate a shared alias with a no-op
    // deleter to preserve the declared signature without double-delete.
    auto alias = std::shared_ptr<LlamaModel>(this, [](LlamaModel*) {});
    return std::make_shared<LlamaContext>(alias, params);
}

std::shared_ptr<LlamaTokenizer> LlamaModel::getTokenizer() {
    if (!isLoaded()) {
        return nullptr;
    }
    auto alias = std::shared_ptr<LlamaModel>(this, [](LlamaModel*) {});
    return std::make_shared<LlamaTokenizer>(alias);
}

bool LlamaModel::applyLoRA(const std::string& loraPath, float scale) {
    if (loraPath.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->loaded) {
        return false;
    }
    impl_->loraPath = loraPath;
    impl_->loraScale = scale;
    return true;
}

void LlamaModel::removeLoRA() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->loraPath.clear();
    impl_->loraScale = 0.0f;
}

void* LlamaModel::getNativeHandle() const {
    return nullptr; // no llama.cpp backend linked
}

// ===========================================================================
// LlamaTokenizer::Impl — deterministic byte/word hybrid tokenizer
// ===========================================================================

struct LlamaTokenizer::Impl {
    // Word-piece registry: deterministic hash-derived ids in
    // [kByteBase+3, kReferenceVocabSize) with collision-stable storage so
    // detokenize() is an exact inverse of tokenize().
    std::unordered_map<std::string, LlamaToken> wordToToken;
    std::unordered_map<LlamaToken, std::string> tokenToWord;
    mutable std::mutex mutex;

    LlamaToken internWord(const std::string& word) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = wordToToken.find(word);
        if (it != wordToToken.end()) {
            return it->second;
        }
        const uint32_t span = kReferenceVocabSize - (kByteBase + 3);
        LlamaToken candidate =
            static_cast<LlamaToken>((fnv1a(word) % span) + kByteBase + 3);
        // Linear-probe hash collisions to keep the mapping bijective.
        while (tokenToWord.count(candidate) > 0) {
            candidate = (candidate + 1 - (kByteBase + 3)) % span + kByteBase + 3;
        }
        wordToToken[word] = candidate;
        tokenToWord[candidate] = word;
        return candidate;
    }

    std::string lookup(LlamaToken token) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = tokenToWord.find(token);
        return it != tokenToWord.end() ? it->second : std::string();
    }
};

LlamaTokenizer::LlamaTokenizer(std::shared_ptr<LlamaModel> model)
    : impl_(std::make_unique<Impl>()), model_(std::move(model)) {}

LlamaTokenizer::~LlamaTokenizer() = default;

TokenSequence LlamaTokenizer::tokenize(const std::string& text, bool addBos,
                                       bool addEos) const {
    TokenSequence tokens;
    if (addBos) {
        tokens.push_back(kBosToken);
    }
    // Whitespace-delimited word pieces with punctuation splits; newlines map
    // to the dedicated newline token to mirror llama vocab conventions.
    std::string word;
    auto flush = [&]() {
        if (!word.empty()) {
            tokens.push_back(impl_->internWord(word));
            word.clear();
        }
    };
    for (char c : text) {
        if (c == '\n') {
            flush();
            tokens.push_back(kNlToken);
        } else if (c == ' ' || c == '\t' || c == '\r') {
            flush();
        } else {
            word.push_back(c);
        }
    }
    flush();
    if (addEos) {
        tokens.push_back(kEosToken);
    }
    return tokens;
}

std::string LlamaTokenizer::detokenize(const TokenSequence& tokens) const {
    std::string out;
    for (LlamaToken t : tokens) {
        if (t == kBosToken || t == kEosToken || t == kPadToken) {
            continue;
        }
        if (t == kNlToken) {
            out += '\n';
            continue;
        }
        const std::string piece = impl_->lookup(t);
        if (piece.empty()) {
            continue;
        }
        if (!out.empty() && out.back() != '\n') {
            out += ' ';
        }
        out += piece;
    }
    return out;
}

std::string LlamaTokenizer::tokenToPiece(LlamaToken token) const {
    if (token == kNlToken) {
        return "\n";
    }
    if (isSpecialToken(token)) {
        return std::string();
    }
    const std::string piece = impl_->lookup(token);
    return piece.empty() ? std::string() : (" " + piece);
}

size_t LlamaTokenizer::vocabSize() const {
    return kReferenceVocabSize;
}

LlamaToken LlamaTokenizer::getBosToken() const { return kBosToken; }
LlamaToken LlamaTokenizer::getEosToken() const { return kEosToken; }
LlamaToken LlamaTokenizer::getNlToken() const { return kNlToken; }
LlamaToken LlamaTokenizer::getPadToken() const { return kPadToken; }

bool LlamaTokenizer::isSpecialToken(LlamaToken token) const {
    return token == kBosToken || token == kEosToken || token == kPadToken;
}

std::string LlamaTokenizer::tokenToString(LlamaToken token, bool showSpecial) const {
    if (token == kBosToken) return showSpecial ? "<s>" : "";
    if (token == kEosToken) return showSpecial ? "</s>" : "";
    if (token == kPadToken) return showSpecial ? "<pad>" : "";
    if (token == kNlToken) return "\n";
    return impl_->lookup(token);
}

std::string LlamaTokenizer::applyChatTemplate(
    const std::vector<std::pair<std::string, std::string>>& messages,
    bool addGenerationPrompt) const {
    std::stringstream ss;
    for (const auto& [role, content] : messages) {
        ss << "<|" << role << "|>" << content << "</s>";
    }
    if (addGenerationPrompt) {
        ss << "<|assistant|>";
    }
    return ss.str();
}

// ===========================================================================
// LlamaGrammar::Impl — token-level constraint automaton
// ===========================================================================

struct LlamaGrammar::Impl {
    std::string grammarStr;
    bool valid = false;
    bool jsonMode = false;
    // Simple constraint model: when jsonMode, track brace/bracket balance to
    // know when the grammar is complete; for BNF we accept any token but
    // recognize completeness when balance returns to zero after opening.
    int depth = 0;
    bool opened = false;
    mutable std::mutex mutex;
};

LlamaGrammar::LlamaGrammar() : impl_(std::make_unique<Impl>()) {}
LlamaGrammar::~LlamaGrammar() = default;

bool LlamaGrammar::loadFromBNF(const std::string& bnf) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (bnf.empty() || bnf.find("::=") == std::string::npos) {
        impl_->valid = false;
        return false;
    }
    impl_->grammarStr = bnf;
    impl_->jsonMode = false;
    impl_->valid = true;
    impl_->depth = 0;
    impl_->opened = false;
    return true;
}

bool LlamaGrammar::loadFromJsonSchema(const std::string& schema) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    // Minimal validation: must look like a JSON object.
    const auto first = schema.find_first_not_of(" \t\n\r");
    if (first == std::string::npos || schema[first] != '{') {
        impl_->valid = false;
        return false;
    }
    impl_->grammarStr = schema;
    impl_->jsonMode = true;
    impl_->valid = true;
    impl_->depth = 0;
    impl_->opened = false;
    return true;
}

bool LlamaGrammar::isTokenAllowed(LlamaToken token) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->valid) {
        return true; // no constraint
    }
    // The reference automaton forbids further tokens once a JSON grammar has
    // closed its root object (completeness), mirroring llama.cpp behavior.
    if (impl_->jsonMode && impl_->opened && impl_->depth == 0) {
        return token == kEosToken;
    }
    return true;
}

void LlamaGrammar::acceptToken(LlamaToken token) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    (void)token;
    if (!impl_->valid || !impl_->jsonMode) {
        return;
    }
    // Token ids carry no text here; the context feeds pieces through
    // acceptPiece via the sampling loop. We approximate by treating every
    // 16th token as a structural close to guarantee termination in the
    // weightless reference engine.
    if (!impl_->opened) {
        impl_->opened = true;
        impl_->depth = 1;
    } else if (impl_->depth > 0) {
        static thread_local int counter = 0;
        if (++counter % 16 == 0) {
            impl_->depth -= 1;
        }
    }
}

void LlamaGrammar::reset() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->depth = 0;
    impl_->opened = false;
}

bool LlamaGrammar::isValid() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->valid;
}

const std::string& LlamaGrammar::getGrammarString() const {
    return impl_->grammarStr;
}

// ===========================================================================
// LlamaBatch::Impl
// ===========================================================================

struct LlamaBatch::Impl {
    size_t maxTokens = 0;
    int32_t maxSeq = 1;
    struct Entry {
        LlamaToken token;
        int32_t seqId;
        int32_t pos;
        bool computeLogits;
    };
    std::vector<Entry> entries;
};

LlamaBatch::LlamaBatch(size_t maxTokens, int32_t maxSeq)
    : impl_(std::make_unique<Impl>()) {
    impl_->maxTokens = maxTokens;
    impl_->maxSeq = maxSeq;
    impl_->entries.reserve(maxTokens);
}

LlamaBatch::~LlamaBatch() = default;

bool LlamaBatch::addTokens(const TokenSequence& tokens, int32_t seqId,
                           int32_t startPos) {
    if (seqId >= impl_->maxSeq) {
        return false;
    }
    if (impl_->entries.size() + tokens.size() > impl_->maxTokens) {
        return false;
    }
    int32_t pos = startPos;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const bool logits = (i + 1 == tokens.size());
        impl_->entries.push_back({tokens[i], seqId, pos++, logits});
    }
    return true;
}

bool LlamaBatch::addToken(LlamaToken token, int32_t seqId, int32_t pos,
                          bool computeLogits) {
    if (seqId >= impl_->maxSeq || isFull()) {
        return false;
    }
    impl_->entries.push_back({token, seqId, pos, computeLogits});
    return true;
}

void LlamaBatch::clear() {
    impl_->entries.clear();
}

size_t LlamaBatch::size() const {
    return impl_->entries.size();
}

bool LlamaBatch::empty() const {
    return impl_->entries.empty();
}

bool LlamaBatch::isFull() const {
    return impl_->entries.size() >= impl_->maxTokens;
}

// ===========================================================================
// LlamaContext::Impl — token bookkeeping only; no inference backend
// ===========================================================================

struct LlamaContext::Impl {
    ContextParams params;
    std::shared_ptr<LlamaTokenizer> tokenizer;
    std::shared_ptr<LlamaGrammar> grammar;
    TokenSequence history; // processed tokens (KV cache analogue)
    std::vector<float> lastLogits; // stays empty: no backend computes logits
    mutable std::mutex mutex;
};

LlamaContext::LlamaContext(std::shared_ptr<LlamaModel> model,
                           const ContextParams& params)
    : impl_(std::make_unique<Impl>()), model_(std::move(model)) {
    impl_->params = params;
    if (model_) {
        impl_->tokenizer = model_->getTokenizer();
    }
}

LlamaContext::~LlamaContext() = default;

bool LlamaContext::process(const TokenSequence& tokens, bool computeLogits) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!model_ || !model_->isLoaded()) {
        return false;
    }
    if (impl_->history.size() + tokens.size() > impl_->params.nCtx) {
        return false;
    }
    impl_->history.insert(impl_->history.end(), tokens.begin(), tokens.end());
    (void)computeLogits; // no backend: logits are never produced
    return true;
}

bool LlamaContext::process(LlamaBatch& batch) {
    TokenSequence tokens;
    tokens.reserve(batch.size());
    for (const auto& e : batch.impl_->entries) {
        tokens.push_back(e.token);
    }
    return process(tokens, true);
}

LlamaToken LlamaContext::sampleToken(const SamplingParams& params) {
    // Sampling requires model logits. No backend is linked, so there is
    // nothing to sample from; -1 signals "no token" to the caller.
    (void)params;
    return -1;
}

GenerationResult LlamaContext::generate(const std::string& prompt,
                                        const SamplingParams& params,
                                        TokenCallback callback) {
    GenerationResult result;
    if (!model_ || !model_->isLoaded() || !impl_->tokenizer) {
        result.success = false;
        result.errorMessage = "model not loaded";
        result.stopReason = StopReason::ERROR;
        return result;
    }
    const auto promptStart = std::chrono::steady_clock::now();
    const TokenSequence promptTokens =
        impl_->tokenizer->tokenize(prompt, /*addBos=*/true);
    if (!process(promptTokens, true)) {
        result.success = false;
        result.errorMessage = "context overflow while processing prompt";
        result.stopReason = StopReason::ERROR;
        return result;
    }
    result.promptTokens = promptTokens.size();
    result.promptTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - promptStart);

    // The prompt is in the context, but there is no model to continue it.
    // The streaming callback is never invoked: no token exists to report.
    (void)params;
    (void)callback;
    return failNoBackend(result);
}

GenerationResult LlamaContext::continueGeneration(const SamplingParams& params,
                                                  TokenCallback callback) {
    GenerationResult result;
    if (!model_ || !model_->isLoaded() || !impl_->tokenizer) {
        result.success = false;
        result.errorMessage = "model not loaded";
        result.stopReason = StopReason::ERROR;
        return result;
    }
    // No inference backend is linked: nothing can be generated and the
    // callback is never invoked.
    (void)params;
    (void)callback;
    return failNoBackend(result);
}

EmbeddingResult LlamaContext::getEmbedding(const std::string& text,
                                           bool normalize) {
    EmbeddingResult result;
    result.text = text;
    if (!model_ || !model_->isLoaded() || !impl_->tokenizer) {
        result.success = false;
        result.errorMessage = "model not loaded";
        return result;
    }
    // Embeddings are model output; without a backend there is none to give.
    (void)normalize;
    result.tokenCount = impl_->tokenizer->tokenize(text, false).size();
    result.dimensions = 0;
    result.embedding.clear();
    result.success = false;
    result.errorMessage = kNoBackendMessage;
    return result;
}

std::vector<float> LlamaContext::getLogits(int32_t tokenIndex) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    (void)tokenIndex; // no backend: never populated
    return impl_->lastLogits;
}

std::vector<std::pair<LlamaToken, float>> LlamaContext::getTopKTokens(
    int32_t k) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<std::pair<LlamaToken, float>> out;
    if (impl_->lastLogits.empty() || k <= 0) {
        return out;
    }
    std::vector<int> idx(impl_->lastLogits.size());
    std::iota(idx.begin(), idx.end(), 0);
    const size_t kk = std::min<size_t>(static_cast<size_t>(k), idx.size());
    std::partial_sort(idx.begin(), idx.begin() + kk, idx.end(),
                      [&](int a, int b) {
                          return impl_->lastLogits[a] > impl_->lastLogits[b];
                      });
    for (size_t i = 0; i < kk; ++i) {
        out.emplace_back(static_cast<LlamaToken>(idx[i]),
                         impl_->lastLogits[idx[i]]);
    }
    return out;
}

void LlamaContext::reset() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->history.clear();
    impl_->lastLogits.clear();
    if (impl_->grammar) {
        impl_->grammar->reset();
    }
}

size_t LlamaContext::getTokenCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->history.size();
}

size_t LlamaContext::getRemainingCapacity() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->history.size() >= impl_->params.nCtx
               ? 0
               : impl_->params.nCtx - impl_->history.size();
}

size_t LlamaContext::getContextSize() const {
    return impl_->params.nCtx;
}

bool LlamaContext::saveState(const std::string& path) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::ofstream f(path, std::ios::binary);
    if (!f.good()) {
        return false;
    }
    const uint64_t magic = 0x454C495A4C4C4D31ULL; // "ELIZLLM1"
    const uint64_t count = impl_->history.size();
    f.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    f.write(reinterpret_cast<const char*>(&count), sizeof(count));
    f.write(reinterpret_cast<const char*>(impl_->history.data()),
            static_cast<std::streamsize>(count * sizeof(LlamaToken)));
    return f.good();
}

bool LlamaContext::loadState(const std::string& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::ifstream f(path, std::ios::binary);
    if (!f.good()) {
        return false;
    }
    uint64_t magic = 0;
    uint64_t count = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != 0x454C495A4C4C4D31ULL) {
        return false;
    }
    f.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (count > impl_->params.nCtx) {
        return false;
    }
    impl_->history.resize(count);
    f.read(reinterpret_cast<char*>(impl_->history.data()),
           static_cast<std::streamsize>(count * sizeof(LlamaToken)));
    if (!f.good()) {
        impl_->history.clear();
        return false;
    }
    return true;
}

void LlamaContext::setGrammar(std::shared_ptr<LlamaGrammar> grammar) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->grammar = std::move(grammar);
}

void LlamaContext::clearGrammar() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->grammar.reset();
}

void* LlamaContext::getNativeHandle() const {
    return nullptr; // no llama.cpp backend linked
}

// ===========================================================================
// LlamaSession::Impl — chat session over a context
// ===========================================================================

struct LlamaSession::Impl {
    std::vector<std::pair<std::string, std::string>> history;
    std::string systemPrompt;
    mutable std::mutex mutex;
};

LlamaSession::LlamaSession(std::shared_ptr<LlamaContext> context)
    : impl_(std::make_unique<Impl>()), context_(std::move(context)) {}

LlamaSession::~LlamaSession() = default;

void LlamaSession::addMessage(const std::string& role,
                              const std::string& content) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->history.emplace_back(role, content);
}

GenerationResult LlamaSession::generateResponse(const SamplingParams& params,
                                                TokenCallback callback) {
    GenerationResult result;
    if (!context_) {
        result.success = false;
        result.errorMessage = "no context attached to session";
        result.stopReason = StopReason::ERROR;
        return result;
    }
    std::vector<std::pair<std::string, std::string>> messages;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->systemPrompt.empty()) {
            messages.emplace_back("system", impl_->systemPrompt);
        }
        messages.insert(messages.end(), impl_->history.begin(),
                        impl_->history.end());
    }
    // Build a chat-template prompt. The context is reset per turn so the
    // full transcript is the single source of truth (stateless re-prompt).
    std::stringstream prompt;
    for (const auto& [role, content] : messages) {
        prompt << "<|" << role << "|>" << content << "</s>";
    }
    prompt << "<|assistant|>";
    context_->reset();
    result = context_->generate(prompt.str(), params, std::move(callback));
    if (result.success) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->history.emplace_back("assistant", result.text);
    }
    return result;
}

std::vector<std::pair<std::string, std::string>> LlamaSession::getHistory()
    const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->history;
}

void LlamaSession::clearHistory() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->history.clear();
}

void LlamaSession::setSystemPrompt(const std::string& prompt) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->systemPrompt = prompt;
}

const std::string& LlamaSession::getSystemPrompt() const {
    return impl_->systemPrompt;
}

bool LlamaSession::save(const std::string& path) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::ofstream f(path);
    if (!f.good()) {
        return false;
    }
    // Line-oriented format with escaped newlines: ROLE\tCONTENT
    auto escape = [](const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '\n') out += "\\n";
            else if (c == '\t') out += "\\t";
            else if (c == '\\') out += "\\\\";
            else out += c;
        }
        return out;
    };
    f << "system\t" << escape(impl_->systemPrompt) << "\n";
    for (const auto& [role, content] : impl_->history) {
        f << escape(role) << "\t" << escape(content) << "\n";
    }
    return f.good();
}

bool LlamaSession::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.good()) {
        return false;
    }
    auto unescape = [](const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                if (s[i + 1] == 'n') { out += '\n'; ++i; continue; }
                if (s[i + 1] == 't') { out += '\t'; ++i; continue; }
                if (s[i + 1] == '\\') { out += '\\'; ++i; continue; }
            }
            out += s[i];
        }
        return out;
    };
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->history.clear();
    std::string line;
    bool first = true;
    while (std::getline(f, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }
        const std::string role = unescape(line.substr(0, tab));
        const std::string content = unescape(line.substr(tab + 1));
        if (first && role == "system") {
            impl_->systemPrompt = content;
            first = false;
            continue;
        }
        first = false;
        impl_->history.emplace_back(role, content);
    }
    return true;
}

// ===========================================================================
// LlamaAsyncTask::Impl
// ===========================================================================

struct LlamaAsyncTask::Impl {
    std::shared_ptr<LlamaContext> context;
    std::string prompt;
    SamplingParams params;
    TokenCallback userCallback;
    std::thread worker;
    std::atomic<bool> started{false};
    std::atomic<bool> complete{false};
    std::atomic<bool> cancelled{false};
    std::atomic<int> tokensEmitted{0};
    GenerationResult result;
    std::mutex resultMutex;
    std::condition_variable cv;
};

LlamaAsyncTask::LlamaAsyncTask(std::shared_ptr<LlamaContext> context,
                               const std::string& prompt,
                               const SamplingParams& params,
                               TokenCallback callback)
    : impl_(std::make_unique<Impl>()) {
    impl_->context = std::move(context);
    impl_->prompt = prompt;
    impl_->params = params;
    impl_->userCallback = std::move(callback);
}

LlamaAsyncTask::~LlamaAsyncTask() {
    cancel();
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
}

void LlamaAsyncTask::start() {
    bool expected = false;
    if (!impl_->started.compare_exchange_strong(expected, true)) {
        return;
    }
    impl_->worker = std::thread([this]() {
        auto wrappedCallback = [this](LlamaToken token,
                                      const std::string& piece) -> bool {
            if (impl_->cancelled.load()) {
                return false;
            }
            impl_->tokensEmitted.fetch_add(1);
            if (impl_->userCallback) {
                return impl_->userCallback(token, piece);
            }
            return true;
        };
        GenerationResult r;
        if (impl_->context) {
            r = impl_->context->generate(impl_->prompt, impl_->params,
                                         wrappedCallback);
        } else {
            r.success = false;
            r.errorMessage = "no context";
            r.stopReason = StopReason::ERROR;
        }
        {
            std::lock_guard<std::mutex> lock(impl_->resultMutex);
            impl_->result = std::move(r);
        }
        impl_->complete.store(true);
        impl_->cv.notify_all();
    });
}

void LlamaAsyncTask::cancel() {
    impl_->cancelled.store(true);
}

bool LlamaAsyncTask::wait(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(impl_->resultMutex);
    if (timeout.count() <= 0) {
        impl_->cv.wait(lock, [this]() { return impl_->complete.load(); });
        return true;
    }
    return impl_->cv.wait_for(lock, timeout,
                              [this]() { return impl_->complete.load(); });
}

bool LlamaAsyncTask::isComplete() const {
    return impl_->complete.load();
}

bool LlamaAsyncTask::isCancelled() const {
    return impl_->cancelled.load();
}

GenerationResult LlamaAsyncTask::getResult() {
    std::lock_guard<std::mutex> lock(impl_->resultMutex);
    return impl_->result;
}

float LlamaAsyncTask::getProgress() const {
    if (impl_->complete.load()) {
        return 1.0f;
    }
    const int emitted = impl_->tokensEmitted.load();
    const int target = std::max(impl_->params.maxTokens, 1);
    return std::min(0.99f, static_cast<float>(emitted) /
                               static_cast<float>(target));
}

// ===========================================================================
// LlamaModelManager
// ===========================================================================

struct LlamaModelManager::Impl {
    std::vector<std::thread> preloadThreads;
};

LlamaModelManager::LlamaModelManager() : impl_(std::make_unique<Impl>()) {}

LlamaModelManager::~LlamaModelManager() {
    for (auto& t : impl_->preloadThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

LlamaModelManager& LlamaModelManager::getInstance() {
    static LlamaModelManager instance;
    return instance;
}

std::shared_ptr<LlamaModel> LlamaModelManager::getModel(
    const std::string& modelPath, const ModelLoadParams& params,
    ProgressCallback progress) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(modelPath);
        if (it != cache_.end() && it->second->isLoaded()) {
            return it->second;
        }
    }
    auto model = std::make_shared<LlamaModel>();
    ModelLoadParams p = params;
    if (p.modelPath.empty()) {
        p.modelPath = modelPath;
    }
    if (!model->load(p, std::move(progress))) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    // Enforce cache budget (approximate: evict arbitrary entries until the
    // new model fits). Reference models are near-zero sized so this mostly
    // matters for the real backend.
    if (maxCacheSize_ > 0) {
        size_t total = model->getInfo().sizeModel;
        for (const auto& [path, m] : cache_) {
            total += m->getInfo().sizeModel;
        }
        auto it = cache_.begin();
        while (total > maxCacheSize_ && it != cache_.end()) {
            total -= it->second->getInfo().sizeModel;
            it = cache_.erase(it);
        }
    }
    cache_[modelPath] = model;
    return model;
}

void LlamaModelManager::preloadModel(const std::string& modelPath,
                                     const ModelLoadParams& params) {
    impl_->preloadThreads.emplace_back(
        [this, modelPath, params]() { getModel(modelPath, params); });
}

void LlamaModelManager::unloadModel(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(modelPath);
    if (it != cache_.end()) {
        it->second->unload();
        cache_.erase(it);
    }
}

void LlamaModelManager::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [path, model] : cache_) {
        model->unload();
    }
    cache_.clear();
}

std::vector<std::string> LlamaModelManager::getLoadedModels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> models;
    models.reserve(cache_.size());
    for (const auto& [path, model] : cache_) {
        models.push_back(path);
    }
    return models;
}

size_t LlamaModelManager::getTotalMemoryUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t total = 0;
    for (const auto& [path, model] : cache_) {
        total += model->getInfo().sizeModel;
    }
    return total;
}

void LlamaModelManager::setMaxCacheSize(size_t maxBytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxCacheSize_ = maxBytes;
}

// ===========================================================================
// utils
// ===========================================================================

namespace utils {

bool isGpuAvailable() {
    return false; // no backend linked
}

size_t getGpuMemory() {
    return 0;
}

int32_t getGpuDeviceCount() {
    return 0;
}

uint32_t getOptimalThreadCount() {
    const unsigned hw = std::thread::hardware_concurrency();
    return hw > 0 ? hw : 4;
}

QuantizationType parseQuantization(const std::string& str) {
    static const std::unordered_map<std::string, QuantizationType> map = {
        {"F32", QuantizationType::F32},       {"F16", QuantizationType::F16},
        {"Q8_0", QuantizationType::Q8_0},     {"Q6_K", QuantizationType::Q6_K},
        {"Q5_K_M", QuantizationType::Q5_K_M}, {"Q5_K_S", QuantizationType::Q5_K_S},
        {"Q4_K_M", QuantizationType::Q4_K_M}, {"Q4_K_S", QuantizationType::Q4_K_S},
        {"Q4_0", QuantizationType::Q4_0},     {"Q4_1", QuantizationType::Q4_1},
        {"Q3_K_M", QuantizationType::Q3_K_M}, {"Q3_K_S", QuantizationType::Q3_K_S},
        {"Q2_K", QuantizationType::Q2_K},     {"IQ4_NL", QuantizationType::IQ4_NL},
        {"IQ3_XXS", QuantizationType::IQ3_XXS},
        {"IQ2_XXS", QuantizationType::IQ2_XXS},
    };
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    auto it = map.find(upper);
    return it != map.end() ? it->second : QuantizationType::UNKNOWN;
}

std::string quantizationToString(QuantizationType quant) {
    switch (quant) {
        case QuantizationType::F32: return "F32";
        case QuantizationType::F16: return "F16";
        case QuantizationType::Q8_0: return "Q8_0";
        case QuantizationType::Q6_K: return "Q6_K";
        case QuantizationType::Q5_K_M: return "Q5_K_M";
        case QuantizationType::Q5_K_S: return "Q5_K_S";
        case QuantizationType::Q4_K_M: return "Q4_K_M";
        case QuantizationType::Q4_K_S: return "Q4_K_S";
        case QuantizationType::Q4_0: return "Q4_0";
        case QuantizationType::Q4_1: return "Q4_1";
        case QuantizationType::Q3_K_M: return "Q3_K_M";
        case QuantizationType::Q3_K_S: return "Q3_K_S";
        case QuantizationType::Q2_K: return "Q2_K";
        case QuantizationType::IQ4_NL: return "IQ4_NL";
        case QuantizationType::IQ3_XXS: return "IQ3_XXS";
        case QuantizationType::IQ2_XXS: return "IQ2_XXS";
        case QuantizationType::UNKNOWN: break;
    }
    return "UNKNOWN";
}

ModelArchitecture parseArchitecture(const std::string& str) {
    static const std::unordered_map<std::string, ModelArchitecture> map = {
        {"LLAMA", ModelArchitecture::LLAMA},
        {"LLAMA2", ModelArchitecture::LLAMA2},
        {"LLAMA3", ModelArchitecture::LLAMA3},
        {"MISTRAL", ModelArchitecture::MISTRAL},
        {"MIXTRAL", ModelArchitecture::MIXTRAL},
        {"FALCON", ModelArchitecture::FALCON},
        {"GPT_NEOX", ModelArchitecture::GPT_NEOX},
        {"GPT2", ModelArchitecture::GPT2},
        {"STARCODER", ModelArchitecture::STARCODER},
        {"PHI", ModelArchitecture::PHI},
        {"QWEN", ModelArchitecture::QWEN},
        {"QWEN2", ModelArchitecture::QWEN2},
        {"GEMMA", ModelArchitecture::GEMMA},
        {"COMMAND_R", ModelArchitecture::COMMAND_R},
        {"DEEPSEEK", ModelArchitecture::DEEPSEEK},
    };
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    auto it = map.find(upper);
    return it != map.end() ? it->second : ModelArchitecture::UNKNOWN;
}

std::string architectureToString(ModelArchitecture arch) {
    switch (arch) {
        case ModelArchitecture::LLAMA: return "LLAMA";
        case ModelArchitecture::LLAMA2: return "LLAMA2";
        case ModelArchitecture::LLAMA3: return "LLAMA3";
        case ModelArchitecture::MISTRAL: return "MISTRAL";
        case ModelArchitecture::MIXTRAL: return "MIXTRAL";
        case ModelArchitecture::FALCON: return "FALCON";
        case ModelArchitecture::GPT_NEOX: return "GPT_NEOX";
        case ModelArchitecture::GPT2: return "GPT2";
        case ModelArchitecture::STARCODER: return "STARCODER";
        case ModelArchitecture::PHI: return "PHI";
        case ModelArchitecture::QWEN: return "QWEN";
        case ModelArchitecture::QWEN2: return "QWEN2";
        case ModelArchitecture::GEMMA: return "GEMMA";
        case ModelArchitecture::COMMAND_R: return "COMMAND_R";
        case ModelArchitecture::DEEPSEEK: return "DEEPSEEK";
        case ModelArchitecture::UNKNOWN: break;
    }
    return "UNKNOWN";
}

size_t estimateMemoryUsage(const std::string& modelPath, uint32_t contextSize,
                           uint32_t batchSize) {
    std::ifstream f(modelPath, std::ios::binary | std::ios::ate);
    const size_t modelBytes =
        f.good() ? static_cast<size_t>(f.tellg()) : (512ull << 20);
    // KV cache: 2 (K and V) * layers * embd * 2 bytes (f16) per token.
    const size_t kvPerToken = 2ull * 8 * 512 * 2;
    const size_t kvBytes = kvPerToken * contextSize;
    const size_t batchBytes = static_cast<size_t>(batchSize) * 512 * 4;
    return modelBytes + kvBytes + batchBytes;
}

std::string validateModelFile(const std::string& modelPath) {
    std::ifstream f(modelPath, std::ios::binary);
    if (!f.good()) {
        return "file not found: " + modelPath;
    }
    char magic[4] = {0};
    f.read(magic, 4);
    if (f.gcount() < 4) {
        return "file too small to be a GGUF model";
    }
    if (std::memcmp(magic, "GGUF", 4) != 0) {
        return "invalid magic bytes (expected GGUF)";
    }
    return std::string();
}

} // namespace utils

} // namespace llama
} // namespace dte
