// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorDragTools/EditorViewportClientProxy.h"
#include "GameFramework/Volume.h"
#include "LevelEditorViewport.h"
#include "UnrealWidget.h"

IEditorViewportClientProxy* IEditorViewportClientProxy::CreateViewportClientProxy(FEditorViewportClient* InViewportClient)
{
	if (InViewportClient->IsLevelEditorClient())
	{
		if (FLevelEditorViewportClient* LevelEditorViewportClient = static_cast<FLevelEditorViewportClient*>(InViewportClient))
		{
			return new FLevelEditorViewportClientProxy(LevelEditorViewportClient);
		}
	}

	return new FEditorViewportClientProxy(InViewportClient);
}

bool IEditorViewportClientProxy::IsMouseOnWidgetAxis(IEditorViewportClientProxy* InEditorViewportClientProxy)
{
	if (InEditorViewportClientProxy)
	{
		if (FEditorViewportClient* const EditorViewportClient = InEditorViewportClientProxy->GetEditorViewportClient())
		{
			if (FViewport* const Viewport = EditorViewportClient->Viewport)
			{
				FIntPoint MousePos;
				EditorViewportClient->Viewport->GetMousePos(MousePos);

				// Since some drag tool does not involve any keyboard modifier (Alt, Shift, Ctrl),
				// in some cases we need to make sure the user is not hovering over widget axis proxies.
				// Intercepting input in that case prevents TRS gizmos from working.
				if (const HHitProxy* const Proxy = Viewport->GetHitProxy(MousePos.X, MousePos.Y))
				{
					// We hit a widget axis, so we don't start the drag sequence
					if (Proxy->IsA(HWidgetAxis::StaticGetType()))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

FEditorViewportClientProxy::FEditorViewportClientProxy(FEditorViewportClient* InEditorViewportClient)
	: EditorViewportClient(InEditorViewportClient)
{
}

bool FEditorViewportClientProxy::IsActorVisible(const AActor* InActor) const
{
	if (InActor)
	{
		return !InActor->IsA(AVolume::StaticClass());
	}

	return false;
}

const TArray<FName> FEditorViewportClientProxy::GetHiddenLayers() const
{
	return TArray<FName>();
}

FEditorViewportClient* FEditorViewportClientProxy::GetEditorViewportClient()
{
	return EditorViewportClient.Get();
}

FLevelEditorViewportClientProxy::FLevelEditorViewportClientProxy(FLevelEditorViewportClient* InLevelEditorViewportClient)
	:LevelEditorViewportClient(InLevelEditorViewportClient)
{
}

bool FLevelEditorViewportClientProxy::IsActorVisible(const AActor* InActor) const
{
	if (InActor && LevelEditorViewportClient)
	{
		return !InActor->IsA(AVolume::StaticClass()) || !LevelEditorViewportClient->IsVolumeVisibleInViewport(*InActor);
	}

	return false;
}

const TArray<FName> FLevelEditorViewportClientProxy::GetHiddenLayers() const
{
	if (LevelEditorViewportClient)
	{
		return LevelEditorViewportClient->ViewHiddenLayers;
	}

	return TArray<FName>();
}

FEditorViewportClient* FLevelEditorViewportClientProxy::GetEditorViewportClient()
{
	return LevelEditorViewportClient.Get();
}
