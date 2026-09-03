// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MassEntityHandle.h"
#include "MassEntityTypes.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassExternalSubsystemTraits.h"
#include "MassEntityQuery.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassArchetypeTypes.h"
#include "MassSubsystemAccess.h"
#include "MassProcessor.h"


#define CHECK_IF_VALID(View, Type) \
	checkf(View \
		, TEXT("Requested fragment type not bound, type %s. Make sure it has been listed as required."), *GetNameSafe(Type))

#define CHECK_IF_READWRITE(View) \
	checkf(View == nullptr || View->Requirement.AccessMode == EMassFragmentAccess::ReadWrite \
		, TEXT("Requested fragment type not bound for writing, type %s. Make sure it has been listed as required in ReadWrite mode.") \
		, View ? *GetNameSafe(View->Requirement.StructType) : TEXT("[Not found]"))

struct FMassEntityQuery;

struct FMassExecutionContext
{
private:

	template< typename ViewType >
	struct TFragmentView 
	{
		FMassFragmentRequirementDescription Requirement;
		ViewType FragmentView;

		TFragmentView() {}
		explicit TFragmentView(const FMassFragmentRequirementDescription& InRequirement) : Requirement(InRequirement) {}

		bool operator==(const UScriptStruct* FragmentType) const { return Requirement.StructType == FragmentType; }
	};
	using FFragmentView = TFragmentView<TArrayView<FMassFragment>>;
	TArray<FFragmentView, TInlineAllocator<8>> FragmentViews;

	using FChunkFragmentView = TFragmentView<FStructView>;
	TArray<FChunkFragmentView, TInlineAllocator<4>> ChunkFragmentViews;

	using FConstSharedFragmentView = TFragmentView<FConstStructView>;
	TArray<FConstSharedFragmentView, TInlineAllocator<4>> ConstSharedFragmentViews;

	using FSharedFragmentView = TFragmentView<FStructView>;
	TArray<FSharedFragmentView, TInlineAllocator<4>> SharedFragmentViews;

	FMassSubsystemAccess SubsystemAccess;
	
	// mz@todo make this shared ptr thread-safe and never auto-flush in MT environment. 
	TSharedPtr<FMassCommandBuffer> DeferredCommandBuffer;
	TArrayView<FMassEntityHandle> EntityListView;
	
	/** If set this indicates the exact archetype and its chunks to be processed. 
	 *  @todo this data should live somewhere else, preferably be just a parameter to Query.ForEachEntityChunk function */
	FMassArchetypeEntityCollection EntityCollection;
	
	/** @todo rename to "payload" */
	FInstancedStruct AuxData;
	float DeltaTimeSeconds = 0.0f;
	int32 ChunkSerialModificationNumber = -1;
	FMassArchetypeCompositionDescriptor CurrentArchetypeCompositionDescriptor;
#if WITH_MASSENTITY_DEBUG
	FColor DebugColor;
#endif // WITH_MASSENTITY_DEBUG

	TSharedRef<FMassEntityManager> EntityManager;

	struct FQueryTransientRuntime
	{
		TNotNull<FMassEntityQuery*> Query;
		FMassExternalSubsystemBitSet ConstSubsystemsBitSet;
		FMassExternalSubsystemBitSet MutableSubsystemsBitSet;
#if WITH_MASSENTITY_DEBUG
		/** MaxBreakFragmentCount needs to be bigger than the greatest number of fragments a query has a write requirement for to that can have a breakpoint set */
		static constexpr uint32 MaxFragmentBreakpointCount = 8;
		TStaticArray<const UScriptStruct*, MaxFragmentBreakpointCount> FragmentTypesToBreakOn;

		bool bCheckProcessorBreaks = false;
		int32 BreakFragmentsCount = 0;
#endif // WITH_MASSENTITY_DEBUG

		/** Serial number to ensure iterator consistency (subsequent calls to CreateEntityIterator should not pass equivelncy test) */
		uint32 IteratorSerialNumber = 0;

		/** Helper function to create an empty instance with a valid Query ptr */
		static FQueryTransientRuntime& GetDummyInstance();
	};

	/** We usually expect the queries to go only a single layer deep, so 2 elements here should suffice most of the time */
	TArray<FQueryTransientRuntime, TInlineAllocator<2>> QueriesStack;

	/** Track the serial number for FEntityIterator creation */
	uint32 IteratorSerialNumberGenerator = 0;

#if WITH_MASSENTITY_DEBUG
	FString DebugExecutionDescription;

	/** Currently executing processor, used for debugger breakpoint checking */
	// js@todo make this more generic
	TWeakObjectPtr<UMassProcessor> DebugProcessor;
#endif // WITH_MASSENTITY_DEBUG
	
	/** Used to control when the context is allowed to flush commands collected in DeferredCommandBuffer. This mechanism 
	 * is mainly utilized to avoid numerous small flushes in favor of fewer larger ones. */
	bool bFlushDeferredCommands = true;

	TArrayView<FFragmentView> GetMutableRequirements() { return FragmentViews; }
	TArrayView<FChunkFragmentView> GetMutableChunkRequirements() { return ChunkFragmentViews; }
	TArrayView<FConstSharedFragmentView> GetMutableConstSharedRequirements() { return ConstSharedFragmentViews; }
	TArrayView<FSharedFragmentView> GetMutableSharedRequirements() { return SharedFragmentViews; }

	void GetSubsystemRequirementBits(FMassExternalSubsystemBitSet& OutConstSubsystemsBitSet, FMassExternalSubsystemBitSet& OutMutableSubsystemsBitSet)
	{
		SubsystemAccess.GetSubsystemRequirementBits(OutConstSubsystemsBitSet, OutMutableSubsystemsBitSet);
	}

	void SetSubsystemRequirementBits(const FMassExternalSubsystemBitSet& InConstSubsystemsBitSet, const FMassExternalSubsystemBitSet& InMutableSubsystemsBitSet)
	{
		SubsystemAccess.SetSubsystemRequirementBits(InConstSubsystemsBitSet, InMutableSubsystemsBitSet);
	}

	EMassExecutionContextType ExecutionType = EMassExecutionContextType::Local;

	friend FMassArchetypeData;

	/**
	 * Note that this operator is private on purpose, used to simplify the implementation of constructors.
	 * The context itself does not support assignment.
	 */
	FMassExecutionContext& operator=(const FMassExecutionContext& Other) = default;

public:
	MASSENTITY_API explicit FMassExecutionContext(FMassEntityManager& InEntityManager, const float InDeltaTimeSeconds = 0.f, const bool bInFlushDeferredCommands = true);
	FMassExecutionContext(FMassExecutionContext&& Other) = default;
	MASSENTITY_API FMassExecutionContext(const FMassExecutionContext& Other);
	MASSENTITY_API FMassExecutionContext(const FMassExecutionContext& Other, FMassEntityQuery& Query, const TSharedPtr<FMassCommandBuffer>& InCommandBuffer = {});
	MASSENTITY_API ~FMassExecutionContext();

	/** For internal use only, should never be exported as part of API */
	MASSENTITY_API static FMassExecutionContext& GetDummyInstance();

	FMassEntityManager& GetEntityManagerChecked() const;
	const TSharedRef<FMassEntityManager>& GetSharedEntityManager();

#if WITH_MASSENTITY_DEBUG
	const FString& DebugGetExecutionDesc() const { return DebugExecutionDescription; }
	void DebugSetExecutionDesc(const FString& Description) { DebugExecutionDescription = Description; }
	void DebugSetExecutionDesc(FString&& Description) { DebugExecutionDescription = MoveTemp(Description); }

	UMassProcessor* DebugGetProcessor() const { return DebugProcessor.Get(); }
	void DebugSetProcessor(UMassProcessor* Processor) { DebugProcessor = Processor; }
#endif

	void PushQuery(FMassEntityQuery& InQuery);
	void PopQuery(const FMassEntityQuery& InQuery);
	const FMassEntityQuery& GetCurrentQuery() const;
	bool IsCurrentQuery(const FMassEntityQuery& Query) const;
	void ApplyFragmentRequirements(const FMassEntityQuery& RequestingQuery);
	void ClearFragmentViews(const FMassEntityQuery& RequestingQuery);

	/**
	 * Iterator to easily loop through entities in the current chunk.
	 * Supports ranged for and can be used directly as an entity index for the current chunk.
	 */
	struct FEntityIterator
	{
		FORCEINLINE int32 operator*() const
		{
			return EntityIndex;
		}

		FORCEINLINE bool operator!=(const int& Other) const
		{
			return EntityIndex != Other;
		}

		FORCEINLINE operator int32() const
		{
			return EntityIndex;
		}

		FORCEINLINE operator bool() const
		{
			return SerialNumber && EntityIndex < NumEntities;
		}

		FORCEINLINE bool operator<(const int32 Other) const
		{
			return SerialNumber && EntityIndex != INDEX_NONE && EntityIndex < Other;
		}

		FORCEINLINE FEntityIterator& operator++()
		{
			++EntityIndex;
#if WITH_MASSENTITY_DEBUG
			if (UNLIKELY(QueryRuntime.bCheckProcessorBreaks || QueryRuntime.BreakFragmentsCount != 0) 
				&& EntityIndex < NumEntities)
			{
				TestBreakpoints();
			}
#endif //WITH_MASSENTITY_DEBUG
			return *this;
		}

		FORCEINLINE void operator++(int)
		{
			++(*this);
		}

		FEntityIterator&& begin()
		{
			return MoveTemp(*this);
		}

		FEntityIterator end() const
		{
			FEntityIterator End;
			End.EntityIndex = NumEntities;
			return End;
		}

		MASSENTITY_API FEntityIterator();
		FEntityIterator(FEntityIterator&&) = default;

		FEntityIterator& operator=(const FEntityIterator&) = delete;
		FEntityIterator& operator=(FEntityIterator&&) = delete;
		/**
		 * Iterator copying is disabled to avoid additional checks to detect if entity chunk being iterated on changed.
		 * This decision is to be reconsidered when valid iterator-copying scenarios emerge. 
		 */
		FEntityIterator(const FEntityIterator&) = delete;

	private:
		friend FMassExecutionContext;
		FEntityIterator(FMassExecutionContext& InExecutionContext);
		FEntityIterator(FMassExecutionContext& InExecutionContext, FQueryTransientRuntime& InQueryRuntime);

#if WITH_MASSENTITY_DEBUG
		MASSENTITY_API void TestBreakpoints();
#endif //!WITH_MASSENTITY_DEBUG

		const FMassExecutionContext& ExecutionContext;
		const FQueryTransientRuntime& QueryRuntime;
		int32 EntityIndex = INDEX_NONE;
		const int32 NumEntities = INDEX_NONE;
		const uint32 SerialNumber = 0;
	};

	/**
	 * Creates an Entity Iterator for the current chunk.
	 * Supports range-based for loop and can be used directly as an entity index for the current chunk.
	 */
	MASSENTITY_API FEntityIterator CreateEntityIterator();

	/** Sets bFlushDeferredCommands. Note that setting to True while the system is being executed doesn't result in
	 *  immediate commands flushing */
	void SetFlushDeferredCommands(const bool bNewFlushDeferredCommands);
	void SetDeferredCommandBuffer(const TSharedPtr<FMassCommandBuffer>& InDeferredCommandBuffer);
	MASSENTITY_API void SetEntityCollection(const FMassArchetypeEntityCollection& InEntityCollection);
	MASSENTITY_API void SetEntityCollection(FMassArchetypeEntityCollection&& InEntityCollection);
	void ClearEntityCollection();
	void SetAuxData(const FInstancedStruct& InAuxData);

	void SetExecutionType(EMassExecutionContextType InExecutionType);
	EMassExecutionContextType GetExecutionType() const;

	float GetDeltaTimeSeconds() const;

	MASSENTITY_API UWorld* GetWorld();

	TSharedPtr<FMassCommandBuffer> GetSharedDeferredCommandBuffer() const { return DeferredCommandBuffer; }
	FMassCommandBuffer& Defer() const { checkSlow(DeferredCommandBuffer.IsValid()); return *DeferredCommandBuffer.Get(); }

	TConstArrayView<FMassEntityHandle> GetEntities() const { return EntityListView; }
	int32 GetNumEntities() const { return EntityListView.Num(); }

	FMassEntityHandle GetEntity(const int32 Index) const
	{
		return EntityListView[Index];
	}

	void ForEachEntityInChunk(const FMassEntityExecuteFunction& EntityExecuteFunction)
	{
		for (FEntityIterator EntityIterator = CreateEntityIterator(); EntityIterator; ++EntityIterator)
		{
			EntityExecuteFunction(*this, EntityIterator);
		}
	}

	bool DoesArchetypeHaveFragment(const UScriptStruct& FragmentType) const
	{
		return CurrentArchetypeCompositionDescriptor.Fragments.Contains(FragmentType);
	}

	template<typename T>
	bool DoesArchetypeHaveFragment() const
	{
		static_assert(UE::Mass::CFragment<T>, "Given struct is not of a valid fragment type.");
		return CurrentArchetypeCompositionDescriptor.Fragments.Contains<T>();
	}

	bool DoesArchetypeHaveTag(const UScriptStruct& TagType) const
	{
		return CurrentArchetypeCompositionDescriptor.Tags.Contains(TagType);
	}

	template<typename T>
	bool DoesArchetypeHaveTag() const
	{
		static_assert(UE::Mass::CTag<T>, "Given struct is not of a valid tag type.");
		return CurrentArchetypeCompositionDescriptor.Tags.Contains<T>();
	}

#if WITH_MASSENTITY_DEBUG
	FColor DebugGetArchetypeColor() const
	{
		return DebugColor;
	}
#endif // WITH_MASSENTITY_DEBUG

	/** Chunk related operations */
	void SetCurrentChunkSerialModificationNumber(const int32 SerialModificationNumber) { ChunkSerialModificationNumber = SerialModificationNumber; }
	int32 GetChunkSerialModificationNumber() const { return ChunkSerialModificationNumber; }

	template<typename T>
	T* GetMutableChunkFragmentPtr()
	{
		static_assert(UE::Mass::CChunkFragment<T>, "Given struct doesn't represent a valid chunk fragment type. Make sure to inherit from FMassChunkFragment or one of its child-types.");

		const UScriptStruct* Type = T::StaticStruct();
		FChunkFragmentView* FoundChunkFragmentData = ChunkFragmentViews.FindByPredicate([Type](const FChunkFragmentView& Element) { return Element.Requirement.StructType == Type; } );
		CHECK_IF_READWRITE(FoundChunkFragmentData);
		return FoundChunkFragmentData ? FoundChunkFragmentData->FragmentView.GetPtr<T>() : static_cast<T*>(nullptr);
	}
	
	template<typename T>
	T& GetMutableChunkFragment()
	{
		T* ChunkFragment = GetMutableChunkFragmentPtr<T>();
		CHECK_IF_VALID(ChunkFragment, T::StaticStruct());
		return *ChunkFragment;
	}

	template<typename T>
	const T* GetChunkFragmentPtr() const
	{
		static_assert(UE::Mass::CChunkFragment<T>, "Given struct doesn't represent a valid chunk fragment type. Make sure to inherit from FMassChunkFragment or one of its child-types.");

		const UScriptStruct* Type = T::StaticStruct();
		const FChunkFragmentView* FoundChunkFragmentData = ChunkFragmentViews.FindByPredicate([Type](const FChunkFragmentView& Element) { return Element.Requirement.StructType == Type; } );
		return FoundChunkFragmentData ? FoundChunkFragmentData->FragmentView.GetPtr<T>() : static_cast<const T*>(nullptr);
	}
	
	template<typename T>
	const T& GetChunkFragment() const
	{
		const T* ChunkFragment = GetChunkFragmentPtr<T>();
		CHECK_IF_VALID(ChunkFragment, T::StaticStruct());
		return *ChunkFragment;
	}

	/** Shared fragment related operations */
	const void* GetConstSharedFragmentPtr(const UScriptStruct& SharedFragmentType) const
	{
		const FConstSharedFragmentView* FoundSharedFragmentData = ConstSharedFragmentViews.FindByPredicate([&SharedFragmentType](const FConstSharedFragmentView& Element) { return Element.Requirement.StructType == &SharedFragmentType; });
		return FoundSharedFragmentData ? FoundSharedFragmentData->FragmentView.GetMemory() : nullptr;
	}
	
	template<typename T>
	const T* GetConstSharedFragmentPtr() const
	{
		static_assert(UE::Mass::CConstSharedFragment<T>, "Given struct doesn't represent a valid const shared fragment type. Make sure to inherit from FMassConstSharedFragment or one of its child-types.");

		const FConstSharedFragmentView* FoundSharedFragmentData = ConstSharedFragmentViews.FindByPredicate([](const FConstSharedFragmentView& Element) { return Element.Requirement.StructType == T::StaticStruct(); });
		return FoundSharedFragmentData ? FoundSharedFragmentData->FragmentView.GetPtr<const T>() : static_cast<const T*>(nullptr);
	}

	template<typename T>
	const T& GetConstSharedFragment() const
	{
		const T* SharedFragment = GetConstSharedFragmentPtr<const T>();
		CHECK_IF_VALID(SharedFragment, T::StaticStruct());
		return *SharedFragment;
	}

	template<typename T>
	T* GetMutableSharedFragmentPtr()
	{
		static_assert(UE::Mass::CSharedFragment<T>, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");

		FSharedFragmentView* FoundSharedFragmentData = SharedFragmentViews.FindByPredicate([](const FSharedFragmentView& Element) { return Element.Requirement.StructType == T::StaticStruct(); });
		CHECK_IF_READWRITE(FoundSharedFragmentData);
		return FoundSharedFragmentData ? FoundSharedFragmentData->FragmentView.GetPtr<T>() : static_cast<T*>(nullptr);
	}

	template<typename T>
	const T* GetSharedFragmentPtr() const
	{
		static_assert(UE::Mass::CSharedFragment<T>, "Given struct doesn't represent a valid shared fragment type. Make sure to inherit from FMassSharedFragment or one of its child-types.");

		const FSharedFragmentView* FoundSharedFragmentData = SharedFragmentViews.FindByPredicate([](const FSharedFragmentView& Element) { return Element.Requirement.StructType == T::StaticStruct(); });
		return FoundSharedFragmentData ? FoundSharedFragmentData->FragmentView.GetPtr<T>() : static_cast<const T*>(nullptr);
	}

	template<typename T>
	T& GetMutableSharedFragment()
	{
		T* SharedFragment = GetMutableSharedFragmentPtr<T>();
		CHECK_IF_VALID(SharedFragment, T::StaticStruct());
		return *SharedFragment;
	}

	template<typename T>
	const T& GetSharedFragment() const
	{
		const T* SharedFragment = GetSharedFragmentPtr<T>();
		CHECK_IF_VALID(SharedFragment, T::StaticStruct());
		return *SharedFragment;
	}

	/* Fragments related operations */
	template<typename TFragment>
	TArrayView<TFragment> GetMutableFragmentView()
	{
		const UScriptStruct* FragmentType = TFragment::StaticStruct();
		const FFragmentView* View = FragmentViews.FindByPredicate([FragmentType](const FFragmentView& Element) { return Element.Requirement.StructType == FragmentType; });
		CHECK_IF_VALID(View, FragmentType);
		CHECK_IF_READWRITE(View);
		return MakeArrayView<TFragment>((TFragment*)View->FragmentView.GetData(), View->FragmentView.Num());
	}

	template<typename TFragment>
	TConstArrayView<TFragment> GetFragmentView() const
	{
		const UScriptStruct* FragmentType = TFragment::StaticStruct();
		const FFragmentView* View = FragmentViews.FindByPredicate([FragmentType](const FFragmentView& Element) { return Element.Requirement.StructType == FragmentType; });
		CHECK_IF_VALID(View, TFragment::StaticStruct());
		return TConstArrayView<TFragment>((const TFragment*)View->FragmentView.GetData(), View->FragmentView.Num());
	}

	TConstArrayView<FMassFragment> GetFragmentView(const UScriptStruct* FragmentType) const
	{
		const FFragmentView* View = FragmentViews.FindByPredicate([FragmentType](const FFragmentView& Element) { return Element.Requirement.StructType == FragmentType; });
		CHECK_IF_VALID(View, FragmentType);
		return TConstArrayView<FMassFragment>((const FMassFragment*)View->FragmentView.GetData(), View->FragmentView.Num());;
	}

	TArrayView<FMassFragment> GetMutableFragmentView(const UScriptStruct* FragmentType) 
	{
		const FFragmentView* View = FragmentViews.FindByPredicate([FragmentType](const FFragmentView& Element) { return Element.Requirement.StructType == FragmentType; });
		CHECK_IF_VALID(View, FragmentType);
		CHECK_IF_READWRITE(View);
		return View->FragmentView;
	}

	template<typename TFragmentBase>
	TConstArrayView<TFragmentBase> GetFragmentView(TNotNull<const UScriptStruct*> FragmentType) const
	{
		check(FragmentType->IsChildOf(TFragmentBase::StaticStruct()));
		TConstArrayView<FMassFragment> View = GetFragmentView(FragmentType);
		return TConstArrayView<TFragmentBase>(reinterpret_cast<const TFragmentBase*>(View.GetData()), View.Num());
	}

	template<typename TFragmentBase>
	TArrayView<TFragmentBase> GetMutableFragmentView(TNotNull<const UScriptStruct*> FragmentType)
	{
		check(FragmentType->IsChildOf(TFragmentBase::StaticStruct()));
		TArrayView<FMassFragment> View = GetMutableFragmentView(FragmentType);
		return TArrayView<TFragmentBase>(reinterpret_cast<TFragmentBase*>(View.GetData()), View.Num());;
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T* GetMutableSubsystem()
	{
		return SubsystemAccess.GetMutableSubsystem<T>();
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T& GetMutableSubsystemChecked()
	{
		return SubsystemAccess.GetMutableSubsystemChecked<T>();
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T* GetSubsystem()
	{
		return SubsystemAccess.GetSubsystem<T>();
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T& GetSubsystemChecked()
	{
		return SubsystemAccess.GetSubsystemChecked<T>();
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T* GetMutableSubsystem(const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetMutableSubsystem<T>(SubsystemClass);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	T& GetMutableSubsystemChecked(const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetMutableSubsystemChecked<T>(SubsystemClass);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T* GetSubsystem(const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetSubsystem<T>(SubsystemClass);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, USubsystem>::IsDerived>::Type>
	const T& GetSubsystemChecked(const TSubclassOf<USubsystem> SubsystemClass)
	{
		return SubsystemAccess.GetSubsystemChecked<T>(SubsystemClass);
	}

	/** Sparse chunk related operation */
	const FMassArchetypeEntityCollection& GetEntityCollection() const { return EntityCollection; }

	const FInstancedStruct& GetAuxData() const { return AuxData; }
	FInstancedStruct& GetMutableAuxData() { return AuxData; }
	
	template<typename TFragment>
	bool ValidateAuxDataType() const
	{
		const UScriptStruct* FragmentType = GetAuxData().GetScriptStruct();
		return FragmentType != nullptr && FragmentType == TFragment::StaticStruct();
	}

	MASSENTITY_API void FlushDeferred();

	void ClearExecutionData();
	void SetCurrentArchetypeCompositionDescriptor(const FMassArchetypeCompositionDescriptor& Descriptor)
	{
		CurrentArchetypeCompositionDescriptor = Descriptor;
	}

#if WITH_MASSENTITY_DEBUG
	void DebugSetColor(FColor InColor)
	{
		DebugColor = InColor;
	}
#endif // WITH_MASSENTITY_DEBUG

	/** 
	 * Processes SubsystemRequirements to fetch and cache all the indicated subsystems. If a UWorld is required to fetch
	 * a specific subsystem then the one associated with the stored EntityManager will be used.
	 *
	 * @param SubsystemRequirements indicates all the subsystems that are expected to be accessed. Requesting a subsystem 
	 *	not indicated by the SubsystemRequirements will result in a failure.
	 * 
	 * @return `true` if all required subsystems have been found, `false` otherwise.
	 */
	bool CacheSubsystemRequirements(const FMassSubsystemRequirements& SubsystemRequirements);

protected:
	void SetSubsystemRequirements(const FMassSubsystemRequirements& SubsystemRequirements)
	{
		SubsystemAccess.SetSubsystemRequirements(SubsystemRequirements);
	}

	void SetFragmentRequirements(const FMassFragmentRequirements& FragmentRequirements);

	void ClearFragmentViews()
	{
		for (FFragmentView& View : FragmentViews)
		{
			View.FragmentView = TArrayView<FMassFragment>();
		}
		for (FChunkFragmentView& View : ChunkFragmentViews)
		{
			View.FragmentView.Reset();
		}
		for (FConstSharedFragmentView& View : ConstSharedFragmentViews)
		{
			View.FragmentView.Reset();
		}
		for (FSharedFragmentView& View : SharedFragmentViews)
		{
			View.FragmentView.Reset();
		}
	}

public:
	//-----------------------------------------------------------------------------
	// DEPRECATED functions
	//-----------------------------------------------------------------------------
	UE_DEPRECATED(5.4, "Deprecated in favor of 'SetCurrentArchetypeCompositionDescriptor' as this provides information on the entire archetype.")
	void SetCurrentArchetypesTagBitSet(const FMassTagBitSet&) {}
};

//-----------------------------------------------------------------------------
// Inlines
//-----------------------------------------------------------------------------
inline FMassEntityManager& FMassExecutionContext::GetEntityManagerChecked() const
{
	return EntityManager.Get();
}

inline const TSharedRef<FMassEntityManager>& FMassExecutionContext::GetSharedEntityManager()
{
	return EntityManager;
}

inline void FMassExecutionContext::SetFlushDeferredCommands(const bool bNewFlushDeferredCommands)
{
	bFlushDeferredCommands = bNewFlushDeferredCommands;
}

inline void FMassExecutionContext::SetDeferredCommandBuffer(const TSharedPtr<FMassCommandBuffer>& InDeferredCommandBuffer)
{
	DeferredCommandBuffer = InDeferredCommandBuffer;
}

inline void FMassExecutionContext::ClearEntityCollection()
{
	EntityCollection.Reset();
}

inline void FMassExecutionContext::SetAuxData(const FInstancedStruct& InAuxData)
{
	AuxData = InAuxData;
}

inline float FMassExecutionContext::GetDeltaTimeSeconds() const
{
	return DeltaTimeSeconds;
}

inline void FMassExecutionContext::SetExecutionType(EMassExecutionContextType InExecutionType)
{
	check(InExecutionType != EMassExecutionContextType::MAX);
	ExecutionType = InExecutionType;
}

inline EMassExecutionContextType FMassExecutionContext::GetExecutionType() const
{
	return ExecutionType;
}

inline const FMassEntityQuery& FMassExecutionContext::GetCurrentQuery() const
{
	check(QueriesStack.Num());
	return *QueriesStack.Last().Query;
}

inline bool FMassExecutionContext::IsCurrentQuery(const FMassEntityQuery& Query) const
{
	return QueriesStack.Num() && QueriesStack.Last().Query == &Query;
}

inline void FMassExecutionContext::ApplyFragmentRequirements(const FMassEntityQuery& RequestingQuery)
{
	check(IsCurrentQuery(RequestingQuery));
	SetFragmentRequirements(RequestingQuery);
}

inline void FMassExecutionContext::ClearFragmentViews(const FMassEntityQuery& RequestingQuery)
{
	check(IsCurrentQuery(RequestingQuery));
	ClearFragmentViews();
}

#undef CHECK_IF_VALID
#undef CHECK_IF_READWRITE
