// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Comprehensive Automated Test Framework

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "DeepTreeEchoTestFramework.generated.h"

class ADeepTreeEchoCharacter;
class UDeepTreeEchoInteractionComponent;
class UDeepTreeEchoCosmeticsComponent;
class UAvatarSwitchingSystem;
class UAnimationMontageSystem;

/**
 * ETestCategory
 * 
 * Categories of tests for organization.
 */
UENUM(BlueprintType)
enum class ETestCategory : uint8
{
	Unit,           // Unit tests for individual components
	Integration,    // Integration tests for component interactions
	Performance,    // Performance benchmarks
	Visual,         // Visual regression tests
	Stress,         // Stress tests for stability
	Functional      // End-to-end functional tests
};

/**
 * FTestResult
 * 
 * Result of a single test.
 */
USTRUCT(BlueprintType)
struct FTestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FName TestName;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	ETestCategory Category;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	bool bPassed;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	float ExecutionTime;

	UPROPERTY(BlueprintReadOnly, Category = "Test")
	FDateTime Timestamp;

	FTestResult()
		: Category(ETestCategory::Unit)
		, bPassed(false)
		, ExecutionTime(0.0f)
	{}
};

/**
 * FPerformanceMetrics
 * 
 * Performance metrics collected during tests.
 */
USTRUCT(BlueprintType)
struct FPerformanceMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Performance")
	float AverageFrameTime;

	UPROPERTY(BlueprintReadOnly, Category = "Performance")
	float MinFrameTime;

	UPROPERTY(BlueprintReadOnly, Category = "Performance")
	float MaxFrameTime;

	UPROPERTY(BlueprintReadOnly, Category = "Performance")
	float PercentileFrameTime99;

	UPROPERTY(BlueprintReadOnly, Category = "Performance")
	int64 MemoryUsageBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Performance")
	int32 DrawCalls;

	UPROPERTY(BlueprintReadOnly, Category = "Performance")
	int32 TriangleCount;

	FPerformanceMetrics()
		: AverageFrameTime(0.0f)
		, MinFrameTime(FLT_MAX)
		, MaxFrameTime(0.0f)
		, PercentileFrameTime99(0.0f)
		, MemoryUsageBytes(0)
		, DrawCalls(0)
		, TriangleCount(0)
	{}
};

/**
 * UDeepTreeEchoTestFramework
 *
 * Comprehensive test framework for the Deep Tree Echo avatar system.
 * Provides unit tests, integration tests, performance benchmarks,
 * and visual regression testing capabilities.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UDeepTreeEchoTestFramework : public UObject
{
	GENERATED_BODY()

public:
	UDeepTreeEchoTestFramework();

	// Run all tests
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void RunAllTests();

	// Run tests by category
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void RunTestsByCategory(ETestCategory Category);

	// Run specific test
	UFUNCTION(BlueprintCallable, Category = "Testing")
	FTestResult RunTest(FName TestName);

	// Get all test results
	UFUNCTION(BlueprintPure, Category = "Testing")
	TArray<FTestResult> GetTestResults() const { return TestResults; }

	// Get performance metrics
	UFUNCTION(BlueprintPure, Category = "Testing")
	FPerformanceMetrics GetPerformanceMetrics() const { return PerformanceMetrics; }

	// Clear results
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void ClearResults();

	// Export results to JSON
	UFUNCTION(BlueprintCallable, Category = "Testing")
	FString ExportResultsToJson() const;

	// Set test character
	UFUNCTION(BlueprintCallable, Category = "Testing")
	void SetTestCharacter(ADeepTreeEchoCharacter* Character);

protected:
	// Test character reference
	UPROPERTY()
	TObjectPtr<ADeepTreeEchoCharacter> TestCharacter;

	// Test results
	UPROPERTY()
	TArray<FTestResult> TestResults;

	// Performance metrics
	UPROPERTY()
	FPerformanceMetrics PerformanceMetrics;

	// Unit Tests
	FTestResult Test_InteractionComponent_Initialize();
	FTestResult Test_InteractionComponent_Scan();
	FTestResult Test_InteractionComponent_Interact();
	FTestResult Test_InteractionComponent_Memory();

	FTestResult Test_CosmeticsComponent_Initialize();
	FTestResult Test_CosmeticsComponent_OutfitChange();
	FTestResult Test_CosmeticsComponent_AccessoryAttach();
	FTestResult Test_CosmeticsComponent_EmotionalEffects();

	FTestResult Test_AvatarSwitching_Initialize();
	FTestResult Test_AvatarSwitching_ModeChange();
	FTestResult Test_AvatarSwitching_StateSync();
	FTestResult Test_AvatarSwitching_Transition();

	FTestResult Test_MontageSystem_Initialize();
	FTestResult Test_MontageSystem_PlayMontage();
	FTestResult Test_MontageSystem_VariantSelection();
	FTestResult Test_MontageSystem_Cooldowns();

	// Integration Tests
	FTestResult Test_Integration_InteractionToCosmetics();
	FTestResult Test_Integration_EmotionToAnimation();
	FTestResult Test_Integration_PersonalityToMontage();
	FTestResult Test_Integration_FullPipeline();

	// Performance Tests
	FTestResult Test_Performance_InteractionScan();
	FTestResult Test_Performance_MaterialUpdates();
	FTestResult Test_Performance_AnimationBlending();
	FTestResult Test_Performance_AvatarSwitching();

	// Stress Tests
	FTestResult Test_Stress_RapidInteractions();
	FTestResult Test_Stress_RapidModeSwitch();
	FTestResult Test_Stress_ManyAccessories();
	FTestResult Test_Stress_LongRunning();

	// Helper functions
	void RecordResult(const FTestResult& Result);
	void UpdatePerformanceMetrics(float FrameTime);
	FTestResult CreateTestResult(FName TestName, ETestCategory Category, bool bPassed, const FString& Message, float ExecutionTime);
};

//////////////////////////////////////////////////////////////////////////
// Automation Tests (Unreal's built-in test framework)

/**
 * FDeepTreeEchoInteractionTest
 * 
 * Automated tests for the interaction system.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTreeEchoInteractionTest, "DeepTreeEcho.Interaction.BasicFunctionality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * FDeepTreeEchoCosmeticsTest
 * 
 * Automated tests for the cosmetics system.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTreeEchoCosmeticsTest, "DeepTreeEcho.Cosmetics.BasicFunctionality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * FDeepTreeEchoAvatarSwitchingTest
 * 
 * Automated tests for the avatar switching system.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTreeEchoAvatarSwitchingTest, "DeepTreeEcho.AvatarSwitching.BasicFunctionality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * FDeepTreeEchoAnimationTest
 * 
 * Automated tests for the animation montage system.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTreeEchoAnimationTest, "DeepTreeEcho.Animation.BasicFunctionality", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

/**
 * FDeepTreeEchoPerformanceTest
 * 
 * Performance benchmark tests.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTreeEchoPerformanceTest, "DeepTreeEcho.Performance.Benchmarks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)

/**
 * FDeepTreeEchoIntegrationTest
 * 
 * Integration tests for system interactions.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTreeEchoIntegrationTest, "DeepTreeEcho.Integration.SystemInteractions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
