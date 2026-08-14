// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
CustomizableObjectMeshUpdate.cpp: Helpers to stream in CustomizableObject skeletal mesh LODs.
=============================================================================*/


#include "MuCO/CustomizableObjectMeshUpdate.h"
#include "Components/SkinnedMeshComponent.h"

#include "MutableStreamRequest.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/CustomizableObjectSkeletalMesh.h"
#include "MuCO/UnrealConversionUtils.h"

#include "MuR/Model.h"
#include "MuR/MeshBufferSet.h"

#include "Engine/SkeletalMesh.h"
#include "Streaming/RenderAssetUpdate.inl"
#include "Rendering/SkeletalMeshRenderData.h"


template class TRenderAssetUpdate<FSkelMeshUpdateContext>;

#define UE_MUTABLE_UPDATE_MESH_REGION		TEXT("Task_Mutable_UpdateMesh")

static bool bEnableGCHangFix = true;
FAutoConsoleVariableRef CVarMutableEnableGCHangFix(
	TEXT("mutable.EnableGCHangFix"),
	bEnableGCHangFix,
	TEXT("Fix hang when FCustomizableObjectMeshStreamIn is canceled and TaskSynchronization is higher than 0.")
	TEXT("If true, TaskSynchronization decrement will happen in the Abort method instead of DoCancelMeshUpdate.")
);

FCustomizableObjectMeshStreamIn::FCustomizableObjectMeshStreamIn(
	const UCustomizableObjectSkeletalMesh* InMesh,
	EThreadType CreateResourcesThread,
	const TSharedPtr<FModelStreamableBulkData>& ModelStreamableBulkData) :
	FSkeletalMeshStreamIn(InMesh, CreateResourcesThread)
{
	OperationData = MakeShared<FMutableMeshOperationData>();

	// This must run in the mutable thread.
	check(UCustomizableObjectSystem::IsCreated());
	OperationData->System = UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem;

	OperationData->Model = InMesh->Model;
	OperationData->Parameters = InMesh->Parameters;
	OperationData->State = InMesh->State;

	OperationData->ModelStreamableBulkData = ModelStreamableBulkData;
	
	OperationData->MeshIDs = InMesh->MeshIDs;
	OperationData->Meshes.SetNum(InMesh->MeshIDs.Num());

	OperationData->CurrentFirstLODIdx = CurrentFirstLODIdx;
	OperationData->PendingFirstLODIdx = PendingFirstLODIdx;

	PushTask(FContext(InMesh, TT_None), TT_Async, SRA_UPDATE_CALLBACK(DoInitiate), TT_None, nullptr);
}

void FCustomizableObjectMeshStreamIn::OnUpdateMeshFinished()
{
	if (!IsCancelled() || !bEnableGCHangFix)
	{
		check(TaskSynchronization.GetValue() > 0)

		// At this point task synchronization would hold the number of pending requests.
		TaskSynchronization.Decrement();

		// The tick here is intended to schedule the success or cancel callback.
		// Using TT_None ensure gets which could create a dead lock.
		Tick(FSkeletalMeshUpdate::TT_None);
	}
}

void FCustomizableObjectMeshStreamIn::Abort()
{
	if (!IsCancelled() && !IsCompleted() && bEnableGCHangFix)
	{
		FSkeletalMeshStreamIn::Abort();

		// At this point task synchronization might hold the number of pending requests.
		TaskSynchronization.Set(0);

		if (MutableTaskId != FMutableTaskGraph::INVALID_ID && UCustomizableObjectSystem::IsCreated())
		{
			if (UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate())
			{
				// Cancel task if not launched yet.
				CustomizableObjectSystem->MutableTaskGraph.CancelMutableThreadTaskLowPriority(MutableTaskId);
			}
		}
	}
	else
	{
		FSkeletalMeshStreamIn::Abort();
	}
}

void FCustomizableObjectMeshStreamIn::DoInitiate(const FContext& Context)
{
	check(Context.CurrentThread == TT_Async);

	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectMeshStreamIn::DoInitiate)
	
	// Launch MutableTask
	RequestMeshUpdate(Context);

	if (bEnableGCHangFix)
	{
		PushTask(Context, TT_Async, SRA_UPDATE_CALLBACK(DoConvertResources), TT_Async, SRA_UPDATE_CALLBACK(DoCancel));
	}
	else
	{
		PushTask(Context, TT_Async, SRA_UPDATE_CALLBACK(DoConvertResources), TT_Async, SRA_UPDATE_CALLBACK(DoCancelMeshUpdate));
	}
}

void FCustomizableObjectMeshStreamIn::DoConvertResources(const FContext& Context)
{
	check(Context.CurrentThread == TT_Async);

	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectMeshStreamIn::DoConvertResources)

	bool bMarkRenderStateDirty = false;
	ConvertMesh(Context, bMarkRenderStateDirty);

	if (bMarkRenderStateDirty)
	{
		PushTask(Context, TT_GameThread, SRA_UPDATE_CALLBACK(MarkRenderStateDirty), TT_None, SRA_UPDATE_CALLBACK(DoCancel));
	}
	else
	{
		PushTask(Context, CreateResourcesThread, SRA_UPDATE_CALLBACK(DoCreateBuffers), static_cast<EThreadType>(Context.CurrentThread), SRA_UPDATE_CALLBACK(DoCancel));
	}
}

void FCustomizableObjectMeshStreamIn::DoCreateBuffers(const FContext& Context)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectMeshStreamIn::DoCreateBuffers)

	CreateBuffers(Context);

	check(!TaskSynchronization.GetValue());

	// We cannot cancel once DoCreateBuffers has started executing, as there's an RHICmdList that must be submitted.
	// Pass the same callback for both task and cancel.
	PushTask(Context, 
		TT_Render, SRA_UPDATE_CALLBACK(DoFinishUpdate), 
		TT_Render, SRA_UPDATE_CALLBACK(DoFinishUpdate));
}

void FCustomizableObjectMeshStreamIn::DoCancelMeshUpdate(const FContext& Context)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectMeshStreamIn::DoCancelMeshUpdate)

	CancelMeshUpdate(Context);
	PushTask(Context, TT_None, nullptr, (EThreadType)Context.CurrentThread, SRA_UPDATE_CALLBACK(DoCancel));
}


namespace impl
{
	void Task_Mutable_UpdateMesh_End(const TSharedPtr<FMutableMeshOperationData> OperationData, TRefCountPtr<FCustomizableObjectMeshStreamIn>& Task, mu::FInstance::FID InstanceID)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateMesh_End);

		// End update
		OperationData->System->EndUpdate(InstanceID);
		OperationData->System->ReleaseInstance(InstanceID);

		if (UCustomizableObjectSystem::ShouldClearWorkingMemoryOnUpdateEnd())
		{
			OperationData->System->ClearWorkingMemory();
		}

		OperationData->Event.Trigger();
		
		TRACE_END_REGION(UE_MUTABLE_UPDATE_MESH_REGION);
	}

	void Task_Mutable_UpdateMesh_Loop(
		const TSharedPtr<FMutableMeshOperationData> OperationData,
		TRefCountPtr<FCustomizableObjectMeshStreamIn>& Task,
		mu::FInstance::FID InstanceID,
		int32 LODIndex)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateMesh_Loop);

		if (Task->IsCancelled() || LODIndex == OperationData->CurrentFirstLODIdx + OperationData->AssetLODBias)
		{
			Task_Mutable_UpdateMesh_End(OperationData, Task, InstanceID);
			return;
		}

		const mu::FResourceID MeshID = OperationData->MeshIDs[LODIndex];

		constexpr mu::EMeshContentFlags MeshContentFilter = mu::EMeshContentFlags::AllFlags;
		UE::Tasks::TTask<TSharedPtr<const mu::FMesh>> GetMeshTask = OperationData->System->GetMesh(InstanceID, MeshID, MeshContentFilter);

		UE::Tasks::AddNested(UE::Tasks::Launch(TEXT("Task_MutableGetMeshes_GetMesh_Post"), [=]() mutable
			{
				OperationData->Meshes[LODIndex] = GetMeshTask.GetResult();

				Task_Mutable_UpdateMesh_Loop(OperationData, Task, InstanceID, LODIndex + 1);
			},
			GetMeshTask,
			LowLevelTasks::ETaskPriority::Inherit));
	}

	void Task_Mutable_UpdateMesh(const TSharedPtr<FMutableMeshOperationData> OperationData, TRefCountPtr<FCustomizableObjectMeshStreamIn>& Task)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateMesh);

		if (Task->IsCancelled() && bEnableGCHangFix)
		{
			return;
		}

		TRACE_BEGIN_REGION(UE_MUTABLE_UPDATE_MESH_REGION);

		TSharedPtr<mu::FSystem> System = OperationData->System;
		const TSharedPtr<mu::FModel, ESPMode::ThreadSafe> Model = OperationData->Model;

#if WITH_EDITOR
		// Recompiling a CO in the editor will invalidate the previously generated Model. Check that it is valid before accessing the streamed data.
		if (!Model || !Model->IsValid())
		{
			TRACE_END_REGION(UE_MUTABLE_UPDATE_MESH_REGION);
			Task->Abort();
			return;
		}
#endif

		// For now, we are forcing the recreation of mutable-side instances with every update.
		mu::FInstance::FID InstanceID = System->NewInstance(Model);
		UE_LOG(LogMutable, Verbose, TEXT("Creating Mutable instance with id [%d] for a mesh update"), InstanceID);

		// LOD mask, set to all ones to build all LODs
		const uint32 LODMask = 0xFFFFFFFF;

		// Main instance generation step
		TSharedPtr<const mu::FInstance> Instance = System->BeginUpdate(InstanceID, OperationData->Parameters, OperationData->State, LODMask);
		check(Instance);

		Task_Mutable_UpdateMesh_Loop(OperationData, Task, InstanceID, OperationData->PendingFirstLODIdx + OperationData->AssetLODBias);
	}
	
} // namespace


void FCustomizableObjectMeshStreamIn::RequestMeshUpdate(const FContext& Context)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectMeshStreamIn::RequestMeshUpdate)
	
	FSoftObjectPath Path(Cast<UCustomizableObjectSkeletalMesh>(Context.Mesh)->CustomizableObjectPathName);
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*Path.GetAssetName())

	if (IsCancelled())
	{
		return;
	}

	if (!UCustomizableObjectSystem::IsActive())
	{
		Abort();
		return;
	}

	check(OperationData.IsValid());

#if WITH_EDITOR
	// Recompiling a CO in the editor will invalidate the previously generated Model. Check that it is valid before accessing the streamed data.
	if (!OperationData->Model || !OperationData->Model->IsValid())
	{
		Abort();
		return;
	}
#endif

	OperationData->AssetLODBias = Context.AssetLODBias;

	UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
	
	TaskSynchronization.Increment();

	MutableTaskId = CustomizableObjectSystem->MutableTaskGraph.AddMutableThreadTaskLowPriority(
		TEXT("Mutable_MeshUpdate"),
		[SharedOperationData = OperationData, RefThis = TRefCountPtr<FCustomizableObjectMeshStreamIn>(this)]() mutable
		{
			impl::Task_Mutable_UpdateMesh(SharedOperationData, RefThis);
		});

	// Stream data (morphs, clothing...).
	UE::Tasks::TTask StreamData = UE::Tasks::Launch(TEXT("StreamData"),
		[RefThis = TRefCountPtr(this)]()
		{
			FMutableStreamRequest StreamRequest(RefThis->OperationData->ModelStreamableBulkData);

			// Get morphs to be streamed.
			for (TSharedPtr<const mu::FMesh>& Mesh : RefThis->OperationData->Meshes)
			{
				if (!Mesh)
				{
					continue;
				}

				LoadMorphTargetsData(StreamRequest, Mesh.ToSharedRef(), RefThis->OperationData->MorphTargetMeshData);
				LoadMorphTargetsMetadata(StreamRequest, Mesh.ToSharedRef(), RefThis->OperationData->MorphTargetMeshData);
				
				LoadClothing(StreamRequest, Mesh.ToSharedRef(), RefThis->OperationData->ClothingMeshData);
			}

			// Stream data.
			UE::Tasks::AddNested(StreamRequest.Stream());
		},
		OperationData->Event,
		UE::Tasks::ETaskPriority::Inherit);

	// Go to next step.
	UE::Tasks::Launch(TEXT("OnUpdateMeshFinished"),
		[RefThis = TRefCountPtr(this)]()
		{
			RefThis->OnUpdateMeshFinished();
		},
		StreamData,
		UE::Tasks::ETaskPriority::Inherit,
		UE::Tasks::EExtendedTaskPriority::Inline);
	
	if (IsCancelled() && TaskSynchronization.GetValue() > 0 && bEnableGCHangFix)
	{
		TaskSynchronization.Set(0);
	}
}


void FCustomizableObjectMeshStreamIn::CancelMeshUpdate(const FContext& Context)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectMeshStreamIn::CancelMeshUpdate)
	
	UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
	if (CustomizableObjectSystem && MutableTaskId != FMutableTaskGraph::INVALID_ID)
	{
		// Cancel task if not launched yet.
		const bool bMutableTaskCancelledBeforeRun = CustomizableObjectSystem->MutableTaskGraph.CancelMutableThreadTaskLowPriority(MutableTaskId);
		if (bMutableTaskCancelledBeforeRun)
		{
			// Clear MeshUpdate data
			OperationData = nullptr;

			// At this point task synchronization would hold the number of pending requests.
			TaskSynchronization.Decrement();
			check(TaskSynchronization.GetValue() == 0);
		}
	}
	else
	{
		check(TaskSynchronization.GetValue() == 0);
	}

	// The tick here is intended to schedule the success or cancel callback.
	// Using TT_None ensure gets which could create a dead lock.
	Tick(FSkeletalMeshUpdate::TT_None);
}


void FCustomizableObjectMeshStreamIn::ConvertMesh(const FContext& Context, bool& bOutMarkRenderStateDirty)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectMeshStreamIn::ConvertMesh);

	check(!TaskSynchronization.GetValue());

	const UCustomizableObjectSkeletalMesh* Mesh = Cast<UCustomizableObjectSkeletalMesh>(Context.Mesh);
	FSkeletalMeshRenderData* RenderData = Context.RenderData;
	if (IsCancelled() || !Mesh || !RenderData)
	{
		return;
	}

	for (int32 LODIndex = PendingFirstLODIdx; LODIndex < CurrentFirstLODIdx; ++LODIndex)
	{
		TSharedPtr<const mu::FMesh> MutableMesh = OperationData->Meshes[LODIndex + Context.AssetLODBias];

		if (!MutableMesh)
		{
			check(false);
			Abort();
			return;
		}

		if (MutableMesh->GetVertexCount() == 0 || MutableMesh->GetSurfaceCount() == 0 || MutableMesh->GetVertexBuffers().IsDescriptor())
		{
			check(false);
			Abort();
			return;
		}

		const bool bNeedsCPUAccess = Mesh->GetResourceForRendering()->RequiresCPUSkinning(GMaxRHIFeatureLevel) || Mesh->NeedCPUData(LODIndex);

		FSkeletalMeshLODRenderData& LODResource = *Context.LODResourcesView[LODIndex];
		UnrealConversionUtils::CopyMutableVertexBuffers(LODResource, MutableMesh.Get(), bNeedsCPUAccess);
		UnrealConversionUtils::CopyMutableIndexBuffers(LODResource, MutableMesh.Get(), Mesh->SurfaceIDs[LODIndex], bOutMarkRenderStateDirty);
		UnrealConversionUtils::CopyMutableSkinWeightProfilesBuffers(LODResource, *const_cast<UCustomizableObjectSkeletalMesh*>(Mesh), LODIndex, MutableMesh.Get(), Mesh->SkinWeightProfileIDs);
		UnrealConversionUtils::MorphTargetVertexInfoBuffers(LODResource, *Mesh, *MutableMesh.Get(), OperationData->MorphTargetMeshData, LODIndex);
		UnrealConversionUtils::ClothVertexBuffers(LODResource, *MutableMesh.Get(), OperationData->ClothingMeshData, LODIndex);
		
		UnrealConversionUtils::UpdateSkeletalMeshLODRenderDataBuffersSize(LODResource);
	}

	// Clear MeshUpdate data
	OperationData = nullptr;
}


void FCustomizableObjectMeshStreamIn::MarkRenderStateDirty(const FContext& Context)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectMeshStreamIn::ModifyRenderData);

	check(Context.CurrentThread == TT_GameThread);
	
	const USkeletalMesh* Mesh = Context.Mesh;
	FSkeletalMeshRenderData* RenderData = Context.RenderData;
	
	if (!IsCancelled() && Mesh && RenderData)
	{
		TArray<const UPrimitiveComponent*> Components;
		IStreamingManager::Get().GetRenderAssetStreamingManager().GetAssetComponents(Mesh, Components);

		for (const UPrimitiveComponent* ConstComponent : Components)
		{
			UPrimitiveComponent* Component = const_cast<UPrimitiveComponent*>(ConstComponent);
			CastChecked<USkinnedMeshComponent>(Component)->MarkRenderStateDirty();
		}
	}
	else
	{
		Abort();
	}
	
	PushTask(Context, CreateResourcesThread, SRA_UPDATE_CALLBACK(DoCreateBuffers), (EThreadType)Context.CurrentThread, SRA_UPDATE_CALLBACK(DoCancel));
}