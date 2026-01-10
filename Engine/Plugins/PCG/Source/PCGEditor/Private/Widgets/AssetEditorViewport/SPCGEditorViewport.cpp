// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetEditorViewport/SPCGEditorViewport.h"

#include "AdvancedPreviewScene.h"
#include "AssetViewerSettings.h"
#include "EditorViewportClient.h"
#include "ToolMenus.h"
#include "ViewportToolbar/UnrealEdViewportToolbar.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"

namespace PCGEditorViewportConstants
{
	static const FVector DefaultViewLocation = FVector::ZeroVector;
	static const FRotator DefaultViewRotation = FRotator(-25.0f, -135.0f, 0.0f);
	static const float DefaultOrbitDistance = 500.0f;
}

class FPCGEditorViewportClient : public FEditorViewportClient
{
public:
	FPCGEditorViewportClient(const TSharedRef<SPCGEditorViewport>& InAssetEditorViewport);

	//~ Begin FViewportClient Interface
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputAxis(const FInputKeyEventArgs& Args) override;
	//~ End FViewportClient Interface

	//~ Begin FEditorViewportClient Interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End FEditorViewportClient Interface

	FAdvancedPreviewScene* GetPreviewScene() { return Scene.Get(); }
	FAdvancedPreviewScene* ResetScene();

private:
	TSharedPtr<FAdvancedPreviewScene> Scene = nullptr;
};

FPCGEditorViewportClient::FPCGEditorViewportClient(const TSharedRef<SPCGEditorViewport>& InAssetEditorViewport)
	: FEditorViewportClient(nullptr, nullptr, StaticCastSharedRef<SEditorViewport>(InAssetEditorViewport))
{
	ResetScene();

	bUsesDrawHelper = true;

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(80, 80, 80);
	DrawHelper.GridColorMajor = FColor(72, 72, 72);
	DrawHelper.GridColorMinor = FColor(64, 64, 64);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;

	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.SetSnap(0);
	EngineShowFlags.SetSeparateTranslucency(true);
}

bool FPCGEditorViewportClient::InputKey(const FInputKeyEventArgs& Args)
{
	if (!Scene.IsValid())
	{
		return true;
	}

	bool bHandled = FEditorViewportClient::InputKey(Args);
	bHandled |= InputTakeScreenshot(Args.Viewport, Args.Key, Args.Event);
	bHandled |= Scene->HandleInputKey(Args);

	return bHandled;
}

bool FPCGEditorViewportClient::InputAxis(const FInputKeyEventArgs& Args)
{
	if (!Scene.IsValid())
	{
		return true;
	}

	bool bResult = true;

	if (!bDisableInput)
	{
		bResult = Scene->HandleViewportInput(Args.Viewport, Args.InputDevice, Args.Key, Args.AmountDepressed, Args.DeltaTime, Args.NumSamples, Args.IsGamepad());

		if (bResult)
		{
			Invalidate();
		}
		else
		{
			bResult = FEditorViewportClient::InputAxis(Args);
		}
	}

	return bResult;
}

FLinearColor FPCGEditorViewportClient::GetBackgroundColor() const
{
	return Scene.IsValid() ? Scene->GetBackgroundColor() : FColor(64, 64, 64);
}

void FPCGEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	if (!GIntraFrameDebuggingGameThread && Scene.IsValid() && Scene->GetWorld())
	{
		Scene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

FAdvancedPreviewScene* FPCGEditorViewportClient::ResetScene()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGEditorViewportClient::ResetScene);

	Scene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	PreviewScene = Scene.Get();

	// Restore last used feature level.
	if (UWorld* World = Scene->GetWorld())
	{
		World->ChangeFeatureLevel(GWorld->GetFeatureLevel());
	}

	SetViewportType(ELevelViewportType::LVT_Perspective);
	SetViewMode(EViewModeIndex::VMI_Lit);
	SetViewLocation(PCGEditorViewportConstants::DefaultViewLocation);
	SetViewRotation(PCGEditorViewportConstants::DefaultViewRotation);
	SetViewLocationForOrbiting(PCGEditorViewportConstants::DefaultViewLocation, PCGEditorViewportConstants::DefaultOrbitDistance);

	Scene->SetFloorOffset(0.0f);
	Scene->SetFloorVisibility(true);
	Scene->SetEnvironmentVisibility(true);

	Invalidate();

	return Scene.Get();
}

SPCGEditorViewport::~SPCGEditorViewport()
{
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = nullptr;
	}

	ReleaseManagedResources();
}

void SPCGEditorViewport::Construct(const FArguments& InArgs)
{
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

TSharedRef<class SEditorViewport> SPCGEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SPCGEditorViewport::GetExtenders() const
{
	return MakeShareable(new FExtender);
}

void SPCGEditorViewport::SetupScene(const TArray<UObject*>& InResources, const FPCGSetupSceneFunc& SetupFunc)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SPCGEditorViewport::SetupScene);

	ResetScene();

	if (!ensure(EditorViewportClient.IsValid()))
	{
		return;
	}

	FAdvancedPreviewScene* Scene = EditorViewportClient->GetPreviewScene();

	if (Scene && SetupFunc.IsSet())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SPCGEditorViewport::SetupSceneCallback);

		FPCGSceneSetupParams SceneSetupParams;
		SceneSetupParams.Scene = Scene;
		SceneSetupParams.EditorViewportClient = EditorViewportClient.Get();
		SceneSetupParams.Resources = InResources;

		SetupFunc(SceneSetupParams);

		ManagedResources = MoveTemp(SceneSetupParams.ManagedResources);

		EditorViewportClient->Invalidate();
	}
}

void SPCGEditorViewport::ResetScene()
{
	ReleaseManagedResources();
	
	if (!ensure(EditorViewportClient.IsValid()))
	{
		return;
	}

	EditorViewportClient->ResetScene();
}

TSharedRef<FEditorViewportClient> SPCGEditorViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable(new FPCGEditorViewportClient(SharedThis(this)));
	EditorViewportClient->SetViewLocation(PCGEditorViewportConstants::DefaultViewLocation);
	EditorViewportClient->SetViewRotation(PCGEditorViewportConstants::DefaultViewRotation);
	EditorViewportClient->SetViewLocationForOrbiting(PCGEditorViewportConstants::DefaultViewLocation, PCGEditorViewportConstants::DefaultOrbitDistance);
	EditorViewportClient->bSetListenerPosition = false;
	EditorViewportClient->EngineShowFlags.EnableAdvancedFeatures();
	EditorViewportClient->EngineShowFlags.SetGrid(false);
	EditorViewportClient->EngineShowFlags.SetLighting(true);
	EditorViewportClient->EngineShowFlags.SetIndirectLightingCache(true);
	EditorViewportClient->EngineShowFlags.SetPostProcessing(true);
	EditorViewportClient->Invalidate();

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SPCGEditorViewport::BuildViewportToolbar()
{
	const FName ToolbarName = "PCG.ViewportToolbar";
	
	if (!UToolMenus::Get()->IsMenuRegistered(ToolbarName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(ToolbarName, /*Parent=*/NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		Menu->StyleName = "ViewportToolbar";
		Menu->AddSection("Left");
		
		FToolMenuSection& RightSection = Menu->AddSection("Right");
		RightSection.Alignment = EToolMenuSectionAlign::Last;
		RightSection.AddEntry(UE::UnrealEd::CreateCameraSubmenu(UE::UnrealEd::FViewportCameraMenuOptions().ShowAll()));
		RightSection.AddEntry(UE::UnrealEd::CreateViewModesSubmenu());
		RightSection.AddEntry(UE::UnrealEd::CreateDefaultShowSubmenu());
	}
	
	FToolMenuContext Context;
	Context.AppendCommandList(GetCommandList());
	Context.AddExtender(GetExtenders());
	Context.AddObject(UE::UnrealEd::CreateViewportToolbarDefaultContext(GetViewportWidget()));
	
	return UToolMenus::Get()->GenerateWidget(ToolbarName, Context);
}

void SPCGEditorViewport::ReleaseManagedResources()
{
	ManagedResources.Empty();
}

void SPCGEditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ManagedResources);
}
