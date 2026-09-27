// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/WorldPartitionRuntimeCellTransformer.h"
#if WITH_EDITOR
#include "Templates/SubclassOf.h"
#include "HAL/IConsoleManager.h"
#endif 
#include "FastGeoWorldPartitionRuntimeCellTransformer.generated.h"

class UActorComponent;

enum class EFastGeoTransform : uint32
{
	Allow,		//!< Actor or component can be transformed.
	Reject,		//!< Actor or component can't be transformed.
	Discard,	//!< Actor or component is not relevant and can be fully discarded without impact on the game.
	MAX
};

inline constexpr uint32 EnumToIndex(EFastGeoTransform Value)
{
	return static_cast<uint32>(Value);
}

/** Used to conditionally report the error result when not a success. */
struct FFastGeoTransformResult
{
public:
	FFastGeoTransformResult(EFastGeoTransform InTransformResult, const TCHAR* FailureReason = nullptr);
	FFastGeoTransformResult(EFastGeoTransform InTransformResult, TFunctionRef<FString()> FailureReasonFunc);

	EFastGeoTransform GetResult() const { return TransformResult; }
	uint32 GetResultIndex() const { return EnumToIndex(TransformResult); }

	static bool ShouldReport;

private:
	EFastGeoTransform TransformResult;
};

UCLASS(Config = FastGeoStreaming, DefaultConfig)
class FASTGEOSTREAMING_API UFastGeoWorldPartitionRuntimeCellTransformer : public UWorldPartitionRuntimeCellTransformer
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.

#if WITH_EDITOR
public:
	//~ Begin UWorldPartitionRuntimeCellTransformer Interface.
	virtual void Transform(ULevel* InLevel) override final;
	//~ End UWorldPartitionRuntimeCellTransformer Interface.

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	virtual void PreEditChange(FProperty* InPropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	//~ End UObject Interface.

protected:
	/** Whether the actor can be processed by the transformer. */
	virtual bool IsActorTransformable(AActor* InActor, FString& OutReason) const { return true; }
	/** Whether the component can be processed by the transformer. */
	virtual bool IsComponentTransformable(UPrimitiveComponent* InComponent, FString& OutReason) const { return true; }
	/** Whether the fully transformed actor can be deleted by the transformer. */
	virtual bool IsFullyTransformedActorDeletable(AActor* InActor, FString& OutReason) const { return true; }

	bool IsBlueprintActorWithLogic(AActor* InActor) const;
	FFastGeoTransformResult CanTransformActor(AActor* InActor, bool& bOutIsActorFullyTransformable, TArray<UActorComponent*>& OutTransformableComponents) const;
	FFastGeoTransformResult IsAllowedActorClass(AActor* InActor) const;
	FFastGeoTransformResult CanTransformComponent(UPrimitiveComponent* InComponent) const;
	FFastGeoTransformResult IsAllowedComponentClass(UActorComponent* InComponent) const;
	bool CanRemoveActor(AActor* InActor, TSet<UActorComponent*>& InIgnoredComponents) const;
	bool CanAlwaysIgnoreActor(AActor* InActor) const;
	void OnSelectionChanged(UObject* Object);
	bool IsDebugMode() const;

	struct FTransformableActor
	{
		int32 ActorIndex = 0;
		bool bIsActorFullyTransformable = false;
		TArray<UActorComponent*> TransformableComponents;
	};

	struct FTransformationStats
	{
		int32 TotalActorCount = 0;
		int32 TotalComponentCount = 0;
		int32 FullyTransformableActorCount = 0;
		int32 PartiallyTransformableActorCount = 0;
		int32 TransformedComponentCount = 0;

		void DumpStats(const TCHAR* InPrefixString);
	};

	TMap<AActor*, TArray<AActor*>> BuildActorsReferencesMap(const TArray<AActor*>& InActors);
	void GatherTransformableActors(const TArray<AActor*>& InActors, const ULevel* InLevel, TMap<AActor*, FTransformableActor>& OutTransformableActors, FTransformationStats& OutStats);
#endif

#if WITH_EDITORONLY_DATA
protected:
	/** When enabled, PIE will log the reason why actors/components are not transformable */
	UPROPERTY(EditAnywhere, Transient, SkipSerialization, Category = FastGeo)
	bool bDebugMode;

	/** When enabled, will log the reason why selected actors/components are not transformable. Works in both PIE and Editor. */
	UPROPERTY(EditAnywhere, Transient, SkipSerialization, Category = FastGeo)
	bool bDebugModeOnSelection;

	/** Allowed actor classes (recursive) to convert to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<AActor>> AllowedActorClasses;

	/** Allowed actor classes (exact) to convert to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<AActor>> AllowedExactActorClasses;

	/** Allowed component classes (recursive) to convert to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<UActorComponent>> AllowedComponentClasses;

	/** Allowed component classes (exact) to convert to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<UActorComponent>> AllowedExactComponentClasses;

	/** Disallowed actor classes (recursive) to convert to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<AActor>> DisallowedActorClasses;

	/** Disallowed actor classes (exact) to convert to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<AActor>> DisallowedExactActorClasses;

	/** Disallowed component classes (recursive) to convert to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<UActorComponent>> DisallowedComponentClasses;

	/** Disallowed component classes (exact) to convert to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<UActorComponent>> DisallowedExactComponentClasses;

	/** Component classes (recursive) to ignore when deciding to destroy a converted actor to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<UActorComponent>> IgnoredRemainingComponentClasses;

	/** Component classes (exact) to ignore when deciding to destroy a converted actor to FastGeo */
	UPROPERTY(EditAnywhere, Category = FastGeo)
	TArray<TSubclassOf<UActorComponent>> IgnoredRemainingExactComponentClasses;
#endif

#if WITH_EDITOR
	static bool IsDebugModeEnabled;
	static bool IsFastGeoEnabled;
	static FAutoConsoleVariableRef CVarIsDebugModeEnabled;
	static FAutoConsoleVariableRef CVarIsFastGeoEnabled;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(Config)
	TArray<TSubclassOf<AActor>> BuiltinAllowedActorClasses;

	UPROPERTY(Config)
	TArray<TSubclassOf<AActor>> BuiltinDisallowedActorClasses;

	UPROPERTY(Config)
	TArray<TSubclassOf<UActorComponent>> BuiltinAllowedComponentClasses;

	UPROPERTY(Config)
	TArray<TSubclassOf<UActorComponent>> BuiltinDisallowedComponentClasses;

	UPROPERTY(Config)
	TArray<TSubclassOf<UActorComponent>> BuiltinIgnoredRemainingComponentClasses;

	UPROPERTY(Config)
	TArray<TSubclassOf<UActorComponent>> BuiltinIgnoredRemainingExactComponentClasses;
#endif
};