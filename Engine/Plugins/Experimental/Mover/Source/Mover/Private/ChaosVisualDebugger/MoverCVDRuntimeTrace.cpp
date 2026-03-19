// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVisualDebugger/MoverCVDRuntimeTrace.h"

#if WITH_CHAOS_VISUAL_DEBUGGER

#include "MoverCVDDataWrappers.h"
#include "MoverSimulationTypes.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "MoverComponent.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "ChaosVisualDebugger/ChaosVDTraceMacros.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Interfaces/IPhysicsComponent.h"
#include "Engine/World.h"

namespace UE::MoverUtils
{

CVD_DEFINE_OPTIONAL_DATA_CHANNEL(MoverNetworkedData, EChaosVDDataChannelInitializationFlags::CanChangeEnabledState)
CVD_DEFINE_OPTIONAL_DATA_CHANNEL(MoverLocalSimData, EChaosVDDataChannelInitializationFlags::CanChangeEnabledState)

// FSkipObjectRefsMemoryWriter and FSkipObjectRefsMemoryReader are a 
// workaround for serializing mover info structs with object references in them, such as the mover base
// Ultimately we might want to attempt to translate the object to a particle ID and resolve that on the CVD side
// by customizing the display and show the particle ID instead
class FSkipObjectRefsMemoryWriter : public FMemoryWriter
{
public:
	FSkipObjectRefsMemoryWriter(TArray<uint8, TSizedDefaultAllocator<32>>& InBytes, bool bIsPersistent = false, bool bSetOffset = false, const FName InArchiveName = NAME_None)
		: FMemoryWriter(InBytes, bIsPersistent, bSetOffset, InArchiveName)
	{
	}

	virtual FArchive& operator<<(struct FObjectPtr& Value)
	{
		return *this;
	}

	virtual FArchive& operator<<(struct FWeakObjectPtr& Value)
	{
		return *this;
	}
};
class FSkipObjectRefsMemoryReader : public FMemoryReader
{
public:
	FSkipObjectRefsMemoryReader(const TArray<uint8, TSizedDefaultAllocator<32>>& InBytes, bool bIsPersistent = false)
		: FMemoryReader(InBytes, bIsPersistent)
	{
	}

	virtual FArchive& operator<<(struct FObjectPtr& Value)
	{
		return *this;
	}

	virtual FArchive& operator<<(struct FWeakObjectPtr& Value)
	{
		return *this;
	}
};

void FMoverCVDRuntimeTrace::UnwrapSimData(const FMoverCVDSimDataWrapper& InSimDataWrapper, TSharedPtr<FMoverInputCmdContext>& OutInputCmd, TSharedPtr<FMoverSyncState>& OutSyncState, TSharedPtr<FMoverDataCollection>& OutLocalSimData)
{
	// Consider using Chaos::VisualDebugger::FChaosVDMemoryReader

	// Input Cmd
	{
		// Deserialize into a new struct allocated dynamically
		FMemoryReader ArReader(InSimDataWrapper.InputCmdBytes, true);
		OutInputCmd = MakeShared<FMoverInputCmdContext>();
		FMoverInputCmdContext::StaticStruct()->SerializeBin(ArReader, OutInputCmd.Get());
		// Input cmd's Collection of custom structs
		FSkipObjectRefsMemoryReader ArInputCollectionReader(InSimDataWrapper.InputMoverDataCollectionBytes, true);
		OutInputCmd->InputCollection.SerializeDebugData(ArInputCollectionReader);
	}

	// Sync State
	{
		// Deserialize into a new struct allocated dynamically
		FMemoryReader ArReader(InSimDataWrapper.SyncStateBytes, true);
		OutSyncState = MakeShared<FMoverSyncState>();
		FMoverSyncState::StaticStruct()->SerializeBin(ArReader, OutSyncState.Get());
		// Sync State's Collection of custom structs
		FSkipObjectRefsMemoryReader ArSyncStateCollectionReader(InSimDataWrapper.SyncStateDataCollectionBytes, true);
		OutSyncState->SyncStateCollection.SerializeDebugData(ArSyncStateCollectionReader);
	}

	{
		FSkipObjectRefsMemoryReader ArReader(InSimDataWrapper.LocalSimDataBytes, true);
		OutLocalSimData = MakeShared<FMoverDataCollection>();
		OutLocalSimData->SerializeDebugData(ArReader);
	}
}

void FMoverCVDRuntimeTrace::WrapSimData(uint32 SolverID, uint32 ParticleID, const FMoverInputCmdContext& InInputCmd, const FMoverSyncState& InSyncState, const FMoverDataCollection* LocalSimData, FMoverCVDSimDataWrapper& OutSimDataWrapper)
{
	OutSimDataWrapper.SolverID = SolverID;
	OutSimDataWrapper.ParticleID = ParticleID;

	// Consider using Chaos::VisualDebugger::FChaosVDMemoryWriter

	// Input Cmd
	{
		FMemoryWriter ArWriter(OutSimDataWrapper.InputCmdBytes, true);
		// This is not version friendly, we need to instead use SerializeTagParticles. Slower and Sergio is working on a faster version, but it's not available yet.
		InInputCmd.StaticStruct()->SerializeBin(ArWriter, &const_cast<FMoverInputCmdContext&>(InInputCmd));
	}
	// Input cmd's Collection of custom structs
	{
		FSkipObjectRefsMemoryWriter ArWriter(OutSimDataWrapper.InputMoverDataCollectionBytes, true);
		const_cast<FMoverInputCmdContext&>(InInputCmd).InputCollection.SerializeDebugData(ArWriter);
	}

	// Sync State
	{
		FMemoryWriter ArWriter(OutSimDataWrapper.SyncStateBytes, true);
		// This is not version friendly, we need to instead use SerializeTagParticles. Slower and Sergio is working on a faster version, but it's not available yet.
		InSyncState.StaticStruct()->SerializeBin(ArWriter, &const_cast<FMoverSyncState&>(InSyncState));
	}
	{
		FSkipObjectRefsMemoryWriter ArWriter(OutSimDataWrapper.SyncStateDataCollectionBytes, true);
		const_cast<FMoverSyncState&>(InSyncState).SyncStateCollection.SerializeDebugData(ArWriter);
	}

	// Local sim data (catch all other structs we want to record)
	{
		FSkipObjectRefsMemoryWriter ArWriter(OutSimDataWrapper.LocalSimDataBytes, true);
		if (LocalSimData)
		{
			(const_cast<FMoverDataCollection*>(LocalSimData))->SerializeDebugData(ArWriter);
		}
		else
		{
			FMoverDataCollection EmptyDataCollection;
			EmptyDataCollection.SerializeDebugData(ArWriter);
		}
	}
}

USTRUCT()
struct FMoverMergeableDataCollection : FMoverDataCollection
{
	void AppendShallow(const FMoverDataCollection& Other)
	{
		const FMoverMergeableDataCollection& MergeableOther = reinterpret_cast<const FMoverMergeableDataCollection&>(Other);
		for (int i = 0; i < MergeableOther.DataArray.Num(); ++i)
		{
			if (MergeableOther.DataArray[i].IsValid())
			{
				DataArray.Add(MergeableOther.DataArray[i]);
			}
		}
	}
};

void CombineDataCollections(const NamedDataCollections& DataCollections, FMoverDataCollection& OutDataCollection)
{
	FMoverMergeableDataCollection& OutMergeableDataCollection = reinterpret_cast<FMoverMergeableDataCollection&>(OutDataCollection);
	for (const TPair<FName, const FMoverDataCollection*>& DataCollectionEntry : DataCollections)
	{
		if (const FMoverDataCollection* DataCollectionToAppend = DataCollectionEntry.Value)
		{
			OutMergeableDataCollection.AppendShallow(*DataCollectionToAppend);
		}
	}
}

void FMoverCVDRuntimeTrace::TraceMoverData(UMoverComponent* MoverComponent, const FMoverInputCmdContext* InputCmd, const FMoverSyncState* SyncState, const NamedDataCollections* LocalSimDataCollections /*= nullptr*/)
{
	if (!FChaosVisualDebuggerTrace::IsTracing())
	{
		return;
	}

	if (!CVDDC_MoverNetworkedData->IsChannelEnabled())
	{
		return;
	}

	if (!InputCmd || !SyncState)
	{
		return;
	}

	if (UWorld* World = MoverComponent->GetWorld())
	{
		int32 ParticleID = INDEX_NONE;
		if (const IPhysicsComponent* PhysicsComponent = Cast<IPhysicsComponent>(MoverComponent->GetUpdatedComponent()))
		{
			Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
			const Chaos::FPhysicsObject* PhysicsObject = PhysicsComponent ? PhysicsComponent->GetPhysicsObjectById(0) : nullptr; // get the root physics object
			const Chaos::FGeometryParticleHandle* ParticleHandle = PhysicsObject? Interface.GetParticle(PhysicsObject) : nullptr;
			ParticleID = ParticleHandle ? ParticleHandle->UniqueIdx().Idx : INDEX_NONE;
		}

		int32 SolverID = CVD_TRACE_GET_SOLVER_ID_FROM_WORLD(World);
		const FMoverDataCollection* RecordedLocalSimData = nullptr;
		FMoverDataCollection MergedDataCollection;
		if (CVDDC_MoverLocalSimData->IsChannelEnabled() && LocalSimDataCollections)
		{
			// LocalSimState could add a lot of extra bytes, especially without some sort of delta serialization, so it is only optionally recorded
			CombineDataCollections(*LocalSimDataCollections, MergedDataCollection);
			RecordedLocalSimData = &MergedDataCollection;
		}
		TraceMoverDataPrivate(SolverID, ParticleID, InputCmd, SyncState, RecordedLocalSimData);
	}
}

void FMoverCVDRuntimeTrace::TraceMoverData(uint32 SolverID, uint32 ParticleID, const FMoverInputCmdContext* InputCmd, const FMoverSyncState* SyncState, const NamedDataCollections* LocalSimDataCollections /*= nullptr*/)
{
	if (!FChaosVisualDebuggerTrace::IsTracing())
	{
		return;
	}

	if (!CVDDC_MoverNetworkedData->IsChannelEnabled())
	{
		return;
	}

	if (!InputCmd || !SyncState)
	{
		return;
	}
		
	const FMoverDataCollection* RecordedLocalSimData = nullptr;
	FMoverDataCollection MergedDataCollection;
	if (CVDDC_MoverLocalSimData->IsChannelEnabled() && LocalSimDataCollections)
	{
		// LocalSimState could add a lot of extra bytes, especially without some sort of delta serialization, so it is only optionally recorded
		CombineDataCollections(*LocalSimDataCollections, MergedDataCollection);
		RecordedLocalSimData = &MergedDataCollection;
	}
	TraceMoverDataPrivate(SolverID, ParticleID, InputCmd, SyncState, RecordedLocalSimData);
}

void FMoverCVDRuntimeTrace::TraceMoverDataPrivate(uint32 SolverID, uint32 ParticleID, const FMoverInputCmdContext* InputCmd, const FMoverSyncState* SyncState, const FMoverDataCollection* LocalSimData /*= nullptr*/)
{
	FMoverCVDSimDataWrapper SimDataWrapper;
	WrapSimData(SolverID, ParticleID, *InputCmd, *SyncState, LocalSimData, SimDataWrapper);
	SimDataWrapper.MarkAsValid();

	FChaosVDScopedTLSBufferAccessor TLSDataBuffer;
	Chaos::VisualDebugger::WriteDataToBuffer(TLSDataBuffer.BufferRef, SimDataWrapper);

	FChaosVisualDebuggerTrace::TraceBinaryData(TLSDataBuffer.BufferRef, FMoverCVDSimDataWrapper::WrapperTypeName);
}

} // namespace UE::MoverUtils

#endif // WITH_CHAOS_VISUAL_DEBUGGER

