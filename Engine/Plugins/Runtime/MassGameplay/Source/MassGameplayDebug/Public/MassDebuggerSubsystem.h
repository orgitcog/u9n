// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassGameplayDebugTypes.h"
#include "MassSubsystemBase.h"
#include "MassExternalSubsystemTraits.h"
#include "MassDebuggerSubsystem.generated.h"

#define UE_API MASSGAMEPLAYDEBUG_API


class UMassDebugVisualizationComponent;
class AMassDebugVisualizer;

UCLASS(MinimalAPI)
class UMassDebuggerSubsystem : public UMassSubsystemBase
{
	GENERATED_BODY()
public:
	struct FShapeDesc
	{
		FVector Location = {}; // no init on purpose, value will come from constructor
		float Size = {};
		FShapeDesc(const FVector InLocation, const float InSize) : Location(InLocation), Size(InSize) {}
	};

	// Methods to optimize the collection of data to only when category is enabled
	bool IsCollectingData() const { return bCollectingData; }
	void SetCollectingData() { bCollectingData = true; }
	void DataCollected() { bCollectingData = false; }

	void AddShape(EMassEntityDebugShape Shape, FVector Location, float Size) { Shapes[uint8(Shape)].Add(FShapeDesc(Location, Size)); }
	const TArray<FShapeDesc>* GetShapes() const { return Shapes; }
	UE_API void ResetDebugShapes();

	FMassEntityHandle GetSelectedEntity() const { return SelectedEntity; }
	UE_API void SetSelectedEntity(const FMassEntityHandle InSelectedEntity);

	UE_API void AppendSelectedEntityInfo(const FString& Info);
	const FString& GetSelectedEntityInfo() const { return SelectedEntityDetails; }
	
	/** Fetches the UMassDebugVisualizationComponent owned by lazily created DebugVisualizer */
	UE_API UMassDebugVisualizationComponent* GetVisualizationComponent();

#if WITH_EDITORONLY_DATA
	UE_API AMassDebugVisualizer& GetOrSpawnDebugVisualizer(UWorld& InWorld);
#endif

protected:
	// USubsystem BEGIN
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	// USubsystem END
	
	UE_API void OnProcessingPhaseStarted(const float DeltaSeconds);
	UE_API void PreTickProcessors();
	UE_API void OnEntitySelected(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle);

protected:
	bool bCollectingData = false;

	TArray<FShapeDesc> Shapes[uint8(EMassEntityDebugShape::MAX)];
	TArray<FMassEntityHandle> Entities;
	TArray<FVector> Locations;
	FMassEntityHandle SelectedEntity;
	FString SelectedEntityDetails;

	UPROPERTY(Transient)
	TObjectPtr<UMassDebugVisualizationComponent> VisualizationComponent;

	UPROPERTY(Transient)
	TObjectPtr<AMassDebugVisualizer> DebugVisualizer;

	FDelegateHandle OnEntitySelectedHandle;
};

template<>
struct TMassExternalSubsystemTraits<UMassDebuggerSubsystem> final
{
	enum
	{
		GameThreadOnly = false,
		ThreadSafeWrite = false,
	};
};

#undef UE_API
