// Copyright Epic Games, Inc.All Rights Reserved.

#include "MetaHumanPerformanceControlRigViewportManager.h"
#include "MetaHumanPerformanceControlRigViewportClient.h"
#include "MetaHumanPerformanceControlRigComponent.h"

#include "EditorViewportTabContent.h"
#include "SAssetEditorViewport.h"
#include "AssetEditorModeManager.h"
#include "ControlRigBlueprint.h"
#include "ControlRigGizmoActor.h"
#include "PreviewScene.h"

FMetaHumanPerformanceControlRigViewportManager::FMetaHumanPerformanceControlRigViewportManager()
{
	// Performs the initializations for things required to drive the control rig viewport
	PreviewScene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues{});

	EditorModeManager = MakeShared<FAssetEditorModeManager>();
	EditorModeManager->SetPreviewScene(PreviewScene.Get());

	ViewportTabContent = MakeShared<FEditorViewportTabContent>();
	ViewportClient = MakeShared<FMetaHumanPerformanceControlRigViewportClient>(EditorModeManager.Get(), PreviewScene.Get());
	ViewportDelegate = [this](FAssetEditorViewportConstructionArgs InArgs)
	{
		return SNew(SAssetEditorViewport, InArgs)
			.EditorViewportClient(ViewportClient);
	};

	ControlRigComponent = NewObject<UMetaHumanPerformanceControlRigComponent>();
	PreviewScene->AddComponent(ControlRigComponent, FTransform::Identity);
}

void FMetaHumanPerformanceControlRigViewportManager::SetControlRig(UControlRig* InControlRig)
{
	check(ControlRigComponent);

	// This will spawn the control rig shapes in the scene
	// If nullptr, this will clear existing shape actors from the scene
	ControlRigComponent->SetControlRig(InControlRig);

	if (InControlRig != nullptr)
	{
		const FBox ControlRigShapesBoundingBox = ControlRigComponent->GetShapesBoundingBox();

		// Calculate the distance in the Y axis to make the control rig fill the entire viewport
		FVector Center, Extents;
		ControlRigShapesBoundingBox.GetCenterAndExtents(Center, Extents);

		const float HalfFOVRadians = FMath::DegreesToRadians(ViewportClient->ViewFOV / 2.0f);
		const float CameraOffsetY = Extents.GetMax() / FMath::Tan(HalfFOVRadians);
		ViewportClient->SetViewLocation(FVector{ Center.X, CameraOffsetY, Center.Z });
	}
}

void FMetaHumanPerformanceControlRigViewportManager::SetFaceBoardShapeColor( FLinearColor InColor )
{
	//Set the color of ControlRig lines to Slate's foreground color so they don't blend into the background
	if (ControlRigComponent->ControlRig != nullptr)
	{
		if (ControlRigOnExecuteDelegateHandle.IsValid()) //already bound
		{
			ControlRigComponent->ControlRig->OnExecuted_AnyThread().Remove(ControlRigOnExecuteDelegateHandle);
		}
		ControlRigOnExecuteDelegateHandle = ControlRigComponent->ControlRig->OnExecuted_AnyThread().AddLambda([this, InColor](class URigVMHost*, const FName& InEventName)
			{
				if (ControlRigComponent && ControlRigComponent->ControlRig)
				{
					if (InEventName == TEXT("Construction"))
					{
						URigHierarchy* Hierarchy = ControlRigComponent->ControlRig->GetHierarchy();
						if (Hierarchy)
						{
							FRigElementKey Key = FRigElementKey("MH_FACIAL_BOARD", ERigElementType::Control);
							FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key);
							if (ControlElement)
							{
								ControlElement->Settings.ShapeColor = InColor;
								Hierarchy->Notify(ERigHierarchyNotification::ControlSettingChanged, ControlElement);
							}
						}
					}
				}
			}
		);
	}
}

void FMetaHumanPerformanceControlRigViewportManager::UpdateControlRigShapes()
{
	check(ControlRigComponent);
	ControlRigComponent->UpdateControlRigShapes();
}

void FMetaHumanPerformanceControlRigViewportManager::InitializeControlRigTabContents(TSharedRef<SDockTab> InControlRigTab)
{
	check(ViewportTabContent);
	const FString LayoutId = TEXT("ControlRigViewport");
	ViewportTabContent->Initialize(ViewportDelegate, InControlRigTab, LayoutId);
}