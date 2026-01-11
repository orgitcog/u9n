// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeInstanceData.h"
#include "StateTreeExecutionTypes.h"
#include "Debugger/StateTreeRuntimeValidationInstanceData.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "VisualLogger/VisualLogger.h"
#include "StateTree.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "StateTreeDelegate.h"
#include "AutoRTFM.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeInstanceData)

const FGuid FStateTreeInstanceStorageCustomVersion::GUID(0x60C4F0DE, 0x8B264C34, 0xAA937201, 0x5DFF09CC);
FCustomVersionRegistration GRegisterStateTreeInstanceStorageCustomVersion(FStateTreeInstanceStorageCustomVersion::GUID, FStateTreeInstanceStorageCustomVersion::LatestVersion, TEXT("StateTreeInstanceStorage"));

namespace UE::StateTree
{
	/**
	 * Duplicates object, and tries to covert old BP classes (REINST_*) to their newer version.
	 */
	TNotNull<UObject*> CopyNodeInstance(TNotNull<UObject*> Instance, TNotNull<UObject*> InOwner, bool bDuplicate)
	{
		const UClass* InstanceClass = Instance->GetClass();
		if (InstanceClass->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			const UClass* AuthoritativeClass = InstanceClass->GetAuthoritativeClass();
			UObject* NewInstance = NewObject<UObject>(InOwner, AuthoritativeClass);

			// Try to copy the values over using serialization
			// FObjectAndNameAsStringProxyArchive is used to store and restore names and objects as memory writer does not support UObject references at all.
			TArray<uint8> Data;
			FMemoryWriter Writer(Data);
			FObjectAndNameAsStringProxyArchive WriterProxy(Writer, /*bInLoadIfFindFails*/true);
			Instance->Serialize(WriterProxy);

			FMemoryReader Reader(Data);
			FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, /*bInLoadIfFindFails*/true);
			NewInstance->Serialize(ReaderProxy);

			const UStateTree* OuterStateTree = Instance->GetTypedOuter<UStateTree>();
			UE_LOG(LogStateTree, Display, TEXT("FStateTreeInstanceData: Duplicating '%s' with old class '%s' as '%s', potential data loss. Please resave State Tree asset %s."),
				*GetFullNameSafe(Instance), *GetNameSafe(InstanceClass), *GetNameSafe(AuthoritativeClass), *GetFullNameSafe(OuterStateTree));

			return NewInstance;
		}

		if (bDuplicate)
		{
			return ::DuplicateObject(&(*Instance), &(*InOwner));
		}

		return Instance;
	}

#if WITH_EDITORONLY_DATA
	void GatherForLocalization(const FString& PathToParent, const UScriptStruct* Struct, const void* StructData, const void* DefaultStructData, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
	{
		const FStateTreeInstanceData* ThisInstance = static_cast<const FStateTreeInstanceData*>(StructData);
		const FStateTreeInstanceData* DefaultInstance = static_cast<const FStateTreeInstanceData*>(DefaultStructData);

		PropertyLocalizationDataGatherer.GatherLocalizationDataFromStruct(PathToParent, Struct, StructData, DefaultStructData, GatherTextFlags);

		const uint8* DefaultInstanceMemory = nullptr;
		if (DefaultInstance)
		{
			DefaultInstanceMemory = reinterpret_cast<const uint8*>(&DefaultInstance->GetStorage());
		}
		
		const UScriptStruct* StructTypePtr = FStateTreeInstanceStorage::StaticStruct();
		PropertyLocalizationDataGatherer.GatherLocalizationDataFromStructWithCallbacks(PathToParent + TEXT(".InstanceStorage"), StructTypePtr, &ThisInstance->GetStorage(), DefaultInstanceMemory, GatherTextFlags);
	}

	void RegisterInstanceDataForLocalization()
	{
		{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(TBaseStructure<FStateTreeInstanceData>::Get(), &GatherForLocalization); }
	}
#endif // WITH_EDITORONLY_DATA

} // UE::StateTree


//----------------------------------------------------------------//
// FStateTreeInstanceStorage
//----------------------------------------------------------------//

#if WITH_STATETREE_DEBUG
FStateTreeInstanceStorage::FStateTreeInstanceStorage()
	: RuntimeValidationData(MakePimpl<UE::StateTree::Debug::FRuntimeValidationInstanceData>())
{
}
#else
FStateTreeInstanceStorage::FStateTreeInstanceStorage() = default;
#endif

namespace UE::StateTree::InstanceData
{
	namespace Internal
	{
		bool IsHandleSourceValid(
			const FStateTreeInstanceStorage& InstanceStorage,
			const FStateTreeExecutionFrame* ParentFrame,
			const FStateTreeExecutionFrame& CurrentFrame,
			const FStateTreeDataHandle Handle)
		{
			// Checks that the instance data is valid for specific handle types.
			// 
			// The CurrentFrame may not be yet properly initialized, for that reason we need to check
			// that the path to the handle makes sense (it's part of the active states) as well as that
			// we actually have instance data for the handle (index is valid).
			// 
			// The (base) indices can be invalid if the frame/state is not entered yet.
			// For active instance data we need to check that the frame is initialized for a specific state,
			// as well as that the instance data is initialized.

			switch (Handle.GetSource())
			{
			case EStateTreeDataSourceType::None:
				return true;

			case EStateTreeDataSourceType::GlobalInstanceData:
			case EStateTreeDataSourceType::GlobalInstanceDataObject:
				return CurrentFrame.GlobalInstanceIndexBase.IsValid()
					&& InstanceStorage.IsValidIndex(CurrentFrame.GlobalInstanceIndexBase.Get() + Handle.GetIndex());

			case EStateTreeDataSourceType::ActiveInstanceData:
			case EStateTreeDataSourceType::ActiveInstanceDataObject:
			case EStateTreeDataSourceType::StateParameterData:
				return CurrentFrame.ActiveInstanceIndexBase.IsValid()
					&& CurrentFrame.ActiveStates.Contains(Handle.GetState(), CurrentFrame.NumCurrentlyActiveStates)
					&& InstanceStorage.IsValidIndex(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());

			case EStateTreeDataSourceType::SharedInstanceData:
			case EStateTreeDataSourceType::SharedInstanceDataObject:
				return true;

			case EStateTreeDataSourceType::GlobalParameterData:
				return ParentFrame
					? IsHandleSourceValid(InstanceStorage, nullptr, *ParentFrame, CurrentFrame.GlobalParameterDataHandle)
					: CurrentFrame.GlobalParameterDataHandle.IsValid();

			case EStateTreeDataSourceType::SubtreeParameterData:
				if (ParentFrame)
				{
					// If the current subtree state is not instantiated yet, we cannot assume that the parameter data is instantiated in the parent frame either. 
					if (!CurrentFrame.ActiveInstanceIndexBase.IsValid())
					{
						return false;
					}
					// Linked subtree, params defined in parent scope.
					return IsHandleSourceValid(InstanceStorage, nullptr, *ParentFrame, CurrentFrame.StateParameterDataHandle);
				}
				// Standalone subtree, params define as state params.
				return CurrentFrame.ActiveInstanceIndexBase.IsValid()
					&& CurrentFrame.ActiveStates.Contains(Handle.GetState(), CurrentFrame.NumCurrentlyActiveStates)
					&& InstanceStorage.IsValidIndex(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());

			default:
				checkf(false, TEXT("Unhandled case or unsupported type for InstanceDataStorage %s"), *UEnum::GetValueAsString(Handle.GetSource()));
			}

			return false;
		}

		FStateTreeDataView GetTemporaryDataView(
			FStateTreeInstanceStorage& InstanceStorage, 
			const FStateTreeExecutionFrame* ParentFrame, 
			const FStateTreeExecutionFrame& CurrentFrame, 
			const FStateTreeDataHandle Handle)
		{
			switch (Handle.GetSource())
			{
			case EStateTreeDataSourceType::GlobalInstanceData:
			case EStateTreeDataSourceType::ActiveInstanceData:
				return InstanceStorage.GetMutableTemporaryStruct(CurrentFrame, Handle);

			case EStateTreeDataSourceType::GlobalInstanceDataObject:
			case EStateTreeDataSourceType::ActiveInstanceDataObject:
				return InstanceStorage.GetMutableTemporaryObject(CurrentFrame, Handle);

			case EStateTreeDataSourceType::GlobalParameterData:
				if (ParentFrame)
				{
					if (FCompactStateTreeParameters* Params = InstanceStorage.GetMutableTemporaryStruct(*ParentFrame, CurrentFrame.GlobalParameterDataHandle).GetPtr<FCompactStateTreeParameters>())
					{
						return Params->Parameters.GetMutableValue();
					}
				}
				break;

			case EStateTreeDataSourceType::SubtreeParameterData:
				if (ParentFrame)
				{
					// Linked subtree, params defined in parent scope.
					if (FCompactStateTreeParameters* Params = InstanceStorage.GetMutableTemporaryStruct(*ParentFrame, CurrentFrame.StateParameterDataHandle).GetPtr<FCompactStateTreeParameters>())
					{
						return Params->Parameters.GetMutableValue();
					}
				}
				// Standalone subtree, params define as state params.
				if (FCompactStateTreeParameters* Params = InstanceStorage.GetMutableTemporaryStruct(CurrentFrame, Handle).GetPtr<FCompactStateTreeParameters>())
				{
					return Params->Parameters.GetMutableValue();
				}
				break;

			case EStateTreeDataSourceType::StateParameterData:
				if (FCompactStateTreeParameters* Params = InstanceStorage.GetMutableTemporaryStruct(CurrentFrame, Handle).GetPtr<FCompactStateTreeParameters>())
				{
					return Params->Parameters.GetMutableValue();
				}
				break;

			default:
				checkf(false, TEXT("Unhandled case or unsupported type for InstanceDataStorage %s"), *UEnum::GetValueAsString(Handle.GetSource()));
			}

			return {};
		}

	}

	FStateTreeDataView GetDataView(
		FStateTreeInstanceStorage& InstanceStorage, 
		FStateTreeInstanceStorage* SharedInstanceStorage,
		const FStateTreeExecutionFrame* ParentFrame, 
		const FStateTreeExecutionFrame& CurrentFrame, 
		const FStateTreeDataHandle Handle)
	{
		switch (Handle.GetSource())
		{
		case EStateTreeDataSourceType::GlobalInstanceData:
			return InstanceStorage.GetMutableStruct(CurrentFrame.GlobalInstanceIndexBase.Get() + Handle.GetIndex());
		case EStateTreeDataSourceType::GlobalInstanceDataObject:
			return InstanceStorage.GetMutableObject(CurrentFrame.GlobalInstanceIndexBase.Get() + Handle.GetIndex());

		case EStateTreeDataSourceType::ActiveInstanceData:
			return InstanceStorage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());
		case EStateTreeDataSourceType::ActiveInstanceDataObject:
			return InstanceStorage.GetMutableObject(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());

		case EStateTreeDataSourceType::SharedInstanceData:
			check(SharedInstanceStorage);
			return SharedInstanceStorage->GetMutableStruct(Handle.GetIndex());
		case EStateTreeDataSourceType::SharedInstanceDataObject:
			check(SharedInstanceStorage);
			return SharedInstanceStorage->GetMutableObject(Handle.GetIndex());

		case EStateTreeDataSourceType::GlobalParameterData:
			// Defined in parent frame or is root state tree parameters
			if (ParentFrame)
			{
				return GetDataView(InstanceStorage, SharedInstanceStorage, nullptr, *ParentFrame, CurrentFrame.GlobalParameterDataHandle);
			}
			return InstanceStorage.GetMutableGlobalParameters();

		case EStateTreeDataSourceType::SubtreeParameterData:
		{
			// Defined in parent frame.
			if (ParentFrame)
			{
				// Linked subtree, params defined in parent scope.
				return GetDataView(InstanceStorage, SharedInstanceStorage, nullptr, *ParentFrame, CurrentFrame.StateParameterDataHandle);
			}
			// Standalone subtree, params define as state params.
			FCompactStateTreeParameters& SubtreeParams = InstanceStorage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex()).Get<FCompactStateTreeParameters>();
			return SubtreeParams.Parameters.GetMutableValue();
		}

		case EStateTreeDataSourceType::StateParameterData:
		{
			FCompactStateTreeParameters& StateParams = InstanceStorage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex()).Get<FCompactStateTreeParameters>();
			return StateParams.Parameters.GetMutableValue();
		}

		case EStateTreeDataSourceType::StateEvent:
		{
			// Return FStateTreeEvent from shared event.
			FStateTreeSharedEvent& SharedEvent = InstanceStorage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex()).Get<FStateTreeSharedEvent>();
			if (ensure(SharedEvent.IsValid()))
			{
				// Events are read only, but we cannot express that in FStateTreeDataView.
				return FStateTreeDataView(FStructView::Make(*SharedEvent.GetMutable()));
			}
			return {};
		}

		default:
			checkf(false, TEXT("Unhandled case or unsupported type for InstanceDataStorage %s"), *UEnum::GetValueAsString(Handle.GetSource()));
		}

		return {};
	}

	FStateTreeDataView GetDataViewOrTemporary(FStateTreeInstanceStorage& InstanceStorage, FStateTreeInstanceStorage* SharedInstanceStorage,
		const FStateTreeExecutionFrame* ParentFrame, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle)
	{
		using namespace Internal;
		if (IsHandleSourceValid(InstanceStorage, ParentFrame, CurrentFrame, Handle))
		{
			return GetDataView(InstanceStorage, SharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
		}

		return GetTemporaryDataView(InstanceStorage, ParentFrame, CurrentFrame, Handle);
	}
}

FStateTreeInstanceStorage::FStateTreeInstanceStorage(const FStateTreeInstanceStorage& Other)
	: InstanceStructs(Other.InstanceStructs)
	, ExecutionState(Other.ExecutionState)
	, TemporaryInstances(Other.TemporaryInstances)
	, EventQueue(MakeShared<FStateTreeEventQueue>(*Other.EventQueue))
	, TransitionRequests(Other.TransitionRequests)
	, GlobalParameters(Other.GlobalParameters)
#if ENABLE_MT_DETECTOR
	, AccessDetector(Other.AccessDetector)
#endif
	, bIsOwningEventQueue(true)
#if WITH_STATETREE_DEBUG
	, RuntimeValidationData(MakePimpl<UE::StateTree::Debug::FRuntimeValidationInstanceData>(*Other.RuntimeValidationData))
#endif
{
}

FStateTreeInstanceStorage::FStateTreeInstanceStorage(FStateTreeInstanceStorage&& Other) noexcept
	: InstanceStructs(MoveTemp(Other.InstanceStructs))
	, ExecutionState(MoveTemp(Other.ExecutionState))
	, TemporaryInstances(MoveTemp(Other.TemporaryInstances))
	, EventQueue(Other.EventQueue)
	, TransitionRequests(MoveTemp(Other.TransitionRequests))
	, GlobalParameters(MoveTemp(Other.GlobalParameters))
#if ENABLE_MT_DETECTOR
	, AccessDetector(MoveTemp(Other.AccessDetector))
#endif
	, bIsOwningEventQueue(Other.bIsOwningEventQueue)
#if WITH_STATETREE_DEBUG
	, RuntimeValidationData(MoveTemp(Other.RuntimeValidationData))
#endif
{
	Other.EventQueue = MakeShared<FStateTreeEventQueue>();
#if WITH_STATETREE_DEBUG
	Other.RuntimeValidationData = MakePimpl<UE::StateTree::Debug::FRuntimeValidationInstanceData>();
#endif
}

FStateTreeInstanceStorage& FStateTreeInstanceStorage::operator=(const FStateTreeInstanceStorage& Other)
{
	InstanceStructs = Other.InstanceStructs;
	ExecutionState = Other.ExecutionState;
	TemporaryInstances = Other.TemporaryInstances;
	EventQueue = MakeShared<FStateTreeEventQueue>(*Other.EventQueue);
	TransitionRequests = Other.TransitionRequests;
	GlobalParameters = Other.GlobalParameters;

#if ENABLE_MT_DETECTOR
	AccessDetector = Other.AccessDetector;
#endif

	bIsOwningEventQueue = true;

#if WITH_STATETREE_DEBUG
	RuntimeValidationData = MakePimpl<UE::StateTree::Debug::FRuntimeValidationInstanceData>(*Other.RuntimeValidationData);
#endif

	return *this;
}

FStateTreeInstanceStorage& FStateTreeInstanceStorage::operator=(FStateTreeInstanceStorage&& Other) noexcept
{
	InstanceStructs = MoveTemp(Other.InstanceStructs);
	ExecutionState = MoveTemp(Other.ExecutionState);
	TemporaryInstances = MoveTemp(Other.TemporaryInstances);
	EventQueue = Other.EventQueue;
	Other.EventQueue = MakeShared<FStateTreeEventQueue>();
	TransitionRequests = MoveTemp(Other.TransitionRequests);
	GlobalParameters = MoveTemp(Other.GlobalParameters);

#if ENABLE_MT_DETECTOR
	AccessDetector = MoveTemp(Other.AccessDetector);
#endif

	bIsOwningEventQueue = Other.bIsOwningEventQueue;
	Other.bIsOwningEventQueue = true;

#if WITH_STATETREE_DEBUG
	RuntimeValidationData = MoveTemp(Other.RuntimeValidationData);
	Other.RuntimeValidationData = MakePimpl<UE::StateTree::Debug::FRuntimeValidationInstanceData>();
#endif

	return *this;
}

void FStateTreeInstanceStorage::SetSharedEventQueue(const TSharedRef<FStateTreeEventQueue>& InSharedEventQueue)
{
	EventQueue = InSharedEventQueue;
	bIsOwningEventQueue = false;
}

void FStateTreeInstanceStorage::AddTransitionRequest(const UObject* Owner, const FStateTreeTransitionRequest& Request)
{
	constexpr int32 MaxPendingTransitionRequests = 32;
	
	if (TransitionRequests.Num() >= MaxPendingTransitionRequests)
	{
		UE_VLOG_UELOG(Owner, LogStateTree, Error, TEXT("%s: Too many transition requests sent to '%s' (%d pending). Dropping request."), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner), TransitionRequests.Num());
		return;
	}

	TransitionRequests.Add(Request);
}

void FStateTreeInstanceStorage::MarkDelegateAsBroadcasted(const FStateTreeDelegateDispatcher& Dispatcher)
{
	// The array is reset once the transitions are processed.
	BroadcastedDelegates.AddUnique(Dispatcher);
}

bool FStateTreeInstanceStorage::IsDelegateBroadcasted(const FStateTreeDelegateDispatcher& Dispatcher) const
{
	return BroadcastedDelegates.Contains(Dispatcher);
}

void FStateTreeInstanceStorage::ResetBroadcastedDelegates()
{
	BroadcastedDelegates.Empty();
}

bool FStateTreeInstanceStorage::HasBroadcastedDelegates() const
{
	return !BroadcastedDelegates.IsEmpty();
}

void FStateTreeInstanceStorage::ResetTransitionRequests()
{
	TransitionRequests.Reset();
}

bool FStateTreeInstanceStorage::AreAllInstancesValid() const
{
	for (FConstStructView Instance : InstanceStructs)
	{
		if (!Instance.IsValid())
		{
			return false;
		}
		if (const FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FStateTreeInstanceObjectWrapper>())
		{
			if (!Wrapper->InstanceObject)
			{
				return false;
			}
		}
	}
	return true;
}

FStructView FStateTreeInstanceStorage::AddTemporaryInstance(UObject& InOwner, const FStateTreeExecutionFrame& Frame, const FStateTreeIndex16 OwnerNodeIndex, const FStateTreeDataHandle DataHandle, FConstStructView NewInstanceData)
{
	FStateTreeTemporaryInstanceData* TempInstance = TemporaryInstances.FindByPredicate([&Frame, &OwnerNodeIndex, &DataHandle](const FStateTreeTemporaryInstanceData& TempInstance)
	{
		return TempInstance.FrameID == Frame.FrameID
				&& TempInstance.OwnerNodeIndex == OwnerNodeIndex
				&& TempInstance.DataHandle == DataHandle;
	});
	
	if (TempInstance)
	{
		if (TempInstance->Instance.GetScriptStruct() != NewInstanceData.GetScriptStruct())
		{
			TempInstance->Instance = NewInstanceData;
		}
	}
	else
	{
		TempInstance = &TemporaryInstances.AddDefaulted_GetRef();
		check(TempInstance);
		TempInstance->FrameID = Frame.FrameID;
		TempInstance->OwnerNodeIndex = OwnerNodeIndex;
		TempInstance->DataHandle = DataHandle;
		TempInstance->Instance = NewInstanceData;
	}

	if (FStateTreeInstanceObjectWrapper* Wrapper = TempInstance->Instance.GetMutablePtr<FStateTreeInstanceObjectWrapper>())
	{
		if (Wrapper->InstanceObject)
		{
			constexpr bool bDuplicate = true;
			Wrapper->InstanceObject = UE::StateTree::CopyNodeInstance(Wrapper->InstanceObject, &InOwner, bDuplicate);
		}
	}

	return TempInstance->Instance;
}

FStructView FStateTreeInstanceStorage::GetMutableTemporaryStruct(const FStateTreeExecutionFrame& Frame, const FStateTreeDataHandle DataHandle)
{
	FStateTreeTemporaryInstanceData* ExistingInstance = TemporaryInstances.FindByPredicate([&Frame, &DataHandle](const FStateTreeTemporaryInstanceData& TempInstance)
	{
		return TempInstance.FrameID == Frame.FrameID
				&& TempInstance.DataHandle == DataHandle;
	});
	return ExistingInstance ? FStructView(ExistingInstance->Instance) : FStructView();
}

UObject* FStateTreeInstanceStorage::GetMutableTemporaryObject(const FStateTreeExecutionFrame& Frame, const FStateTreeDataHandle DataHandle)
{
	FStateTreeTemporaryInstanceData* ExistingInstance = TemporaryInstances.FindByPredicate([&Frame, &DataHandle](const FStateTreeTemporaryInstanceData& TempInstance)
	{
		return TempInstance.FrameID == Frame.FrameID
				&& TempInstance.DataHandle == DataHandle;
	});
	if (ExistingInstance)
	{
		const FStateTreeInstanceObjectWrapper& Wrapper = ExistingInstance->Instance.Get<FStateTreeInstanceObjectWrapper>();
		return Wrapper.InstanceObject;
	}
	return nullptr;
}

void FStateTreeInstanceStorage::ResetTemporaryInstances()
{
	TemporaryInstances.Reset();
}


void FStateTreeInstanceStorage::SetGlobalParameters(const FInstancedPropertyBag& Parameters)
{
	GlobalParameters = Parameters;
}

uint32 FStateTreeInstanceStorage::GenerateUniqueId()
{
	uint32 NewId = ++UniqueIdGenerator;
	if (NewId == 0)
	{
#if WITH_STATETREE_TRACE && DO_ENSURE
		ensureAlwaysMsgf(false, TEXT("The unique id overflow. Id:%d Serial:%d"), ExecutionState.InstanceDebugId.Id, ExecutionState.InstanceDebugId.SerialNumber);
#elif WITH_STATETREE_TRACE
	UE_LOG(LogStateTree, Error, TEXT("The unique id overflow. Id:%d Serial:%d"), ExecutionState.InstanceDebugId.Id, ExecutionState.InstanceDebugId.SerialNumber);
#else
	UE_LOG(LogStateTree, Error, TEXT("The unique id overflow."));
#endif
		NewId = ++UniqueIdGenerator;
	}
	return NewId;
}

void FStateTreeInstanceStorage::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddPropertyReferencesWithStructARO(TBaseStructure<FStateTreeInstanceStorage>::Get(), this);
	Collector.AddPropertyReferencesWithStructARO(TBaseStructure<FStateTreeEventQueue>::Get(), &EventQueue.Get());
}

void FStateTreeInstanceStorage::Reset()
{
	InstanceStructs.Reset();
	ExecutionState.Reset();
	TemporaryInstances.Reset();
	if (bIsOwningEventQueue)
	{
		EventQueue->Reset();
	}
	TransitionRequests.Reset();
	GlobalParameters.Reset();

#if WITH_STATETREE_DEBUG
	RuntimeValidationData = MakePimpl<UE::StateTree::Debug::FRuntimeValidationInstanceData>();
#endif
}

// #jira SOL-8070: Ideally, we should use the transactionally-safe access detector instead of relying on OPEN and ONABORT blocks here.
void FStateTreeInstanceStorage::AcquireReadAccess()
{
	UE_AUTORTFM_OPEN
	{
		UE_MT_ACQUIRE_READ_ACCESS(AccessDetector);
	};
	UE_AUTORTFM_ONABORT(this)
	{
		UE_MT_RELEASE_READ_ACCESS(AccessDetector);
	};
}

void FStateTreeInstanceStorage::ReleaseReadAccess()
{
	UE_AUTORTFM_OPEN
	{
		UE_MT_RELEASE_READ_ACCESS(AccessDetector);
	};
	UE_AUTORTFM_ONABORT(this)
	{
		UE_MT_ACQUIRE_READ_ACCESS(AccessDetector);
	};
}

void FStateTreeInstanceStorage::AcquireWriteAccess()
{
	UE_AUTORTFM_OPEN
	{
		UE_MT_ACQUIRE_WRITE_ACCESS(AccessDetector);
	};
	UE_AUTORTFM_ONABORT(this)
	{
		UE_MT_RELEASE_WRITE_ACCESS(AccessDetector);
	};
}

void FStateTreeInstanceStorage::ReleaseWriteAccess()
{
	UE_AUTORTFM_OPEN
	{
		UE_MT_RELEASE_WRITE_ACCESS(AccessDetector);
	};
	UE_AUTORTFM_ONABORT(this)
	{
		UE_MT_ACQUIRE_WRITE_ACCESS(AccessDetector);
	};
}

UE::StateTree::Debug::FRuntimeValidation FStateTreeInstanceStorage::GetRuntimeValidation() const
{
#if WITH_STATETREE_DEBUG
	return UE::StateTree::Debug::FRuntimeValidation(RuntimeValidationData.Get());
#else
	return UE::StateTree::Debug::FRuntimeValidation();
#endif
}

//----------------------------------------------------------------//
// FStateTreeInstanceData
//----------------------------------------------------------------//

FStateTreeInstanceData::FAddArgs FStateTreeInstanceData::FAddArgs::Default;

FStateTreeInstanceData::FStateTreeInstanceData() = default;

FStateTreeInstanceData::FStateTreeInstanceData(const FStateTreeInstanceData& Other)
{
	InstanceStorage = MakeShared<FStateTreeInstanceStorage>(*Other.InstanceStorage);
}

FStateTreeInstanceData::FStateTreeInstanceData(FStateTreeInstanceData&& Other) noexcept
{
	InstanceStorage = Other.InstanceStorage;
	Other.InstanceStorage = MakeShared<FStateTreeInstanceStorage>();
}

FStateTreeInstanceData& FStateTreeInstanceData::operator=(const FStateTreeInstanceData& Other)
{
	InstanceStorage = MakeShared<FStateTreeInstanceStorage>(*Other.InstanceStorage);
	return *this;
}

FStateTreeInstanceData& FStateTreeInstanceData::operator=(FStateTreeInstanceData&& Other) noexcept
{
	InstanceStorage = Other.InstanceStorage;
	Other.InstanceStorage = MakeShared<FStateTreeInstanceStorage>();
	return *this;
}

FStateTreeInstanceData::~FStateTreeInstanceData()
{
	Reset();
}

const FStateTreeInstanceStorage& FStateTreeInstanceData::GetStorage() const
{
	return *InstanceStorage;
}

TWeakPtr<FStateTreeInstanceStorage> FStateTreeInstanceData::GetWeakMutableStorage()
{
	return InstanceStorage;
}

TWeakPtr<const FStateTreeInstanceStorage> FStateTreeInstanceData::GetWeakStorage() const
{
	return InstanceStorage;
}

FStateTreeInstanceStorage& FStateTreeInstanceData::GetMutableStorage()
{
	return *InstanceStorage;
}

FStateTreeEventQueue& FStateTreeInstanceData::GetMutableEventQueue()
{
	return GetMutableStorage().GetMutableEventQueue();
}

const TSharedRef<FStateTreeEventQueue>& FStateTreeInstanceData::GetSharedMutableEventQueue()
{
	return GetMutableStorage().GetSharedMutableEventQueue();
}

const FStateTreeEventQueue& FStateTreeInstanceData::GetEventQueue() const
{
	return GetStorage().GetEventQueue();
}

bool FStateTreeInstanceData::IsOwningEventQueue() const
{
	return GetStorage().IsOwningEventQueue();
}

void FStateTreeInstanceData::SetSharedEventQueue(const TSharedRef<FStateTreeEventQueue>& InSharedEventQueue)
{
	return GetMutableStorage().SetSharedEventQueue(InSharedEventQueue);
}

void FStateTreeInstanceData::AddTransitionRequest(const UObject* Owner, const FStateTreeTransitionRequest& Request)
{
	GetMutableStorage().AddTransitionRequest(Owner, Request);
}

TConstArrayView<FStateTreeTransitionRequest> FStateTreeInstanceData::GetTransitionRequests() const
{
	return GetStorage().GetTransitionRequests();
}

void FStateTreeInstanceData::ResetTransitionRequests()
{
	GetMutableStorage().ResetTransitionRequests();
}

bool FStateTreeInstanceData::AreAllInstancesValid() const
{
	return GetStorage().AreAllInstancesValid();
}

int32 FStateTreeInstanceData::GetEstimatedMemoryUsage() const
{
	const FStateTreeInstanceStorage& Storage = GetStorage();
	int32 Size = sizeof(FStateTreeInstanceData);

	Size += Storage.InstanceStructs.GetAllocatedMemory();

	for (FConstStructView Instance : Storage.InstanceStructs)
	{
		if (const FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				Size += Wrapper->InstanceObject->GetClass()->GetStructureSize();
			}
		}
	}

	return Size;
}

bool FStateTreeInstanceData::Identical(const FStateTreeInstanceData* Other, uint32 PortFlags) const
{
	if (Other == nullptr)
	{
		return false;
	}

	const FStateTreeInstanceStorage& Storage = GetStorage();
	const FStateTreeInstanceStorage& OtherStorage = Other->GetStorage();

	// Not identical if global parameters don't match.
	if (!Storage.GlobalParameters.Identical(&OtherStorage.GlobalParameters, PortFlags))
	{
		return false;
	}

	// Not identical if structs are different.
	if (Storage.InstanceStructs.Identical(&OtherStorage.InstanceStructs, PortFlags) == false)
	{
		return false;
	}
	
	// Check that the instance object contents are identical.
	// Copied from object property.
	auto AreObjectsIdentical = [](UObject* A, UObject* B, uint32 PortFlags) -> bool
	{
		if ((PortFlags & PPF_DuplicateForPIE) != 0)
		{
			return false;
		}

		if (A == B)
		{
			return true;
		}

		// Resolve the object handles and run the deep comparison logic 
		if ((PortFlags & (PPF_DeepCompareInstances | PPF_DeepComparison)) != 0)
		{
			return FObjectPropertyBase::StaticIdentical(A, B, PortFlags);
		}

		return true;
	};

	bool bResult = true;

	for (int32 Index = 0; Index < Storage.InstanceStructs.Num(); Index++)
	{
		const FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<const FStateTreeInstanceObjectWrapper>();
		const FStateTreeInstanceObjectWrapper* OtherWrapper = OtherStorage.InstanceStructs[Index].GetPtr<const FStateTreeInstanceObjectWrapper>();

		if (Wrapper)
		{
			if (!OtherWrapper)
			{
				bResult = false;
				break;
			}
			if (Wrapper->InstanceObject && OtherWrapper->InstanceObject)
			{
				if (!AreObjectsIdentical(Wrapper->InstanceObject, OtherWrapper->InstanceObject, PortFlags))
				{
					bResult = false;
					break;
				}
			}
		}
	}
	
	return bResult;
}

void FStateTreeInstanceData::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	GetMutableStorage().AddStructReferencedObjects(Collector);
}

bool FStateTreeInstanceData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FStateTreeInstanceStorageCustomVersion::GUID);

	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FStateTreeInstanceStorageCustomVersion::GUID) < FStateTreeInstanceStorageCustomVersion::AddedCustomSerialization)
		{
#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			StaticStruct()->SerializeTaggedProperties(Ar, (uint8*)this, StaticStruct(), nullptr);

			if (InstanceStorage_DEPRECATED.IsValid())
			{
				InstanceStorage = MakeShared<FStateTreeInstanceStorage>(MoveTemp(InstanceStorage_DEPRECATED.GetMutable()));
				InstanceStorage_DEPRECATED.Reset();
				return true;
			}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA

			InstanceStorage = MakeShared<FStateTreeInstanceStorage>();
			return true;
		}

		InstanceStorage = MakeShared<FStateTreeInstanceStorage>();
	}

	FStateTreeInstanceStorage::StaticStruct()->SerializeItem(Ar, &InstanceStorage.Get(), nullptr);

	return true;
}

void FStateTreeInstanceData::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	UScriptStruct* ScriptStruct = FStateTreeInstanceStorage::StaticStruct();
	OutDeps.Add(ScriptStruct);

	if (UScriptStruct::ICppStructOps* CppStructOps = ScriptStruct->GetCppStructOps())
	{
		CppStructOps->GetPreloadDependencies(&GetMutableStorage(), OutDeps);
	}

	for (TPropertyValueIterator<FStructProperty> It(ScriptStruct, &GetMutableStorage()); It; ++It)
	{
		const UScriptStruct* StructType = It.Key()->Struct;
		if (UScriptStruct::ICppStructOps* CppStructOps = StructType->GetCppStructOps())
		{
			void* StructDataPtr = const_cast<void*>(It.Value());
			CppStructOps->GetPreloadDependencies(StructDataPtr, OutDeps);
		}
	}
}

void FStateTreeInstanceData::CopyFrom(UObject& InOwner, const FStateTreeInstanceData& InOther)
{
	if (&InOther == this)
	{
		return;
	}

	FStateTreeInstanceStorage& Storage = GetMutableStorage();
	const FStateTreeInstanceStorage& OtherStorage = InOther.GetStorage();

	// Copy structs
	Storage.InstanceStructs = OtherStorage.InstanceStructs;

	// Copy instance objects.
	for (FStructView Instance : Storage.InstanceStructs)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Instance.GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				constexpr bool bDuplicate = true;
				Wrapper->InstanceObject = UE::StateTree::CopyNodeInstance(Wrapper->InstanceObject, &InOwner, bDuplicate);
			}
		}
	}
}

void FStateTreeInstanceData::Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, FAddArgs Args)
{
	Reset();
	Append(InOwner, InStructs, Args);
}

void FStateTreeInstanceData::Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, FAddArgs Args)
{
	Reset();
	Append(InOwner, InStructs, Args);
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, FAddArgs Args)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);

	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				const bool bDuplicate = Args.bDuplicateWrappedObject || &InOwner != Wrapper->InstanceObject->GetOuter();
				Wrapper->InstanceObject = UE::StateTree::CopyNodeInstance(Wrapper->InstanceObject, &InOwner, bDuplicate);
			}
		}
	}
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, FAddArgs Args)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);
	
	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		if (FStateTreeInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				const bool bDuplicate = Args.bDuplicateWrappedObject || &InOwner != Wrapper->InstanceObject->GetOuter();
				Wrapper->InstanceObject = UE::StateTree::CopyNodeInstance(Wrapper->InstanceObject, &InOwner, bDuplicate);
			}
		}
	}
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<FInstancedStruct*> InInstancesToMove, FAddArgs Args)
{
	check(InStructs.Num() == InInstancesToMove.Num());
	
	FStateTreeInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);

	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		FStructView Struct = Storage.InstanceStructs[Index];
		FInstancedStruct* Source = InInstancesToMove[Index - StartIndex];

		// The source is used to move temporary instance data into instance data. Not all entries may have it.
		// The instance struct can be empty, in which case the temporary instance is ignored.
		// If the source is specified, move it to the instance data.
		// We assume that if the source is object wrapper, it is already the instance we want.
		if (Struct.IsValid()
			&& (Source && Source->IsValid()))
		{
			check(Struct.GetScriptStruct() == Source->GetScriptStruct());
				
			FMemory::Memswap(Struct.GetMemory(), Source->GetMutableMemory(), Struct.GetScriptStruct()->GetStructureSize());
			Source->Reset();
		}
		else if (FStateTreeInstanceObjectWrapper* Wrapper = Struct.GetPtr<FStateTreeInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				const bool bDuplicate = Args.bDuplicateWrappedObject || &InOwner != Wrapper->InstanceObject->GetOuter();
				Wrapper->InstanceObject = UE::StateTree::CopyNodeInstance(Wrapper->InstanceObject, &InOwner, bDuplicate);
			}
		}
	}
}

void FStateTreeInstanceData::ShrinkTo(const int32 NumStructs)
{
	FStateTreeInstanceStorage& Storage = GetMutableStorage();
	check(NumStructs <= Storage.InstanceStructs.Num());  
	Storage.InstanceStructs.SetNum(NumStructs);
}

void FStateTreeInstanceData::Reset()
{
	GetMutableStorage().Reset();

}
