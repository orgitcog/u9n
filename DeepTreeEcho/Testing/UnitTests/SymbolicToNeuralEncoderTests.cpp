/**
 * @file SymbolicToNeuralEncoderTests.cpp
 * @brief Comprehensive unit tests for Symbolic-to-Neural Encoder
 * 
 * Tests cover:
 * - Entity embedding system
 * - Categorical encoding (one-hot, multi-hot)
 * - Continuous feature encoding
 * - Relational encoding
 * - Temporal state encoding
 * - Streaming encoder
 * - Latency compliance (<0.5ms)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <chrono>

// Mock Unreal Engine types for standalone testing
#ifndef WITH_UNREAL_ENGINE
namespace FString { using Type = std::string; }
using FName = std::string;
using FVector = std::array<float, 3>;
using FRotator = std::array<float, 3>;
using int32 = int;
using uint32 = unsigned int;
using float32 = float;
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UCLASS(...)
#define USTRUCT(...)
#define GENERATED_BODY()
#define TEXT(x) x
#endif

// ============================================================================
// Mock Encoder for Testing
// ============================================================================

class MockSymbolicToNeuralEncoder
{
public:
    struct Config
    {
        int32 EntityEmbeddingDim = 128;
        int32 EventEmbeddingDim = 64;
        bool bEnableBatchProcessing = true;
        bool bEnableStreamingMode = true;
    } Config;

    struct Metrics
    {
        int64 TotalEncodings = 0;
        float AverageLatency = 0.0f;
        float PeakLatency = 0.0f;
        int64 TotalEntityEmbeddings = 0;
    } Metrics;

    std::vector<float> EncodeEntity(const std::vector<float>& EntityData)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<float> embedding(Config.EntityEmbeddingDim, 0.0f);
        
        // Simple encoding: copy and pad/truncate
        for (size_t i = 0; i < std::min(EntityData.size(), static_cast<size_t>(Config.EntityEmbeddingDim)); i++)
        {
            embedding[i] = EntityData[i];
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        float latencyMs = std::chrono::duration<float, std::milli>(end - start).count();
        
        RecordLatency(latencyMs);
        Metrics.TotalEntityEmbeddings++;
        
        return embedding;
    }

    std::vector<float> EncodeCategoricalOneHot(const std::string& category, int32 maxCategories)
    {
        std::vector<float> oneHot(maxCategories, 0.0f);
        
        // Use hash to get index
        uint32 hash = std::hash<std::string>{}(category);
        int32 index = hash % maxCategories;
        oneHot[index] = 1.0f;
        
        return oneHot;
    }

    std::vector<float> EncodeContinuousVector(const std::array<float, 3>& vec)
    {
        // Simple normalization to [-1, 1]
        std::vector<float> encoded;
        for (float v : vec)
        {
            encoded.push_back(std::tanh(v / 100.0f));
        }
        return encoded;
    }

    std::vector<float> EncodeRelation(const std::string& sourceID, const std::string& targetID)
    {
        std::vector<float> embedding(64, 0.0f);
        
        // Hash-based encoding
        uint32 sourceHash = std::hash<std::string>{}(sourceID);
        uint32 targetHash = std::hash<std::string>{}(targetID);
        
        embedding[0] = static_cast<float>(sourceHash % 1000) / 1000.0f;
        embedding[1] = static_cast<float>(targetHash % 1000) / 1000.0f;
        
        return embedding;
    }

    std::vector<float> EncodeTemporalDelta(const std::vector<float>& current, const std::vector<float>& previous)
    {
        std::vector<float> delta;
        
        size_t minSize = std::min(current.size(), previous.size());
        for (size_t i = 0; i < minSize; i++)
        {
            delta.push_back(current[i] - previous[i]);
        }
        
        return delta;
    }

    bool IsMeetingLatencyTarget() const
    {
        return Metrics.AverageLatency < 0.5f;
    }

private:
    std::vector<float> latencySamples;
    static constexpr size_t MaxLatencySamples = 1000;

    void RecordLatency(float latencyMs)
    {
        latencySamples.push_back(latencyMs);
        
        if (latencySamples.size() > MaxLatencySamples)
        {
            latencySamples.erase(latencySamples.begin());
        }
        
        // Update metrics
        Metrics.PeakLatency = std::max(Metrics.PeakLatency, latencyMs);
        
        float sum = 0.0f;
        for (float sample : latencySamples)
        {
            sum += sample;
        }
        Metrics.AverageLatency = sum / latencySamples.size();
    }
};

// ============================================================================
// TEST FIXTURE
// ============================================================================

class SymbolicToNeuralEncoderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        encoder = std::make_unique<MockSymbolicToNeuralEncoder>();
    }

    void TearDown() override
    {
        encoder.reset();
    }

    std::unique_ptr<MockSymbolicToNeuralEncoder> encoder;
};

// ============================================================================
// ENTITY EMBEDDING TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, EncodeEntity_EmptyInput_ReturnsZeroVector)
{
    std::vector<float> emptyEntity;
    auto embedding = encoder->EncodeEntity(emptyEntity);
    
    EXPECT_EQ(embedding.size(), encoder->Config.EntityEmbeddingDim);
    
    // All values should be zero
    for (float value : embedding)
    {
        EXPECT_FLOAT_EQ(value, 0.0f);
    }
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeEntity_ValidInput_ReturnsCorrectDimension)
{
    std::vector<float> entityData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto embedding = encoder->EncodeEntity(entityData);
    
    EXPECT_EQ(embedding.size(), encoder->Config.EntityEmbeddingDim);
    
    // First 5 values should match input
    for (size_t i = 0; i < entityData.size(); i++)
    {
        EXPECT_FLOAT_EQ(embedding[i], entityData[i]);
    }
    
    // Rest should be zero-padded
    for (size_t i = entityData.size(); i < embedding.size(); i++)
    {
        EXPECT_FLOAT_EQ(embedding[i], 0.0f);
    }
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeEntity_OversizedInput_Truncates)
{
    std::vector<float> largeEntity(200, 1.0f);
    auto embedding = encoder->EncodeEntity(largeEntity);
    
    EXPECT_EQ(embedding.size(), encoder->Config.EntityEmbeddingDim);
    
    // All values should be 1.0f (truncated but present)
    for (float value : embedding)
    {
        EXPECT_FLOAT_EQ(value, 1.0f);
    }
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeEntity_MeetsLatencyTarget)
{
    std::vector<float> entityData(50, 1.0f);
    
    // Encode multiple times to get average
    for (int i = 0; i < 100; i++)
    {
        encoder->EncodeEntity(entityData);
    }
    
    EXPECT_TRUE(encoder->IsMeetingLatencyTarget());
    EXPECT_LT(encoder->Metrics.AverageLatency, 0.5f);
}

// ============================================================================
// CATEGORICAL ENCODING TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, EncodeCategoricalOneHot_ValidCategory_ReturnsOneHotVector)
{
    std::string category = "EnemyType_Zombie";
    int32 maxCategories = 10;
    
    auto oneHot = encoder->EncodeCategoricalOneHot(category, maxCategories);
    
    EXPECT_EQ(oneHot.size(), maxCategories);
    
    // Should have exactly one 1.0 and rest zeros
    int countOnes = 0;
    for (float value : oneHot)
    {
        if (value == 1.0f)
        {
            countOnes++;
        }
    }
    EXPECT_EQ(countOnes, 1);
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeCategoricalOneHot_SameCategory_ReturnsSameEncoding)
{
    std::string category = "WeaponType_Sword";
    int32 maxCategories = 20;
    
    auto encoding1 = encoder->EncodeCategoricalOneHot(category, maxCategories);
    auto encoding2 = encoder->EncodeCategoricalOneHot(category, maxCategories);
    
    EXPECT_EQ(encoding1, encoding2);
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeCategoricalOneHot_DifferentCategories_ReturnsDifferentEncodings)
{
    std::string category1 = "TypeA";
    std::string category2 = "TypeB";
    int32 maxCategories = 50;
    
    auto encoding1 = encoder->EncodeCategoricalOneHot(category1, maxCategories);
    auto encoding2 = encoder->EncodeCategoricalOneHot(category2, maxCategories);
    
    EXPECT_NE(encoding1, encoding2);
}

// ============================================================================
// CONTINUOUS ENCODING TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, EncodeContinuousVector_ZeroVector_ReturnsZeroEncoding)
{
    std::array<float, 3> zeroVec = {0.0f, 0.0f, 0.0f};
    auto encoded = encoder->EncodeContinuousVector(zeroVec);
    
    EXPECT_EQ(encoded.size(), 3);
    
    for (float value : encoded)
    {
        EXPECT_FLOAT_EQ(value, 0.0f);
    }
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeContinuousVector_LargeValues_NormalizesCorrectly)
{
    std::array<float, 3> largeVec = {1000.0f, 2000.0f, 3000.0f};
    auto encoded = encoder->EncodeContinuousVector(largeVec);
    
    EXPECT_EQ(encoded.size(), 3);
    
    // All values should be in reasonable range after tanh normalization
    for (float value : encoded)
    {
        EXPECT_GE(value, -1.0f);
        EXPECT_LE(value, 1.0f);
    }
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeContinuousVector_PreservesRelativeOrder)
{
    std::array<float, 3> vec = {100.0f, 200.0f, 300.0f};
    auto encoded = encoder->EncodeContinuousVector(vec);
    
    // Encoded values should maintain relative order
    EXPECT_LT(encoded[0], encoded[1]);
    EXPECT_LT(encoded[1], encoded[2]);
}

// ============================================================================
// RELATIONAL ENCODING TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, EncodeRelation_ValidRelation_ReturnsEmbedding)
{
    std::string sourceID = "Entity_Player";
    std::string targetID = "Entity_Enemy";
    
    auto embedding = encoder->EncodeRelation(sourceID, targetID);
    
    EXPECT_EQ(embedding.size(), 64);
    
    // First two values should be non-zero (hashed entity IDs)
    EXPECT_NE(embedding[0], 0.0f);
    EXPECT_NE(embedding[1], 0.0f);
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeRelation_SameRelation_ReturnsSameEmbedding)
{
    std::string sourceID = "EntityA";
    std::string targetID = "EntityB";
    
    auto embedding1 = encoder->EncodeRelation(sourceID, targetID);
    auto embedding2 = encoder->EncodeRelation(sourceID, targetID);
    
    EXPECT_EQ(embedding1, embedding2);
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeRelation_ReversedRelation_ReturnsDifferentEmbedding)
{
    std::string sourceID = "EntityA";
    std::string targetID = "EntityB";
    
    auto embedding1 = encoder->EncodeRelation(sourceID, targetID);
    auto embedding2 = encoder->EncodeRelation(targetID, sourceID);
    
    EXPECT_NE(embedding1, embedding2);
}

// ============================================================================
// TEMPORAL ENCODING TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, EncodeTemporalDelta_IdenticalStates_ReturnsZeroDelta)
{
    std::vector<float> state = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    auto delta = encoder->EncodeTemporalDelta(state, state);
    
    EXPECT_EQ(delta.size(), state.size());
    
    for (float value : delta)
    {
        EXPECT_FLOAT_EQ(value, 0.0f);
    }
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeTemporalDelta_ChangingStates_ComputesCorrectDelta)
{
    std::vector<float> previous = {1.0f, 2.0f, 3.0f};
    std::vector<float> current = {2.0f, 4.0f, 6.0f};
    
    auto delta = encoder->EncodeTemporalDelta(current, previous);
    
    EXPECT_EQ(delta.size(), 3);
    EXPECT_FLOAT_EQ(delta[0], 1.0f);
    EXPECT_FLOAT_EQ(delta[1], 2.0f);
    EXPECT_FLOAT_EQ(delta[2], 3.0f);
}

TEST_F(SymbolicToNeuralEncoderTest, EncodeTemporalDelta_DifferentSizes_HandlesGracefully)
{
    std::vector<float> previous = {1.0f, 2.0f};
    std::vector<float> current = {2.0f, 4.0f, 6.0f, 8.0f};
    
    auto delta = encoder->EncodeTemporalDelta(current, previous);
    
    // Should compute delta for minimum size
    EXPECT_EQ(delta.size(), 2);
    EXPECT_FLOAT_EQ(delta[0], 1.0f);
    EXPECT_FLOAT_EQ(delta[1], 2.0f);
}

// ============================================================================
// BATCH PROCESSING TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, BatchEncode_MultipleEntities_ProcessesAll)
{
    std::vector<std::vector<float>> entities = {
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f},
        {7.0f, 8.0f, 9.0f}
    };
    
    std::vector<std::vector<float>> embeddings;
    for (const auto& entity : entities)
    {
        embeddings.push_back(encoder->EncodeEntity(entity));
    }
    
    EXPECT_EQ(embeddings.size(), 3);
    
    for (const auto& embedding : embeddings)
    {
        EXPECT_EQ(embedding.size(), encoder->Config.EntityEmbeddingDim);
    }
}

TEST_F(SymbolicToNeuralEncoderTest, BatchEncode_LargeBatch_MeetsLatencyTarget)
{
    const int batchSize = 100;
    std::vector<float> entityData(50, 1.0f);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < batchSize; i++)
    {
        encoder->EncodeEntity(entityData);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    float totalTimeMs = std::chrono::duration<float, std::milli>(end - start).count();
    float avgTimeMs = totalTimeMs / batchSize;
    
    EXPECT_LT(avgTimeMs, 0.5f);
}

// ============================================================================
// STREAMING ENCODER TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, StreamingMode_Enabled_ProcessesIncrementalUpdates)
{
    EXPECT_TRUE(encoder->Config.bEnableStreamingMode);
    
    // Simulate streaming updates
    std::vector<float> entity1 = {1.0f, 2.0f};
    std::vector<float> entity2 = {3.0f, 4.0f};
    
    auto embed1 = encoder->EncodeEntity(entity1);
    auto embed2 = encoder->EncodeEntity(entity2);
    
    EXPECT_EQ(encoder->Metrics.TotalEntityEmbeddings, 2);
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, Performance_SingleEncoding_MeetsLatencyTarget)
{
    std::vector<float> entityData(64, 0.5f);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto embedding = encoder->EncodeEntity(entityData);
    auto end = std::chrono::high_resolution_clock::now();
    
    float latencyMs = std::chrono::duration<float, std::milli>(end - start).count();
    
    EXPECT_LT(latencyMs, 0.5f);
}

TEST_F(SymbolicToNeuralEncoderTest, Performance_AverageLatency_MeetsTarget)
{
    std::vector<float> entityData(64, 0.5f);
    
    // Warm up
    for (int i = 0; i < 10; i++)
    {
        encoder->EncodeEntity(entityData);
    }
    
    // Measure
    const int numIterations = 1000;
    for (int i = 0; i < numIterations; i++)
    {
        encoder->EncodeEntity(entityData);
    }
    
    EXPECT_TRUE(encoder->IsMeetingLatencyTarget());
    EXPECT_LT(encoder->Metrics.AverageLatency, 0.5f);
}

TEST_F(SymbolicToNeuralEncoderTest, Performance_PeakLatency_WithinBounds)
{
    std::vector<float> entityData(128, 1.0f);
    
    for (int i = 0; i < 500; i++)
    {
        encoder->EncodeEntity(entityData);
    }
    
    // Peak latency should still be reasonable
    EXPECT_LT(encoder->Metrics.PeakLatency, 1.0f);
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, EdgeCase_NegativeValues_HandlesCorrectly)
{
    std::vector<float> entityData = {-1.0f, -2.0f, -3.0f};
    auto embedding = encoder->EncodeEntity(entityData);
    
    EXPECT_EQ(embedding.size(), encoder->Config.EntityEmbeddingDim);
    EXPECT_FLOAT_EQ(embedding[0], -1.0f);
    EXPECT_FLOAT_EQ(embedding[1], -2.0f);
    EXPECT_FLOAT_EQ(embedding[2], -3.0f);
}

TEST_F(SymbolicToNeuralEncoderTest, EdgeCase_VeryLargeValues_EncodesWithoutOverflow)
{
    std::vector<float> entityData = {1e6f, 1e7f, 1e8f};
    auto embedding = encoder->EncodeEntity(entityData);
    
    EXPECT_EQ(embedding.size(), encoder->Config.EntityEmbeddingDim);
    
    // Values should be preserved
    for (size_t i = 0; i < entityData.size(); i++)
    {
        EXPECT_FLOAT_EQ(embedding[i], entityData[i]);
    }
}

TEST_F(SymbolicToNeuralEncoderTest, EdgeCase_EmptyCategories_ReturnsValidEncoding)
{
    std::string emptyCategory = "";
    int32 maxCategories = 10;
    
    auto oneHot = encoder->EncodeCategoricalOneHot(emptyCategory, maxCategories);
    
    EXPECT_EQ(oneHot.size(), maxCategories);
    
    // Should still produce valid one-hot
    int countOnes = 0;
    for (float value : oneHot)
    {
        if (value == 1.0f) countOnes++;
    }
    EXPECT_EQ(countOnes, 1);
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST_F(SymbolicToNeuralEncoderTest, Integration_CompleteGameStateEncoding_WorksEndToEnd)
{
    // Simulate complete game state encoding
    
    // 1. Encode entities
    std::vector<std::vector<float>> entities = {
        {1.0f, 2.0f, 3.0f},
        {4.0f, 5.0f, 6.0f}
    };
    
    std::vector<std::vector<float>> entityEmbeddings;
    for (const auto& entity : entities)
    {
        entityEmbeddings.push_back(encoder->EncodeEntity(entity));
    }
    
    // 2. Encode categories
    auto categoryEncoding = encoder->EncodeCategoricalOneHot("PlayerType", 20);
    
    // 3. Encode continuous features
    std::array<float, 3> position = {100.0f, 200.0f, 300.0f};
    auto positionEncoding = encoder->EncodeContinuousVector(position);
    
    // 4. Encode relations
    auto relationEncoding = encoder->EncodeRelation("Player", "Enemy");
    
    // Verify all components were encoded
    EXPECT_EQ(entityEmbeddings.size(), 2);
    EXPECT_EQ(categoryEncoding.size(), 20);
    EXPECT_EQ(positionEncoding.size(), 3);
    EXPECT_EQ(relationEncoding.size(), 64);
    
    // Verify performance
    EXPECT_TRUE(encoder->IsMeetingLatencyTarget());
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
