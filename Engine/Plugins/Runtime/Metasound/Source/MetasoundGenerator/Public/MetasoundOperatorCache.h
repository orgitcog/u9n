// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundGenerator.h"
#include "Misc/Guid.h"
#include "HAL/CriticalSection.h"
#include "Tasks/Pipe.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"

#define UE_API METASOUNDGENERATOR_API

#ifndef METASOUND_OPERATORCACHEPROFILER_ENABLED
#define METASOUND_OPERATORCACHEPROFILER_ENABLED COUNTERSTRACE_ENABLED
#endif
namespace Metasound
{
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
	namespace Engine
	{
		class FOperatorCacheStatTracker;
	}

	namespace OperatorPoolPrivate
	{
		class FWindowedHitRate
		{
		public:
			// ctor
			FWindowedHitRate();
			void Update();
			void AddHit();
			void AddMiss();
	
		private:
			struct IntermediateResult
			{
				uint32 NumHits = 0;
				uint32 Total = 0;
				float TTLSeconds;
			};
	
			TArray<IntermediateResult> History;
	
			uint32 CurrHitCount = 0;
			uint32 CurrTotal = 0;
			uint32 RunningHitCount = 0;
			uint32 RunningTotal = 0;
	
			float CurrTTLSeconds = 0.f;
	
			uint64 PreviousTimeCycles = 0;
			bool bIsFirstUpdate = true;
	
			void FirstUpdate();
			void SetWindowLength(const float InNewLengthSeconds);
			void ExpireResult(const IntermediateResult& InResultToExpire);
			void TickResults(const float DeltaTimeSeconds);
	
		}; // class FWindowedHitRate
	} // namespace OperatorPoolPrivate
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

	struct FOperatorPoolSettings
	{
		uint32 MaxNumOperators = 64;
	};


	// Data required to build an operator without immediately playing it
	struct FOperatorBuildData
	{
		FMetasoundGeneratorInitParams InitParams;
		Frontend::FGraphRegistryKey RegistryKey;
		FGuid AssetClassID;
		int32 NumInstances;

		// If true, touches existing assets and only builds remaining number if required
		bool bTouchExisting = false; 

		FOperatorBuildData() = delete;
		UE_API FOperatorBuildData(
			  FMetasoundGeneratorInitParams&& InInitParams
			, Frontend::FGraphRegistryKey InRegistryKey
			, FGuid InAssetID
			, int32 InNumInstances = 1
			, bool bInTouchExisting = false
		);

	}; // struct FOperatorPrecacheData

	// Provides additional debug context for the operator the pool is interacting with.
	struct FOperatorContext
	{
		FName GraphInstanceName;
		FStringView MetaSoundName;

		static UE_API FOperatorContext FromInitParams(const FMetasoundGeneratorInitParams& InParams);
	};

	// Pool of re-useable metasound operators to be used / put back by the metasound generator
	// operators can also be pre-constructed via the UMetasoundCacheSubsystem BP api.
	class FOperatorPool : public TSharedFromThis<FOperatorPool>
	{
	public:

		UE_API FOperatorPool(const FOperatorPoolSettings& InSettings);
		UE_API ~FOperatorPool();


		UE_DEPRECATED(5.5, "Use ClaimOperator(const FOperatorPoolEntryID&, ...) instead")
		UE_API FOperatorAndInputs ClaimOperator(const FGuid& InOperatorID);
		UE_API FOperatorAndInputs ClaimOperator(const FOperatorPoolEntryID& InOperatorID, const FOperatorContext& InContext);

		UE_DEPRECATED(5.5, "Use AddOperator(const FOperatorPoolEntryID&, ...) instead")
		UE_API void AddOperator(const FGuid& InOperatorID, TUniquePtr<IOperator>&& InOperator, FInputVertexInterfaceData&& InputData);
		UE_API void AddOperator(const FOperatorPoolEntryID& InOperatorID, TUniquePtr<IOperator>&& InOperator, FInputVertexInterfaceData&& InputData, TSharedPtr<FGraphRenderCost>&& InRenderCost = {});

		UE_DEPRECATED(5.5, "Use AddOperator(const FOperatorPoolEntryID&, ...) instead")
		UE_API void AddOperator(const FGuid& InOperatorID, FOperatorAndInputs&& OperatorAndInputs);
		UE_API void AddOperator(const FOperatorPoolEntryID& InOperatorID, FOperatorAndInputs&& OperatorAndInputs);

		UE_API void BuildAndAddOperator(TUniquePtr<FOperatorBuildData> InBuildData);

		UE_DEPRECATED(5.5, "Use TouchOperators(const FOperatorPoolEntryID&, ...) instead")
		UE_API void TouchOperators(const FGuid& InOperatorID, int32 NumToTouch = 1);
		UE_API void TouchOperators(const FOperatorPoolEntryID& InOperatorID, int32 NumToTouch = 1);
		UE_API void TouchOperatorsViaAssetClassID(const FGuid& InAssetClassID, int32 NumToTouch = 1);

		UE_API bool IsStopping() const;

		UE_DEPRECATED(5.5, "Use RemoveOperatorsWithID(const FOperatorPoolEntryID&) instead")
		UE_API void RemoveOperatorsWithID(const FGuid& InOperatorID);
		UE_API void RemoveOperatorsWithID(const FOperatorPoolEntryID& InOperatorID);
		UE_API void RemoveOperatorsWithAssetClassID(const FGuid& InAssetClassID);

		UE_DEPRECATED(5.5, "Use GetNumCachedOperatorsWithID(const FOperatorPoolEntryID&) instead")
		UE_API int32 GetNumCachedOperatorsWithID(const FGuid& InOperatorID) const;
		UE_API int32 GetNumCachedOperatorsWithID(const FOperatorPoolEntryID& InOperatorID) const;
		UE_API int32 GetNumCachedOperatorsWithAssetClassID(const FGuid& InAssetClassID) const;

		UE_DEPRECATED(5.5, "Adding id to look-up is now private implementation")
		void AddAssetIdToGraphIdLookUp(const FGuid& InAssetClassID, const FOperatorPoolEntryID& InOperatorID) { }

		UE_API void SetMaxNumOperators(uint32 InMaxNumOperators);
#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		UE_API void UpdateHitRateTracker();
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

		UE_DEPRECATED(5.5, "Use StopAsyncTasks")
		UE_API void CancelAllBuildEvents();

		UE_API void StopAsyncTasks();

		using FTaskId = int32;
		using FTaskFunction = TUniqueFunction<void(FOperatorPool::FTaskId, TWeakPtr<FOperatorPool>)>;

	private:
		FTaskId LastTaskId = 0;

		UE_API void AddAssetIdToGraphIdLookUpInternal(const FGuid& InAssetClassID, const FOperatorPoolEntryID& InOperatorID);
		UE_API void AddOperatorInternal(const FOperatorPoolEntryID& InOperatorID, FOperatorAndInputs&& OperatorAndInputs);
		UE_API bool ExecuteTaskAsync(FTaskFunction&& InFunction);
		UE_API void Trim();

		FOperatorPoolSettings Settings;
		mutable FCriticalSection CriticalSection;

#if METASOUND_OPERATORCACHEPROFILER_ENABLED
		OperatorPoolPrivate::FWindowedHitRate HitRateTracker;
		TUniquePtr<Engine::FOperatorCacheStatTracker> CacheStatTracker;
#endif // #if METASOUND_OPERATORCACHEPROFILER_ENABLED

		// Notifies active build tasks to abort as soon as possible
		// and gates additional build tasks from being added.
		std::atomic<bool> bStopping;

		TMap<FTaskId, UE::Tasks::FTask> ActiveBuildTasks;
		UE::Tasks::FPipe AsyncBuildPipe;

		TMap<FOperatorPoolEntryID, TArray<FOperatorAndInputs>> Operators;
		TMap<FGuid, FOperatorPoolEntryID> AssetIdToGraphIdLookUp;
		TMultiMap<FOperatorPoolEntryID, FGuid> GraphIdToAssetIdLookUp;
		TArray<FOperatorPoolEntryID> Stack;
	};
} // namespace Metasound




#undef UE_API
