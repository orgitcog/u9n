// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoStreamingModule.h"
#include "FastGeoPrimitiveComponent.h"
#include "FastGeoStaticMeshComponent.h"
#include "FastGeoInstancedStaticMeshComponent.h"
#include "FastGeoWorldSubsystem.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FFastGeoStreamingModule, FastGeoStreaming);

void FFastGeoStreamingModule::StartupModule()
{
#if WITH_EDITOR
	IPrimitiveComponent::AddImplementer({ UFastGeoStaticMeshComponentEditorProxy::StaticClass(),  [](UObject* Obj)
	{
		return static_cast<IPrimitiveComponent*>(static_cast<UFastGeoPrimitiveComponentEditorProxy*>(Obj));
	}});

	IStaticMeshComponent::AddImplementer({ UFastGeoStaticMeshComponentEditorProxy::StaticClass(), [](UObject* Obj)
	{
		return static_cast<IStaticMeshComponent*>(static_cast<UFastGeoStaticMeshComponentEditorProxy*>(Obj));
	}});

	IPrimitiveComponent::AddImplementer({ UFastGeoInstancedStaticMeshComponentEditorProxy::StaticClass(), [](UObject* Obj)
	{
		return static_cast<IPrimitiveComponent*>(static_cast<UFastGeoPrimitiveComponentEditorProxy*>(Obj));
	}});

	IStaticMeshComponent::AddImplementer({ UFastGeoInstancedStaticMeshComponentEditorProxy::StaticClass(), [](UObject* Obj)
	{
		return static_cast<IStaticMeshComponent*>(static_cast<UFastGeoStaticMeshComponentEditorProxy*>(Obj));
	}});
#endif

	Handle_OnWorldPreSendAllEndOfFrameUpdates = FWorldDelegates::OnWorldPreSendAllEndOfFrameUpdates.AddRaw(this, &FFastGeoStreamingModule::OnWorldPreSendAllEndOfFrameUpdates);
}

void FFastGeoStreamingModule::ShutdownModule()
{
#if WITH_EDITOR
	IPrimitiveComponent::RemoveImplementer(UFastGeoStaticMeshComponentEditorProxy::StaticClass());
	IStaticMeshComponent::RemoveImplementer(UFastGeoStaticMeshComponentEditorProxy::StaticClass());
	IPrimitiveComponent::RemoveImplementer(UFastGeoInstancedStaticMeshComponentEditorProxy::StaticClass());
	IStaticMeshComponent::RemoveImplementer(UFastGeoInstancedStaticMeshComponentEditorProxy::StaticClass());
#endif

	FWorldDelegates::OnWorldPreSendAllEndOfFrameUpdates.Remove(Handle_OnWorldPreSendAllEndOfFrameUpdates);
}

void FFastGeoStreamingModule::OnWorldPreSendAllEndOfFrameUpdates(UWorld* InWorld)
{
	if (UFastGeoWorldSubsystem* WorldSubsystem = InWorld->GetSubsystem<UFastGeoWorldSubsystem>())
	{
		WorldSubsystem->ProcessPendingRecreate();
	}
}
