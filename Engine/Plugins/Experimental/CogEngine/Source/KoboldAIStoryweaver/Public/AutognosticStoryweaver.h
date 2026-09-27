// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "StoryArc.generated.h"

/**
 * Story Arc - Represents a player-specific character arc
 */
USTRUCT(BlueprintType)
struct FStoryArc
{
	GENERATED_BODY()

	/** Unique arc identifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Storyweaver")
	FString ArcID;

	/** Player identifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Storyweaver")
	FString PlayerID;

	/** Current arc phase */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Storyweaver")
	FString CurrentPhase;

	/** Arc milestones */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Storyweaver")
	TArray<FString> Milestones;

	/** Arc coherence score (0-1) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Storyweaver")
	float CoherenceScore;

	FStoryArc()
		: CoherenceScore(1.0f)
	{
	}
};

/**
 * Autognostic Storyweaver - Maintains narrative coherence across massive player base
 */
UCLASS(Blueprintable, BlueprintType)
class KOBOLDAISTORYWEAVER_API UAutognosticStoryweaver : public UObject
{
	GENERATED_BODY()

public:
	UAutognosticStoryweaver();

	/** All active story arcs */
	UPROPERTY(BlueprintReadOnly, Category = "Storyweaver")
	TMap<FString, FStoryArc> ActiveArcs;

	/** World-building core state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Storyweaver")
	FString WorldState;

	/** Maximum number of concurrent arcs */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Storyweaver")
	int64 MaxConcurrentArcs;

	/** Create new story arc for player */
	UFUNCTION(BlueprintCallable, Category = "Storyweaver")
	bool CreateStoryArc(const FString& PlayerID, const FString& ArcID);

	/** Update story arc progression */
	UFUNCTION(BlueprintCallable, Category = "Storyweaver")
	void UpdateArcProgression(const FString& ArcID, const FString& NewPhase);

	/** Align arc with world-building core */
	UFUNCTION(BlueprintCallable, Category = "Storyweaver")
	float AlignArcWithWorldCore(const FString& ArcID);

	/** Check coherence across all arcs */
	UFUNCTION(BlueprintCallable, Category = "Storyweaver")
	void MaintainGlobalCoherence();

	/** Initialize storyweaver */
	UFUNCTION(BlueprintCallable, Category = "Storyweaver")
	void Initialize();

protected:
	/** Calculate coherence score for arc */
	float CalculateCoherence(const FStoryArc& Arc);

	/** Resolve arc conflicts */
	void ResolveArcConflicts();
};
