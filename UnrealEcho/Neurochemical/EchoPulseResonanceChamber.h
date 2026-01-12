#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EchoPulseResonanceChamber.generated.h"

/**
 * Echo Pulse Resonance Chamber
 *
 * Implements resonance dynamics for the Deep Tree Echo cognitive system.
 * Models a damped harmonic oscillator with feedback to simulate echo
 * propagation patterns aligned with the 12-step cognitive cycle.
 *
 * Key features:
 * - Resonance decay with configurable damping
 * - Frequency response based on cognitive step phase
 * - Feedback amplification for pattern reinforcement
 * - Integration with triadic consciousness streams
 */
UCLASS(BlueprintType)
class UEchoPulseResonanceChamber : public UObject
{
	GENERATED_BODY()

public:
	UEchoPulseResonanceChamber();

	// ========================================
	// PRIMARY INTERFACE
	// ========================================

	/** Update resonance with input signal (basic interface) */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	void UpdateResonance(float Signal);

	/** Update resonance with cognitive step alignment */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	void UpdateResonanceWithStep(float Signal, int32 CognitiveStep);

	/** Process resonance decay over time */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	void ProcessDecay(float DeltaTime);

	/** Reset all resonance state */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	void Reset();

	// ========================================
	// RESONANCE QUERIES
	// ========================================

	/** Get current resonance level (0-100) */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	float GetResonanceLevel() const { return ResonanceLevel; }

	/** Get normalized resonance (0-1) */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	float GetNormalizedResonance() const { return ResonanceLevel / 100.0f; }

	/** Check if resonance is in high state */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	bool IsResonating() const { return ResonanceLevel >= ResonanceThreshold; }

	/** Get resonance velocity (rate of change) */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	float GetResonanceVelocity() const { return ResonanceVelocity; }

	/** Get dominant frequency component */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Resonance")
	float GetDominantFrequency() const { return DominantFrequency; }

	// ========================================
	// FEEDBACK SYSTEM
	// ========================================

	/** Apply feedback amplification */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Feedback")
	void ApplyFeedback(float FeedbackAmount);

	/** Set feedback gain */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Feedback")
	void SetFeedbackGain(float Gain);

	/** Enable/disable feedback loop */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Feedback")
	void SetFeedbackEnabled(bool bEnabled);

	// ========================================
	// TRIADIC SYNC
	// ========================================

	/** Sync resonance with consciousness stream */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Sync")
	void SyncWithStream(int32 StreamIndex, float StreamActivation);

	/** Get phase alignment with cognitive cycle */
	UFUNCTION(BlueprintCallable, Category = "Neurochemical|Sync")
	float GetPhaseAlignment() const { return PhaseAlignment; }

	// ========================================
	// PROPERTIES
	// ========================================

	/** Current resonance level (0-100) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical|State")
	float ResonanceLevel;

	/** Resonance velocity (rate of change) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical|State")
	float ResonanceVelocity;

	/** Dominant frequency component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical|State")
	float DominantFrequency;

	/** Phase alignment with cognitive cycle (0-1) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neurochemical|State")
	float PhaseAlignment;

	/** Input signal intensity multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical|Config")
	float IntensityFactor;

	/** Damping coefficient (how quickly resonance decays) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DampingCoefficient;

	/** Natural frequency of the resonance system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical|Config", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float NaturalFrequency;

	/** Feedback gain multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical|Config", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FeedbackGain;

	/** Threshold for resonance detection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical|Config")
	float ResonanceThreshold;

	/** Enable feedback loop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neurochemical|Config")
	bool bFeedbackEnabled;

	// ========================================
	// DELEGATES
	// ========================================

	/** Fired when resonance crosses threshold (entering high state) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResonanceTriggered, float, ResonanceLevel);
	UPROPERTY(BlueprintAssignable, Category = "Neurochemical|Events")
	FOnResonanceTriggered OnResonanceTriggered;

	/** Fired when resonance drops below threshold */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResonanceFaded);
	UPROPERTY(BlueprintAssignable, Category = "Neurochemical|Events")
	FOnResonanceFaded OnResonanceFaded;

private:
	/** Previous resonance level for velocity calculation */
	float PreviousResonanceLevel;

	/** Accumulated phase for oscillation */
	float AccumulatedPhase;

	/** Signal history for frequency analysis */
	TArray<float> SignalHistory;

	/** Was resonating in previous update */
	bool bWasResonating;

	/** Calculate frequency from signal history */
	float CalculateDominantFrequency() const;

	/** Get phase multiplier for cognitive step */
	float GetStepPhaseMultiplier(int32 CognitiveStep) const;
};
