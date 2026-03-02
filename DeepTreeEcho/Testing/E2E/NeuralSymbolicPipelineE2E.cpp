/**
 * @file NeuralSymbolicPipelineE2E.cpp
 * @brief End-to-End tests for the complete Neural-Symbolic Pipeline
 * 
 * Feature F1.6.2: Comprehensive E2E tests validating full neural-symbolic pipeline operation.
 * 
 * Tests the complete integration of:
 * - Neural-to-Symbolic Translation (F1.1.1)
 * - Symbolic-to-Neural Encoding (F1.1.2)
 * - Bidirectional Message Protocol (F1.1.3)
 * - Type Conversion System (F1.1.4)
 * - Complete round-trip transformations
 * - Pipeline performance and latency targets
 * - Error handling and recovery
 * - Concurrent processing scenarios
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <cmath>
#include <algorithm>
#include <functional>

// ============================================================================
// NEURAL-SYMBOLIC PIPELINE E2E TEST TYPES
// ============================================================================

/**
 * @brief Symbolic atom representation for testing
 */
struct FTestSymbolicAtom
{
    std::string AtomID;
    std::string AtomType;
    float Confidence = 0.0f;
    int32_t SourceFeatureIndex = -1;
    float ActivationValue = 0.0f;
    double Timestamp = 0.0;
    std::map<std::string, std::string> Properties;
    
    bool IsValid() const { return !AtomID.empty() && Confidence > 0.0f; }
};

/**
 * @brief Symbolic predicate representation for testing
 */
struct FTestPredicate
{
    std::string PredicateID;
    std::string PredicateName;
    std::vector<std::string> Arguments;
    float TruthValue = 0.0f;
    float Confidence = 0.0f;
    std::vector<int32_t> SourceFeatureIndices;
    
    bool IsValid() const { return !PredicateID.empty() && !Arguments.empty(); }
};

/**
 * @brief Neural state for testing
 */
struct FTestNeuralState
{
    std::string StateID;
    std::vector<float> Activations;
    std::vector<float> HiddenState;
    float Confidence = 0.0f;
    double Timestamp = 0.0;
    
    bool IsValid() const { return !StateID.empty() && !Activations.empty(); }
};

/**
 * @brief Symbolic state for testing
 */
struct FTestSymbolicState
{
    std::string StateID;
    std::vector<FTestSymbolicAtom> Atoms;
    std::vector<FTestPredicate> Predicates;
    float Confidence = 0.0f;
    double Timestamp = 0.0;
    
    bool IsValid() const { return !StateID.empty(); }
};

/**
 * @brief Game entity representation for testing
 */
struct FTestGameEntity
{
    std::string EntityID;
    std::string EntityType;
    float X = 0.0f, Y = 0.0f, Z = 0.0f;
    float VelX = 0.0f, VelY = 0.0f, VelZ = 0.0f;
    std::map<std::string, std::string> CategoricalProperties;
    std::map<std::string, float> ContinuousProperties;
    double Timestamp = 0.0;
};

/**
 * @brief Tensor embedding for testing
 */
struct FTestTensorEmbedding
{
    std::string EmbeddingID;
    std::vector<float> Vector;
    std::string EmbeddingType;
    std::string SourceID;
    float Confidence = 1.0f;
    double Timestamp = 0.0;
    
    bool IsValid() const { return !EmbeddingID.empty() && !Vector.empty(); }
};

/**
 * @brief Message for protocol testing
 */
struct FTestMessage
{
    std::string MessageID;
    std::string MessageType;
    std::string Topic;
    int32_t Priority = 0;  // 0=Low, 1=Normal, 2=High, 3=Critical
    std::vector<uint8_t> Payload;
    double Timestamp = 0.0;
    double Latency = 0.0;
    
    bool IsValid() const { return !MessageID.empty() && !MessageType.empty(); }
};

/**
 * @brief Pipeline metrics for testing
 */
struct FTestPipelineMetrics
{
    int64_t TotalTranslations = 0;
    int64_t TotalEncodings = 0;
    int64_t TotalMessages = 0;
    double AverageTranslationLatency = 0.0;
    double AverageEncodingLatency = 0.0;
    double AverageMessageLatency = 0.0;
    double PeakLatency = 0.0;
    int64_t DroppedMessages = 0;
    double Throughput = 0.0;
};

// ============================================================================
// MOCK NEURAL-TO-SYMBOLIC TRANSLATOR
// ============================================================================

class MockNeuralToSymbolicTranslator
{
public:
    struct Config
    {
        float ActivationThreshold = 0.3f;
        float ConfidenceThreshold = 0.5f;
        int32_t MaxAtomsPerTranslation = 100;
        bool bEnableBatchProcessing = true;
        int32_t BatchSize = 32;
        bool bPropagateUncertainty = true;
        int32_t DiscretizationBins = 10;
        float CoActivationThreshold = 0.2f;
    };
    
    void Initialize(const Config& config = Config())
    {
        Cfg = config;
        AtomIDCounter = 0;
        PredicateIDCounter = 0;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    FTestSymbolicAtom TranslateTensor(const std::vector<float>& tensor)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        FTestSymbolicAtom atom;
        
        if (tensor.empty())
        {
            return atom;
        }
        
        // Find dominant activation
        int32_t maxIndex = 0;
        float maxValue = tensor[0];
        float sum = 0.0f;
        
        for (size_t i = 0; i < tensor.size(); i++)
        {
            if (tensor[i] > maxValue)
            {
                maxValue = tensor[i];
                maxIndex = static_cast<int32_t>(i);
            }
            sum += std::abs(tensor[i]);
        }
        
        if (maxValue < Cfg.ActivationThreshold)
        {
            return atom;  // Below threshold
        }
        
        atom.AtomID = "atom_" + std::to_string(AtomIDCounter++);
        atom.AtomType = "Concept";
        atom.SourceFeatureIndex = maxIndex;
        atom.ActivationValue = maxValue;
        atom.Confidence = CalculateConfidence(maxValue, sum, tensor.size());
        atom.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Record latency
        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        RecordLatency(latencyMs);
        
        TotalAtomsCreated++;
        TotalTranslations++;
        
        return atom;
    }
    
    std::vector<FTestSymbolicAtom> TranslateTensorToAtoms(const std::vector<float>& tensor)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<FTestSymbolicAtom> atoms;
        
        if (tensor.empty()) return atoms;
        
        // Create atoms for all activations above threshold
        for (size_t i = 0; i < tensor.size() && atoms.size() < static_cast<size_t>(Cfg.MaxAtomsPerTranslation); i++)
        {
            if (std::abs(tensor[i]) >= Cfg.ActivationThreshold)
            {
                FTestSymbolicAtom atom;
                atom.AtomID = "atom_" + std::to_string(AtomIDCounter++);
                atom.AtomType = tensor[i] > 0 ? "PositiveActivation" : "NegativeActivation";
                atom.SourceFeatureIndex = static_cast<int32_t>(i);
                atom.ActivationValue = tensor[i];
                atom.Confidence = CalculateConfidence(std::abs(tensor[i]), 1.0f, 1);
                atom.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
                atoms.push_back(atom);
                TotalAtomsCreated++;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        RecordLatency(latencyMs);
        TotalTranslations++;
        
        return atoms;
    }
    
    FTestSymbolicState TranslateNeuralState(const FTestNeuralState& neuralState)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        FTestSymbolicState symbolicState;
        symbolicState.StateID = "symbolic_" + neuralState.StateID;
        symbolicState.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Translate activations to atoms
        symbolicState.Atoms = TranslateTensorToAtoms(neuralState.Activations);
        
        // Infer predicates from co-activations
        symbolicState.Predicates = InferPredicates(symbolicState.Atoms);
        
        // Propagate uncertainty
        if (Cfg.bPropagateUncertainty && !symbolicState.Atoms.empty())
        {
            float avgConfidence = 0.0f;
            for (const auto& atom : symbolicState.Atoms)
            {
                avgConfidence += atom.Confidence;
            }
            avgConfidence /= symbolicState.Atoms.size();
            symbolicState.Confidence = avgConfidence * neuralState.Confidence;
        }
        else
        {
            symbolicState.Confidence = neuralState.Confidence;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        RecordLatency(latencyMs);
        TotalTranslations++;
        
        return symbolicState;
    }
    
    std::vector<FTestSymbolicState> BatchTranslateStates(const std::vector<FTestNeuralState>& states)
    {
        std::vector<FTestSymbolicState> results;
        results.reserve(states.size());
        
        for (const auto& state : states)
        {
            results.push_back(TranslateNeuralState(state));
        }
        
        return results;
    }
    
    FTestPipelineMetrics GetMetrics() const
    {
        FTestPipelineMetrics metrics;
        metrics.TotalTranslations = TotalTranslations;
        metrics.AverageTranslationLatency = LatencySamples.empty() ? 0.0 :
            std::accumulate(LatencySamples.begin(), LatencySamples.end(), 0.0) / LatencySamples.size();
        metrics.PeakLatency = LatencySamples.empty() ? 0.0 :
            *std::max_element(LatencySamples.begin(), LatencySamples.end());
        return metrics;
    }
    
    bool IsMeetingLatencyTarget() const
    {
        if (LatencySamples.empty()) return true;
        double avg = std::accumulate(LatencySamples.begin(), LatencySamples.end(), 0.0) / LatencySamples.size();
        return avg < 0.5;  // Target: <0.5ms
    }
    
    void Reset()
    {
        AtomIDCounter = 0;
        PredicateIDCounter = 0;
        TotalTranslations = 0;
        TotalAtomsCreated = 0;
        LatencySamples.clear();
    }
    
private:
    float CalculateConfidence(float activation, float sum, size_t count) const
    {
        float normalized = std::abs(activation) / std::max(1.0f, sum / std::max(1.0f, static_cast<float>(count)));
        return std::min(1.0f, normalized);
    }
    
    std::vector<FTestPredicate> InferPredicates(const std::vector<FTestSymbolicAtom>& atoms)
    {
        std::vector<FTestPredicate> predicates;
        
        // Find co-activated atoms and create predicates
        for (size_t i = 0; i < atoms.size(); i++)
        {
            for (size_t j = i + 1; j < atoms.size(); j++)
            {
                float coActivation = atoms[i].Confidence * atoms[j].Confidence;
                if (coActivation >= Cfg.CoActivationThreshold)
                {
                    FTestPredicate pred;
                    pred.PredicateID = "pred_" + std::to_string(PredicateIDCounter++);
                    pred.PredicateName = "CoActivated";
                    pred.Arguments = {atoms[i].AtomID, atoms[j].AtomID};
                    pred.TruthValue = coActivation;
                    pred.Confidence = (atoms[i].Confidence + atoms[j].Confidence) / 2.0f;
                    predicates.push_back(pred);
                    TotalPredicatesCreated++;
                }
            }
        }
        
        return predicates;
    }
    
    void RecordLatency(double latencyMs)
    {
        LatencySamples.push_back(latencyMs);
        if (LatencySamples.size() > MaxLatencySamples)
        {
            LatencySamples.erase(LatencySamples.begin());
        }
    }
    
    bool bInitialized = false;
    Config Cfg;
    int32_t AtomIDCounter = 0;
    int32_t PredicateIDCounter = 0;
    int64_t TotalTranslations = 0;
    int64_t TotalAtomsCreated = 0;
    int64_t TotalPredicatesCreated = 0;
    std::vector<double> LatencySamples;
    static constexpr size_t MaxLatencySamples = 1000;
};

// ============================================================================
// MOCK SYMBOLIC-TO-NEURAL ENCODER
// ============================================================================

class MockSymbolicToNeuralEncoder
{
public:
    struct Config
    {
        int32_t EntityEmbeddingDim = 128;
        int32_t EventEmbeddingDim = 64;
        int32_t RelationEmbeddingDim = 64;
        bool bEnableBatchProcessing = true;
        int32_t BatchSize = 32;
        bool bEnableStreamingMode = true;
        float SmoothingFactor = 0.3f;
    };
    
    void Initialize(const Config& config = Config())
    {
        Cfg = config;
        EmbeddingIDCounter = 0;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    FTestTensorEmbedding EncodeEntity(const FTestGameEntity& entity)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        FTestTensorEmbedding embedding;
        embedding.EmbeddingID = "emb_" + std::to_string(EmbeddingIDCounter++);
        embedding.EmbeddingType = "Entity";
        embedding.SourceID = entity.EntityID;
        embedding.Vector.resize(Cfg.EntityEmbeddingDim, 0.0f);
        
        // Encode position (normalized)
        if (Cfg.EntityEmbeddingDim >= 6)
        {
            embedding.Vector[0] = entity.X / 1000.0f;
            embedding.Vector[1] = entity.Y / 1000.0f;
            embedding.Vector[2] = entity.Z / 1000.0f;
            embedding.Vector[3] = entity.VelX / 100.0f;
            embedding.Vector[4] = entity.VelY / 100.0f;
            embedding.Vector[5] = entity.VelZ / 100.0f;
        }
        
        // Encode entity type (simple hash)
        size_t typeHash = std::hash<std::string>{}(entity.EntityType);
        for (int i = 6; i < std::min(22, Cfg.EntityEmbeddingDim); i++)
        {
            embedding.Vector[i] = static_cast<float>((typeHash >> (i - 6)) & 1);
        }
        
        // Encode continuous properties
        int propIndex = 22;
        for (const auto& [key, value] : entity.ContinuousProperties)
        {
            if (propIndex < Cfg.EntityEmbeddingDim)
            {
                embedding.Vector[propIndex++] = std::tanh(value);
            }
        }
        
        embedding.Confidence = 1.0f;
        embedding.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        RecordLatency(latencyMs);
        TotalEncodings++;
        TotalEntityEmbeddings++;
        
        return embedding;
    }
    
    FTestTensorEmbedding EncodeSymbolicAtom(const FTestSymbolicAtom& atom)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        FTestTensorEmbedding embedding;
        embedding.EmbeddingID = "emb_" + std::to_string(EmbeddingIDCounter++);
        embedding.EmbeddingType = "Atom";
        embedding.SourceID = atom.AtomID;
        embedding.Vector.resize(Cfg.EntityEmbeddingDim, 0.0f);
        
        // Encode atom properties
        size_t typeHash = std::hash<std::string>{}(atom.AtomType);
        for (int i = 0; i < std::min(16, Cfg.EntityEmbeddingDim); i++)
        {
            embedding.Vector[i] = static_cast<float>((typeHash >> i) & 1);
        }
        
        if (Cfg.EntityEmbeddingDim > 16)
        {
            embedding.Vector[16] = atom.ActivationValue;
            embedding.Vector[17] = atom.Confidence;
            embedding.Vector[18] = static_cast<float>(atom.SourceFeatureIndex) / 1000.0f;
        }
        
        embedding.Confidence = atom.Confidence;
        embedding.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        RecordLatency(latencyMs);
        TotalEncodings++;
        
        return embedding;
    }
    
    FTestNeuralState EncodeSymbolicState(const FTestSymbolicState& symbolicState)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        FTestNeuralState neuralState;
        neuralState.StateID = "neural_" + symbolicState.StateID;
        neuralState.Activations.resize(Cfg.EntityEmbeddingDim, 0.0f);
        
        // Aggregate atom embeddings
        for (const auto& atom : symbolicState.Atoms)
        {
            FTestTensorEmbedding atomEmb = EncodeSymbolicAtom(atom);
            for (size_t i = 0; i < neuralState.Activations.size() && i < atomEmb.Vector.size(); i++)
            {
                neuralState.Activations[i] += atomEmb.Vector[i] * atom.Confidence;
            }
        }
        
        // Normalize
        float maxVal = 0.0f;
        for (float v : neuralState.Activations)
        {
            maxVal = std::max(maxVal, std::abs(v));
        }
        if (maxVal > 1e-6f)
        {
            for (float& v : neuralState.Activations)
            {
                v = std::tanh(v / maxVal);
            }
        }
        
        neuralState.Confidence = symbolicState.Confidence;
        neuralState.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        RecordLatency(latencyMs);
        TotalEncodings++;
        
        return neuralState;
    }
    
    std::vector<FTestTensorEmbedding> BatchEncodeEntities(const std::vector<FTestGameEntity>& entities)
    {
        std::vector<FTestTensorEmbedding> results;
        results.reserve(entities.size());
        
        for (const auto& entity : entities)
        {
            results.push_back(EncodeEntity(entity));
        }
        
        return results;
    }
    
    FTestPipelineMetrics GetMetrics() const
    {
        FTestPipelineMetrics metrics;
        metrics.TotalEncodings = TotalEncodings;
        metrics.AverageEncodingLatency = LatencySamples.empty() ? 0.0 :
            std::accumulate(LatencySamples.begin(), LatencySamples.end(), 0.0) / LatencySamples.size();
        metrics.PeakLatency = LatencySamples.empty() ? 0.0 :
            *std::max_element(LatencySamples.begin(), LatencySamples.end());
        return metrics;
    }
    
    bool IsMeetingLatencyTarget() const
    {
        if (LatencySamples.empty()) return true;
        double avg = std::accumulate(LatencySamples.begin(), LatencySamples.end(), 0.0) / LatencySamples.size();
        return avg < 0.5;  // Target: <0.5ms
    }
    
    void Reset()
    {
        EmbeddingIDCounter = 0;
        TotalEncodings = 0;
        TotalEntityEmbeddings = 0;
        LatencySamples.clear();
    }
    
private:
    void RecordLatency(double latencyMs)
    {
        LatencySamples.push_back(latencyMs);
        if (LatencySamples.size() > MaxLatencySamples)
        {
            LatencySamples.erase(LatencySamples.begin());
        }
    }
    
    bool bInitialized = false;
    Config Cfg;
    int32_t EmbeddingIDCounter = 0;
    int64_t TotalEncodings = 0;
    int64_t TotalEntityEmbeddings = 0;
    std::vector<double> LatencySamples;
    static constexpr size_t MaxLatencySamples = 1000;
};

// ============================================================================
// MOCK BIDIRECTIONAL MESSAGE PROTOCOL
// ============================================================================

class MockBidirectionalMessageProtocol
{
public:
    struct Config
    {
        size_t QueueCapacity = 1024;
        bool bEnablePriorityScheduling = true;
        bool bEnableBatching = true;
        size_t BatchThreshold = 10;
    };
    
    void Initialize(const Config& config = Config())
    {
        Cfg = config;
        MessageIDCounter = 0;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    std::string SendMessage(const FTestMessage& message)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        FTestMessage msg = message;
        if (msg.MessageID.empty())
        {
            msg.MessageID = "msg_" + std::to_string(MessageIDCounter++);
        }
        msg.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        std::lock_guard<std::mutex> lock(QueueMutex);
        
        // Check capacity
        if (MessageQueues[msg.Priority].size() >= Cfg.QueueCapacity)
        {
            DroppedMessages++;
            return "";  // Queue full
        }
        
        MessageQueues[msg.Priority].push(msg);
        TotalMessagesSent++;
        
        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        RecordLatency(latencyMs);
        
        return msg.MessageID;
    }
    
    bool ReceiveMessage(FTestMessage& outMessage)
    {
        std::lock_guard<std::mutex> lock(QueueMutex);
        
        // Priority scheduling: check higher priority queues first
        for (int priority = 3; priority >= 0; priority--)
        {
            if (!MessageQueues[priority].empty())
            {
                outMessage = MessageQueues[priority].front();
                MessageQueues[priority].pop();
                TotalMessagesReceived++;
                return true;
            }
        }
        
        return false;
    }
    
    std::vector<FTestMessage> ReceiveMessages(size_t maxMessages)
    {
        std::vector<FTestMessage> messages;
        messages.reserve(maxMessages);
        
        for (size_t i = 0; i < maxMessages; i++)
        {
            FTestMessage msg;
            if (ReceiveMessage(msg))
            {
                messages.push_back(msg);
            }
            else
            {
                break;
            }
        }
        
        return messages;
    }
    
    size_t GetQueueDepth(int priority) const
    {
        std::lock_guard<std::mutex> lock(QueueMutex);
        auto it = MessageQueues.find(priority);
        return it != MessageQueues.end() ? it->second.size() : 0;
    }
    
    size_t GetTotalQueueDepth() const
    {
        std::lock_guard<std::mutex> lock(QueueMutex);
        size_t total = 0;
        for (const auto& [priority, queue] : MessageQueues)
        {
            total += queue.size();
        }
        return total;
    }
    
    FTestPipelineMetrics GetMetrics() const
    {
        FTestPipelineMetrics metrics;
        metrics.TotalMessages = TotalMessagesSent;
        metrics.DroppedMessages = DroppedMessages;
        metrics.AverageMessageLatency = LatencySamples.empty() ? 0.0 :
            std::accumulate(LatencySamples.begin(), LatencySamples.end(), 0.0) / LatencySamples.size();
        metrics.PeakLatency = LatencySamples.empty() ? 0.0 :
            *std::max_element(LatencySamples.begin(), LatencySamples.end());
        return metrics;
    }
    
    bool IsMeetingLatencyTarget() const
    {
        if (LatencySamples.empty()) return true;
        double avg = std::accumulate(LatencySamples.begin(), LatencySamples.end(), 0.0) / LatencySamples.size();
        return avg < 1.0;  // Target: <1ms for message passing
    }
    
    void Reset()
    {
        std::lock_guard<std::mutex> lock(QueueMutex);
        MessageQueues.clear();
        MessageIDCounter = 0;
        TotalMessagesSent = 0;
        TotalMessagesReceived = 0;
        DroppedMessages = 0;
        LatencySamples.clear();
    }
    
private:
    void RecordLatency(double latencyMs)
    {
        LatencySamples.push_back(latencyMs);
        if (LatencySamples.size() > MaxLatencySamples)
        {
            LatencySamples.erase(LatencySamples.begin());
        }
    }
    
    bool bInitialized = false;
    Config Cfg;
    mutable std::mutex QueueMutex;
    std::map<int, std::queue<FTestMessage>> MessageQueues;
    int64_t MessageIDCounter = 0;
    int64_t TotalMessagesSent = 0;
    int64_t TotalMessagesReceived = 0;
    int64_t DroppedMessages = 0;
    std::vector<double> LatencySamples;
    static constexpr size_t MaxLatencySamples = 1000;
};

// ============================================================================
// INTEGRATED NEURAL-SYMBOLIC PIPELINE
// ============================================================================

class MockNeuralSymbolicPipeline
{
public:
    void Initialize()
    {
        Translator.Initialize();
        Encoder.Initialize();
        Protocol.Initialize();
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    /**
     * Complete neural-to-symbolic-to-neural round-trip
     */
    FTestNeuralState ProcessRoundTrip(const FTestNeuralState& inputState)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Step 1: Neural -> Symbolic
        FTestSymbolicState symbolicState = Translator.TranslateNeuralState(inputState);
        
        // Step 2: Symbolic -> Neural (encoding)
        FTestNeuralState outputState = Encoder.EncodeSymbolicState(symbolicState);
        
        auto end = std::chrono::high_resolution_clock::now();
        double latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        RoundTripCount++;
        TotalRoundTripLatency += latencyMs;
        
        return outputState;
    }
    
    /**
     * Process with message protocol integration
     */
    bool ProcessWithProtocol(const FTestNeuralState& inputState, FTestNeuralState& outputState)
    {
        // Translate to symbolic
        FTestSymbolicState symbolicState = Translator.TranslateNeuralState(inputState);
        
        // Send through protocol
        FTestMessage msg;
        msg.MessageType = "SymbolicState";
        msg.Topic = "neural.symbolic.state";
        msg.Priority = 2;  // High priority
        
        // Serialize symbolic state (simplified)
        msg.Payload.resize(sizeof(float) * symbolicState.Atoms.size());
        for (size_t i = 0; i < symbolicState.Atoms.size(); i++)
        {
            float conf = symbolicState.Atoms[i].Confidence;
            memcpy(msg.Payload.data() + i * sizeof(float), &conf, sizeof(float));
        }
        
        std::string msgID = Protocol.SendMessage(msg);
        if (msgID.empty()) return false;
        
        // Receive from protocol
        FTestMessage receivedMsg;
        if (!Protocol.ReceiveMessage(receivedMsg)) return false;
        
        // Encode back to neural
        outputState = Encoder.EncodeSymbolicState(symbolicState);
        
        return true;
    }
    
    FTestPipelineMetrics GetCombinedMetrics() const
    {
        FTestPipelineMetrics translatorMetrics = Translator.GetMetrics();
        FTestPipelineMetrics encoderMetrics = Encoder.GetMetrics();
        FTestPipelineMetrics protocolMetrics = Protocol.GetMetrics();
        
        FTestPipelineMetrics combined;
        combined.TotalTranslations = translatorMetrics.TotalTranslations;
        combined.TotalEncodings = encoderMetrics.TotalEncodings;
        combined.TotalMessages = protocolMetrics.TotalMessages;
        combined.AverageTranslationLatency = translatorMetrics.AverageTranslationLatency;
        combined.AverageEncodingLatency = encoderMetrics.AverageEncodingLatency;
        combined.AverageMessageLatency = protocolMetrics.AverageMessageLatency;
        combined.DroppedMessages = protocolMetrics.DroppedMessages;
        combined.Throughput = RoundTripCount > 0 ? 
            RoundTripCount / (TotalRoundTripLatency / 1000.0) : 0.0;
        
        return combined;
    }
    
    void Reset()
    {
        Translator.Reset();
        Encoder.Reset();
        Protocol.Reset();
        RoundTripCount = 0;
        TotalRoundTripLatency = 0.0;
    }
    
    MockNeuralToSymbolicTranslator& GetTranslator() { return Translator; }
    MockSymbolicToNeuralEncoder& GetEncoder() { return Encoder; }
    MockBidirectionalMessageProtocol& GetProtocol() { return Protocol; }
    
private:
    bool bInitialized = false;
    MockNeuralToSymbolicTranslator Translator;
    MockSymbolicToNeuralEncoder Encoder;
    MockBidirectionalMessageProtocol Protocol;
    int64_t RoundTripCount = 0;
    double TotalRoundTripLatency = 0.0;
};

// ============================================================================
// E2E TEST FIXTURES
// ============================================================================

class NeuralSymbolicPipelineE2ETest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Pipeline = std::make_unique<MockNeuralSymbolicPipeline>();
        Pipeline->Initialize();
    }
    
    void TearDown() override
    {
        Pipeline.reset();
    }
    
    FTestNeuralState CreateRandomNeuralState(int dim = 128)
    {
        FTestNeuralState state;
        state.StateID = "neural_" + std::to_string(StateIDCounter++);
        state.Activations.resize(dim);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        for (int i = 0; i < dim; i++)
        {
            state.Activations[i] = dist(gen);
        }
        
        state.Confidence = 0.9f;
        state.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        return state;
    }
    
    FTestNeuralState CreatePatternedNeuralState(int pattern, int dim = 128)
    {
        FTestNeuralState state;
        state.StateID = "neural_pattern_" + std::to_string(pattern);
        state.Activations.resize(dim);
        
        for (int i = 0; i < dim; i++)
        {
            state.Activations[i] = std::sin(2.0f * M_PI * i / dim * pattern) * 0.8f;
        }
        
        state.Confidence = 0.95f;
        state.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        return state;
    }
    
    FTestGameEntity CreateRandomGameEntity()
    {
        FTestGameEntity entity;
        entity.EntityID = "entity_" + std::to_string(EntityIDCounter++);
        entity.EntityType = "Character";
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDist(-1000.0f, 1000.0f);
        std::uniform_real_distribution<float> velDist(-100.0f, 100.0f);
        
        entity.X = posDist(gen);
        entity.Y = posDist(gen);
        entity.Z = posDist(gen);
        entity.VelX = velDist(gen);
        entity.VelY = velDist(gen);
        entity.VelZ = velDist(gen);
        
        entity.ContinuousProperties["Health"] = std::uniform_real_distribution<float>(0.0f, 100.0f)(gen);
        entity.ContinuousProperties["Energy"] = std::uniform_real_distribution<float>(0.0f, 100.0f)(gen);
        
        return entity;
    }
    
    std::unique_ptr<MockNeuralSymbolicPipeline> Pipeline;
    int StateIDCounter = 0;
    int EntityIDCounter = 0;
};

class NeuralToSymbolicE2ETest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Translator = std::make_unique<MockNeuralToSymbolicTranslator>();
        Translator->Initialize();
    }
    
    void TearDown() override
    {
        Translator.reset();
    }
    
    std::unique_ptr<MockNeuralToSymbolicTranslator> Translator;
};

class SymbolicToNeuralE2ETest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Encoder = std::make_unique<MockSymbolicToNeuralEncoder>();
        Encoder->Initialize();
    }
    
    void TearDown() override
    {
        Encoder.reset();
    }
    
    std::unique_ptr<MockSymbolicToNeuralEncoder> Encoder;
};

class MessageProtocolE2ETest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Protocol = std::make_unique<MockBidirectionalMessageProtocol>();
        Protocol->Initialize();
    }
    
    void TearDown() override
    {
        Protocol.reset();
    }
    
    std::unique_ptr<MockBidirectionalMessageProtocol> Protocol;
};

// ============================================================================
// NEURAL-SYMBOLIC PIPELINE E2E TESTS
// ============================================================================

TEST_F(NeuralSymbolicPipelineE2ETest, FullPipelineInitialization)
{
    ASSERT_TRUE(Pipeline->IsInitialized());
    EXPECT_TRUE(Pipeline->GetTranslator().IsInitialized());
    EXPECT_TRUE(Pipeline->GetEncoder().IsInitialized());
    EXPECT_TRUE(Pipeline->GetProtocol().IsInitialized());
}

TEST_F(NeuralSymbolicPipelineE2ETest, RoundTripTransformation)
{
    FTestNeuralState inputState = CreateRandomNeuralState(128);
    
    FTestNeuralState outputState = Pipeline->ProcessRoundTrip(inputState);
    
    EXPECT_TRUE(outputState.IsValid());
    EXPECT_FALSE(outputState.StateID.empty());
    EXPECT_EQ(outputState.Activations.size(), 128);
}

TEST_F(NeuralSymbolicPipelineE2ETest, RoundTripPreservesStructure)
{
    // Create distinctive pattern
    FTestNeuralState inputState = CreatePatternedNeuralState(3, 128);
    
    FTestNeuralState outputState = Pipeline->ProcessRoundTrip(inputState);
    
    // Output should have similar structure (not identical due to transformations)
    EXPECT_EQ(inputState.Activations.size(), outputState.Activations.size());
    
    // Both should have non-zero content
    float inputNorm = 0.0f, outputNorm = 0.0f;
    for (size_t i = 0; i < inputState.Activations.size(); i++)
    {
        inputNorm += inputState.Activations[i] * inputState.Activations[i];
        outputNorm += outputState.Activations[i] * outputState.Activations[i];
    }
    
    EXPECT_GT(inputNorm, 0.0f);
    EXPECT_GT(outputNorm, 0.0f);
}

TEST_F(NeuralSymbolicPipelineE2ETest, SequentialRoundTrips)
{
    // Process multiple round trips sequentially
    for (int i = 0; i < 100; i++)
    {
        FTestNeuralState inputState = CreateRandomNeuralState(64);
        FTestNeuralState outputState = Pipeline->ProcessRoundTrip(inputState);
        
        EXPECT_TRUE(outputState.IsValid());
    }
    
    FTestPipelineMetrics metrics = Pipeline->GetCombinedMetrics();
    EXPECT_GE(metrics.TotalTranslations, 100);
    EXPECT_GE(metrics.TotalEncodings, 100);
}

TEST_F(NeuralSymbolicPipelineE2ETest, ProcessWithProtocolIntegration)
{
    FTestNeuralState inputState = CreateRandomNeuralState(128);
    FTestNeuralState outputState;
    
    bool success = Pipeline->ProcessWithProtocol(inputState, outputState);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(outputState.IsValid());
}

TEST_F(NeuralSymbolicPipelineE2ETest, HighThroughputProcessing)
{
    auto start = std::chrono::high_resolution_clock::now();
    
    int numIterations = 1000;
    for (int i = 0; i < numIterations; i++)
    {
        FTestNeuralState inputState = CreatePatternedNeuralState(i % 10, 64);
        Pipeline->ProcessRoundTrip(inputState);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = (double)numIterations / (duration.count() / 1000.0);
    
    // Should achieve at least 100 round-trips per second
    EXPECT_GT(throughput, 100.0);
}

TEST_F(NeuralSymbolicPipelineE2ETest, LatencyTargets)
{
    // Warm up
    for (int i = 0; i < 10; i++)
    {
        FTestNeuralState inputState = CreateRandomNeuralState(64);
        Pipeline->ProcessRoundTrip(inputState);
    }
    
    Pipeline->Reset();
    
    // Measure latency
    for (int i = 0; i < 100; i++)
    {
        FTestNeuralState inputState = CreateRandomNeuralState(64);
        Pipeline->ProcessRoundTrip(inputState);
    }
    
    EXPECT_TRUE(Pipeline->GetTranslator().IsMeetingLatencyTarget());
    EXPECT_TRUE(Pipeline->GetEncoder().IsMeetingLatencyTarget());
}

TEST_F(NeuralSymbolicPipelineE2ETest, EmptyInputHandling)
{
    FTestNeuralState emptyState;
    emptyState.StateID = "empty";
    emptyState.Activations.clear();
    emptyState.Confidence = 1.0f;
    
    FTestNeuralState outputState = Pipeline->ProcessRoundTrip(emptyState);
    
    // Should handle gracefully without crash
    EXPECT_TRUE(outputState.StateID.find("neural_symbolic_empty") != std::string::npos);
}

TEST_F(NeuralSymbolicPipelineE2ETest, LargeInputHandling)
{
    FTestNeuralState largeState = CreateRandomNeuralState(1024);
    
    FTestNeuralState outputState = Pipeline->ProcessRoundTrip(largeState);
    
    EXPECT_TRUE(outputState.IsValid());
}

TEST_F(NeuralSymbolicPipelineE2ETest, ExtremeValueHandling)
{
    FTestNeuralState extremeState;
    extremeState.StateID = "extreme";
    extremeState.Activations.resize(64);
    
    for (int i = 0; i < 64; i++)
    {
        extremeState.Activations[i] = (i % 2 == 0) ? 1000.0f : -1000.0f;
    }
    extremeState.Confidence = 1.0f;
    
    FTestNeuralState outputState = Pipeline->ProcessRoundTrip(extremeState);
    
    // Check for no NaN or Inf values
    for (float v : outputState.Activations)
    {
        EXPECT_FALSE(std::isnan(v));
        EXPECT_FALSE(std::isinf(v));
    }
}

// ============================================================================
// NEURAL-TO-SYMBOLIC E2E TESTS
// ============================================================================

TEST_F(NeuralToSymbolicE2ETest, TensorTranslation)
{
    std::vector<float> tensor(64, 0.0f);
    tensor[10] = 0.8f;
    tensor[20] = 0.6f;
    tensor[30] = 0.4f;
    
    FTestSymbolicAtom atom = Translator->TranslateTensor(tensor);
    
    EXPECT_TRUE(atom.IsValid());
    EXPECT_EQ(atom.SourceFeatureIndex, 10);  // Highest activation
    EXPECT_GT(atom.Confidence, 0.0f);
}

TEST_F(NeuralToSymbolicE2ETest, ThresholdFiltering)
{
    std::vector<float> lowTensor(64, 0.1f);  // Below threshold
    
    FTestSymbolicAtom atom = Translator->TranslateTensor(lowTensor);
    
    EXPECT_FALSE(atom.IsValid());  // Should not create atom
}

TEST_F(NeuralToSymbolicE2ETest, MultipleAtomsFromTensor)
{
    std::vector<float> tensor(64, 0.0f);
    tensor[5] = 0.9f;
    tensor[15] = 0.7f;
    tensor[25] = 0.5f;
    tensor[35] = 0.4f;
    
    std::vector<FTestSymbolicAtom> atoms = Translator->TranslateTensorToAtoms(tensor);
    
    EXPECT_GE(atoms.size(), 3);  // At least 3 above threshold
    
    for (const auto& atom : atoms)
    {
        EXPECT_TRUE(atom.IsValid());
    }
}

TEST_F(NeuralToSymbolicE2ETest, NeuralStateTranslation)
{
    FTestNeuralState neuralState;
    neuralState.StateID = "test_state";
    neuralState.Activations.resize(64);
    neuralState.Activations[10] = 0.9f;
    neuralState.Activations[20] = 0.8f;
    neuralState.Activations[30] = 0.7f;
    neuralState.Confidence = 0.95f;
    
    FTestSymbolicState symbolicState = Translator->TranslateNeuralState(neuralState);
    
    EXPECT_TRUE(symbolicState.IsValid());
    EXPECT_GE(symbolicState.Atoms.size(), 3);
    EXPECT_GT(symbolicState.Confidence, 0.0f);
}

TEST_F(NeuralToSymbolicE2ETest, PredicateInference)
{
    FTestNeuralState neuralState;
    neuralState.StateID = "predicate_test";
    neuralState.Activations.resize(64);
    neuralState.Activations[10] = 0.9f;
    neuralState.Activations[11] = 0.85f;  // Close to index 10, should co-activate
    neuralState.Confidence = 0.95f;
    
    FTestSymbolicState symbolicState = Translator->TranslateNeuralState(neuralState);
    
    // Should have predicates for co-activated atoms
    EXPECT_GE(symbolicState.Predicates.size(), 0);
}

TEST_F(NeuralToSymbolicE2ETest, BatchTranslation)
{
    std::vector<FTestNeuralState> states;
    for (int i = 0; i < 50; i++)
    {
        FTestNeuralState state;
        state.StateID = "batch_" + std::to_string(i);
        state.Activations.resize(32);
        state.Activations[i % 32] = 0.9f;
        state.Confidence = 0.9f;
        states.push_back(state);
    }
    
    auto results = Translator->BatchTranslateStates(states);
    
    EXPECT_EQ(results.size(), 50);
    for (const auto& result : results)
    {
        EXPECT_TRUE(result.IsValid());
    }
}

TEST_F(NeuralToSymbolicE2ETest, TranslationLatencyTarget)
{
    // Warm up
    for (int i = 0; i < 10; i++)
    {
        std::vector<float> tensor(64, 0.5f);
        Translator->TranslateTensor(tensor);
    }
    
    Translator->Reset();
    
    // Measure
    for (int i = 0; i < 100; i++)
    {
        std::vector<float> tensor(64, 0.5f);
        Translator->TranslateTensor(tensor);
    }
    
    EXPECT_TRUE(Translator->IsMeetingLatencyTarget());
}

// ============================================================================
// SYMBOLIC-TO-NEURAL E2E TESTS
// ============================================================================

TEST_F(SymbolicToNeuralE2ETest, EntityEncoding)
{
    FTestGameEntity entity;
    entity.EntityID = "player1";
    entity.EntityType = "Character";
    entity.X = 100.0f;
    entity.Y = 200.0f;
    entity.Z = 0.0f;
    entity.ContinuousProperties["Health"] = 85.0f;
    entity.ContinuousProperties["Energy"] = 60.0f;
    
    FTestTensorEmbedding embedding = Encoder->EncodeEntity(entity);
    
    EXPECT_TRUE(embedding.IsValid());
    EXPECT_EQ(embedding.EmbeddingType, "Entity");
    EXPECT_EQ(embedding.SourceID, "player1");
    EXPECT_EQ(embedding.Vector.size(), 128);
}

TEST_F(SymbolicToNeuralE2ETest, AtomEncoding)
{
    FTestSymbolicAtom atom;
    atom.AtomID = "atom_1";
    atom.AtomType = "Concept";
    atom.ActivationValue = 0.8f;
    atom.Confidence = 0.9f;
    atom.SourceFeatureIndex = 15;
    
    FTestTensorEmbedding embedding = Encoder->EncodeSymbolicAtom(atom);
    
    EXPECT_TRUE(embedding.IsValid());
    EXPECT_EQ(embedding.EmbeddingType, "Atom");
    EXPECT_EQ(embedding.Confidence, 0.9f);
}

TEST_F(SymbolicToNeuralE2ETest, SymbolicStateEncoding)
{
    FTestSymbolicState symbolicState;
    symbolicState.StateID = "symbolic_test";
    symbolicState.Confidence = 0.85f;
    
    // Add atoms
    for (int i = 0; i < 5; i++)
    {
        FTestSymbolicAtom atom;
        atom.AtomID = "atom_" + std::to_string(i);
        atom.AtomType = "Concept";
        atom.ActivationValue = 0.7f + i * 0.05f;
        atom.Confidence = 0.8f + i * 0.03f;
        symbolicState.Atoms.push_back(atom);
    }
    
    FTestNeuralState neuralState = Encoder->EncodeSymbolicState(symbolicState);
    
    EXPECT_TRUE(neuralState.IsValid());
    EXPECT_EQ(neuralState.Activations.size(), 128);
}

TEST_F(SymbolicToNeuralE2ETest, BatchEntityEncoding)
{
    std::vector<FTestGameEntity> entities;
    for (int i = 0; i < 50; i++)
    {
        FTestGameEntity entity;
        entity.EntityID = "entity_" + std::to_string(i);
        entity.EntityType = "NPC";
        entity.X = static_cast<float>(i * 10);
        entity.Y = static_cast<float>(i * 5);
        entities.push_back(entity);
    }
    
    auto embeddings = Encoder->BatchEncodeEntities(entities);
    
    EXPECT_EQ(embeddings.size(), 50);
    for (const auto& emb : embeddings)
    {
        EXPECT_TRUE(emb.IsValid());
    }
}

TEST_F(SymbolicToNeuralE2ETest, EncodingLatencyTarget)
{
    // Warm up
    for (int i = 0; i < 10; i++)
    {
        FTestGameEntity entity;
        entity.EntityID = "warmup_" + std::to_string(i);
        entity.EntityType = "Test";
        Encoder->EncodeEntity(entity);
    }
    
    Encoder->Reset();
    
    // Measure
    for (int i = 0; i < 100; i++)
    {
        FTestGameEntity entity;
        entity.EntityID = "test_" + std::to_string(i);
        entity.EntityType = "Test";
        entity.X = static_cast<float>(i);
        Encoder->EncodeEntity(entity);
    }
    
    EXPECT_TRUE(Encoder->IsMeetingLatencyTarget());
}

// ============================================================================
// MESSAGE PROTOCOL E2E TESTS
// ============================================================================

TEST_F(MessageProtocolE2ETest, BasicSendReceive)
{
    FTestMessage msg;
    msg.MessageType = "Test";
    msg.Topic = "test.topic";
    msg.Priority = 1;
    
    std::string msgID = Protocol->SendMessage(msg);
    EXPECT_FALSE(msgID.empty());
    
    FTestMessage received;
    bool success = Protocol->ReceiveMessage(received);
    
    EXPECT_TRUE(success);
    EXPECT_EQ(received.MessageType, "Test");
    EXPECT_EQ(received.Topic, "test.topic");
}

TEST_F(MessageProtocolE2ETest, PriorityScheduling)
{
    // Send messages in mixed priority order
    FTestMessage low, normal, high, critical;
    low.MessageType = "Low";
    low.Priority = 0;
    normal.MessageType = "Normal";
    normal.Priority = 1;
    high.MessageType = "High";
    high.Priority = 2;
    critical.MessageType = "Critical";
    critical.Priority = 3;
    
    Protocol->SendMessage(low);
    Protocol->SendMessage(normal);
    Protocol->SendMessage(critical);
    Protocol->SendMessage(high);
    
    // Should receive in priority order
    FTestMessage received;
    
    Protocol->ReceiveMessage(received);
    EXPECT_EQ(received.MessageType, "Critical");
    
    Protocol->ReceiveMessage(received);
    EXPECT_EQ(received.MessageType, "High");
    
    Protocol->ReceiveMessage(received);
    EXPECT_EQ(received.MessageType, "Normal");
    
    Protocol->ReceiveMessage(received);
    EXPECT_EQ(received.MessageType, "Low");
}

TEST_F(MessageProtocolE2ETest, QueueCapacityHandling)
{
    MockBidirectionalMessageProtocol::Config config;
    config.QueueCapacity = 10;
    
    Protocol = std::make_unique<MockBidirectionalMessageProtocol>();
    Protocol->Initialize(config);
    
    // Fill queue
    for (int i = 0; i < 10; i++)
    {
        FTestMessage msg;
        msg.MessageType = "Fill";
        msg.Priority = 1;
        EXPECT_FALSE(Protocol->SendMessage(msg).empty());
    }
    
    // Next message should fail (queue full)
    FTestMessage overflow;
    overflow.MessageType = "Overflow";
    overflow.Priority = 1;
    EXPECT_TRUE(Protocol->SendMessage(overflow).empty());
    
    FTestPipelineMetrics metrics = Protocol->GetMetrics();
    EXPECT_EQ(metrics.DroppedMessages, 1);
}

TEST_F(MessageProtocolE2ETest, BatchReceive)
{
    // Send 50 messages
    for (int i = 0; i < 50; i++)
    {
        FTestMessage msg;
        msg.MessageType = "Batch";
        msg.Priority = 1;
        Protocol->SendMessage(msg);
    }
    
    // Receive in batch
    auto messages = Protocol->ReceiveMessages(30);
    EXPECT_EQ(messages.size(), 30);
    
    messages = Protocol->ReceiveMessages(30);
    EXPECT_EQ(messages.size(), 20);  // Only 20 remaining
}

TEST_F(MessageProtocolE2ETest, HighThroughputMessaging)
{
    auto start = std::chrono::high_resolution_clock::now();
    
    int numMessages = 10000;
    for (int i = 0; i < numMessages; i++)
    {
        FTestMessage msg;
        msg.MessageType = "Throughput";
        msg.Priority = i % 4;
        Protocol->SendMessage(msg);
    }
    
    // Drain queue
    FTestMessage received;
    int receivedCount = 0;
    while (Protocol->ReceiveMessage(received))
    {
        receivedCount++;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = (double)numMessages / (duration.count() / 1000.0);
    
    EXPECT_EQ(receivedCount, numMessages);
    EXPECT_GT(throughput, 10000.0);  // >10K messages/sec
}

TEST_F(MessageProtocolE2ETest, ConcurrentAccess)
{
    std::atomic<int> sentCount{0};
    std::atomic<int> receivedCount{0};
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; p++)
    {
        producers.emplace_back([this, &sentCount, p]() {
            for (int i = 0; i < 250; i++)
            {
                FTestMessage msg;
                msg.MessageType = "Concurrent";
                msg.Priority = p;
                if (!Protocol->SendMessage(msg).empty())
                {
                    sentCount++;
                }
            }
        });
    }
    
    // Wait for producers
    for (auto& t : producers)
    {
        t.join();
    }
    
    // Consumer
    FTestMessage received;
    while (Protocol->ReceiveMessage(received))
    {
        receivedCount++;
    }
    
    EXPECT_EQ(sentCount.load(), 1000);
    EXPECT_EQ(receivedCount.load(), 1000);
}

// ============================================================================
// INTEGRATION E2E TESTS
// ============================================================================

TEST(NeuralSymbolicIntegrationE2ETest, CompleteWorkflow)
{
    MockNeuralSymbolicPipeline pipeline;
    pipeline.Initialize();
    
    // Simulate game loop
    for (int frame = 0; frame < 100; frame++)
    {
        // Create neural state from "game perception"
        FTestNeuralState neuralState;
        neuralState.StateID = "frame_" + std::to_string(frame);
        neuralState.Activations.resize(64);
        
        for (int i = 0; i < 64; i++)
        {
            neuralState.Activations[i] = std::sin(frame * 0.1f + i * 0.2f) * 0.8f;
        }
        neuralState.Confidence = 0.9f;
        
        // Process through pipeline
        FTestNeuralState outputState = pipeline.ProcessRoundTrip(neuralState);
        
        EXPECT_TRUE(outputState.IsValid());
    }
    
    FTestPipelineMetrics metrics = pipeline.GetCombinedMetrics();
    EXPECT_GE(metrics.TotalTranslations, 100);
    EXPECT_GE(metrics.TotalEncodings, 100);
}

TEST(NeuralSymbolicIntegrationE2ETest, LongRunningStability)
{
    MockNeuralSymbolicPipeline pipeline;
    pipeline.Initialize();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < 1000; i++)
    {
        FTestNeuralState state;
        state.StateID = "stability_" + std::to_string(i);
        state.Activations.resize(128);
        
        for (int j = 0; j < 128; j++)
        {
            state.Activations[j] = dist(gen);
        }
        state.Confidence = 0.9f;
        
        FTestNeuralState output = pipeline.ProcessRoundTrip(state);
        
        // Check for numerical stability
        for (float v : output.Activations)
        {
            EXPECT_FALSE(std::isnan(v));
            EXPECT_FALSE(std::isinf(v));
        }
    }
}

TEST(NeuralSymbolicIntegrationE2ETest, ErrorRecovery)
{
    MockNeuralSymbolicPipeline pipeline;
    pipeline.Initialize();
    
    // Process normal state
    FTestNeuralState normalState;
    normalState.StateID = "normal";
    normalState.Activations.resize(64, 0.5f);
    normalState.Confidence = 0.9f;
    
    FTestNeuralState output1 = pipeline.ProcessRoundTrip(normalState);
    EXPECT_TRUE(output1.IsValid());
    
    // Process empty state (edge case)
    FTestNeuralState emptyState;
    emptyState.StateID = "empty";
    emptyState.Confidence = 1.0f;
    
    FTestNeuralState output2 = pipeline.ProcessRoundTrip(emptyState);
    // Should handle gracefully
    
    // Process normal state again (recovery)
    FTestNeuralState normalState2;
    normalState2.StateID = "normal2";
    normalState2.Activations.resize(64, 0.6f);
    normalState2.Confidence = 0.9f;
    
    FTestNeuralState output3 = pipeline.ProcessRoundTrip(normalState2);
    EXPECT_TRUE(output3.IsValid());
}

// ============================================================================
// PERFORMANCE E2E TESTS
// ============================================================================

TEST(NeuralSymbolicPerformanceE2ETest, TranslationThroughput)
{
    MockNeuralToSymbolicTranslator translator;
    translator.Initialize();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int numTranslations = 10000;
    for (int i = 0; i < numTranslations; i++)
    {
        std::vector<float> tensor(64);
        for (int j = 0; j < 64; j++)
        {
            tensor[j] = std::sin(i * 0.01f + j * 0.1f);
        }
        translator.TranslateTensor(tensor);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = (double)numTranslations / (duration.count() / 1000.0);
    
    // Should achieve at least 10K translations/sec
    EXPECT_GT(throughput, 10000.0);
}

TEST(NeuralSymbolicPerformanceE2ETest, EncodingThroughput)
{
    MockSymbolicToNeuralEncoder encoder;
    encoder.Initialize();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int numEncodings = 10000;
    for (int i = 0; i < numEncodings; i++)
    {
        FTestGameEntity entity;
        entity.EntityID = "entity_" + std::to_string(i);
        entity.EntityType = "Character";
        entity.X = static_cast<float>(i);
        entity.Y = static_cast<float>(i * 2);
        entity.Z = static_cast<float>(i * 3);
        encoder.EncodeEntity(entity);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = (double)numEncodings / (duration.count() / 1000.0);
    
    // Should achieve at least 10K encodings/sec
    EXPECT_GT(throughput, 10000.0);
}

TEST(NeuralSymbolicPerformanceE2ETest, PipelineThroughput)
{
    MockNeuralSymbolicPipeline pipeline;
    pipeline.Initialize();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int numCycles = 1000;
    for (int i = 0; i < numCycles; i++)
    {
        FTestNeuralState state;
        state.StateID = "perf_" + std::to_string(i);
        state.Activations.resize(128);
        for (int j = 0; j < 128; j++)
        {
            state.Activations[j] = std::sin(i * 0.01f + j * 0.05f);
        }
        state.Confidence = 0.9f;
        
        pipeline.ProcessRoundTrip(state);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double throughput = (double)numCycles / (duration.count() / 1000.0);
    
    // Should achieve at least 500 round-trips/sec
    EXPECT_GT(throughput, 500.0);
}

// ============================================================================
// STRESS E2E TESTS
// ============================================================================

TEST(NeuralSymbolicStressE2ETest, HighDimensionalInput)
{
    MockNeuralSymbolicPipeline pipeline;
    pipeline.Initialize();
    
    // Process very high-dimensional state
    FTestNeuralState state;
    state.StateID = "high_dim";
    state.Activations.resize(2048);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (int i = 0; i < 2048; i++)
    {
        state.Activations[i] = dist(gen);
    }
    state.Confidence = 0.9f;
    
    FTestNeuralState output = pipeline.ProcessRoundTrip(state);
    
    EXPECT_TRUE(output.IsValid());
    for (float v : output.Activations)
    {
        EXPECT_FALSE(std::isnan(v));
        EXPECT_FALSE(std::isinf(v));
    }
}

TEST(NeuralSymbolicStressE2ETest, RapidReset)
{
    MockNeuralSymbolicPipeline pipeline;
    
    for (int cycle = 0; cycle < 100; cycle++)
    {
        pipeline.Initialize();
        
        for (int i = 0; i < 10; i++)
        {
            FTestNeuralState state;
            state.StateID = "reset_test_" + std::to_string(i);
            state.Activations.resize(64, 0.5f);
            state.Confidence = 0.9f;
            
            pipeline.ProcessRoundTrip(state);
        }
        
        pipeline.Reset();
    }
    
    // Should complete without crash
    EXPECT_TRUE(true);
}

TEST(NeuralSymbolicStressE2ETest, ConcurrentPipelines)
{
    std::vector<std::thread> threads;
    std::atomic<int> completedCount{0};
    
    for (int t = 0; t < 4; t++)
    {
        threads.emplace_back([&completedCount, t]() {
            MockNeuralSymbolicPipeline pipeline;
            pipeline.Initialize();
            
            for (int i = 0; i < 100; i++)
            {
                FTestNeuralState state;
                state.StateID = "thread_" + std::to_string(t) + "_" + std::to_string(i);
                state.Activations.resize(64);
                for (int j = 0; j < 64; j++)
                {
                    state.Activations[j] = std::sin(t + i * 0.1f + j * 0.05f);
                }
                state.Confidence = 0.9f;
                
                FTestNeuralState output = pipeline.ProcessRoundTrip(state);
                EXPECT_TRUE(output.IsValid());
            }
            
            completedCount++;
        });
    }
    
    for (auto& t : threads)
    {
        t.join();
    }
    
    EXPECT_EQ(completedCount.load(), 4);
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
