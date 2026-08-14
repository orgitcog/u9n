// Copyright Epic Games, Inc.All Rights Reserved.

#include "SMetaHumanEditorViewport.h"

#include "MetaHumanEditorViewportClient.h"
#include "MetaHumanToolkitCommands.h"

#include "Widgets/Layout/SBox.h"
#include "EditorViewportCommands.h"

void SMetaHumanEditorViewport::Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs)
{
	OnGetABViewMenuContentsDelegate = InArgs._OnGetABViewMenuContents;

	ABCommandList = InArgs._ABCommandList;

	TSharedPtr<FMetaHumanEditorViewportClient> ViewportClient = InArgs._ViewportClient;
	check(ViewportClient.IsValid());

	// Needs to be created before the call to SetEditorViewportWidget
	SAssignNew(TrackerImageViewer, SMetaHumanOverlayWidget<STrackerImageViewer>)
		.ShouldDrawCurves(this, &SMetaHumanEditorViewport::IsShowingCurvesForCurrentView)
		.ShouldDrawPoints(this, &SMetaHumanEditorViewport::IsShowingPointsForCurrentView);

	const bool bManagedTextures = true;
	TrackerImageViewer->Setup(bManagedTextures);
	TrackerImageViewer->OnInvalidate().AddLambda([this]
	{
		if (Client.IsValid())
		{
			Client->Invalidate();
		}
	});
	TrackerImageViewer->SetNavigationMode(EABImageNavigationMode::ThreeD);

	// Give the viewport client a reference to the viewport as we
	// can't pass it in the constructor due to restrictions on FBaseAssetToolkit
	ViewportClient->SetEditorViewportWidget(SharedThis(this));

	SAssetEditorViewport::Construct(SAssetEditorViewport::FArguments()
										.EditorViewportClient(ViewportClient),
									InViewportConstructionArgs);

	if (InArgs._Content.Widget != SNullWidget::NullWidget)
	{
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f)
			[
				ChildSlot.GetWidget()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(28) // This could be customized as a parameter if needed
				[
					InArgs._Content.Widget
				]
			]
		];
	}
}

TSharedRef<SMetaHumanOverlayWidget<STrackerImageViewer>> SMetaHumanEditorViewport::GetTrackerImageViewer() const
{
	return TrackerImageViewer.ToSharedRef();
}

void SMetaHumanEditorViewport::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SAssetEditorViewport::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (ViewportWidget.IsValid())
	{
		const FGeometry& ViewportWidgetGeometry = ViewportWidget->GetCachedGeometry();
		if (ViewportWidgetGeometry != CurrentViewportGeometry)
		{
			bool UpdateGeometry = ViewportWidgetGeometry.GetLocalSize() != CurrentViewportGeometry.GetLocalSize();
			CurrentViewportGeometry = ViewportWidgetGeometry;

			const FVector2D& ViewportSize = ViewportWidgetGeometry.GetLocalSize();
			if (UpdateGeometry && ViewportSize != FVector2D::ZeroVector)
			{
				// OnViewportSizeChangedDelegate.ExecuteIfBound(FIntPoint(ViewportSize.X, ViewportSize.Y));
				GetTrackerImageViewer()->ResetView();

				// TODO: This could be done via the above delegate
				GetMetaHumanViewportClient()->UpdateABVisibility();
			}
		}
	}
}

void SMetaHumanEditorViewport::BindCommands()
{
	SAssetEditorViewport::BindCommands();

	TSharedRef<FMetaHumanEditorViewportClient> ViewportClient = GetMetaHumanViewportClient();

	const FMetaHumanToolkitCommands& Commands = FMetaHumanToolkitCommands::Get();

	CommandList->MapAction(Commands.ToggleSingleViewToA,
						   FExecuteAction::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::ToggleABViews),
						   FCanExecuteAction::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::IsShowingSingleView),
						   FIsActionChecked::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::IsShowingViewA));
	CommandList->MapAction(Commands.ToggleSingleViewToB,
						   FExecuteAction::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::ToggleABViews),
						   FCanExecuteAction::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::IsShowingSingleView),
						   FIsActionChecked::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::IsShowingViewB));

	CommandList->MapAction(Commands.ViewMixToSingle,
						   FExecuteAction::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::SetABViewMode, EABImageViewMode::A),
						   FCanExecuteAction{},
						   FIsActionChecked::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::IsShowingSingleView));

	CommandList->MapAction(Commands.ViewMixToDual,
						   FExecuteAction::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::SetABViewMode, EABImageViewMode::ABSide),
						   FCanExecuteAction{},
						   FIsActionChecked::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::IsShowingDualView));

	CommandList->MapAction(Commands.ViewMixToWipe,
						   FExecuteAction::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::SetABViewMode, EABImageViewMode::ABSplit),
						   FCanExecuteAction{},
						   FIsActionChecked::CreateSP(ViewportClient, &FMetaHumanEditorViewportClient::IsShowingWipeView));

	const FEditorViewportCommands& ViewportCommands = FEditorViewportCommands::Get();

	ABCommandList.MapAction(Commands.ToggleUndistortion,
							ViewportClient,
							&FMetaHumanEditorViewportClient::ToggleDistortion,
							&FMetaHumanEditorViewportClient::CanExecuteAction,
							&FMetaHumanEditorViewportClient::IsShowingUndistorted);

	ABCommandList.MapAction(Commands.ToggleDepthMesh,
							ViewportClient,
							&FMetaHumanEditorViewportClient::ToggleDepthMeshVisible,
							&FMetaHumanEditorViewportClient::CanExecuteAction,
							&FMetaHumanEditorViewportClient::IsDepthMeshVisible);

	ABCommandList.MapAction(Commands.ToggleCurves,
							ViewportClient,
							&FMetaHumanEditorViewportClient::ToggleShowCurves,
							&FMetaHumanEditorViewportClient::CanToggleShowCurves,
							&FMetaHumanEditorViewportClient::IsShowingCurves);
	ABCommandList.MapAction(Commands.ToggleControlVertices,
							ViewportClient,
							&FMetaHumanEditorViewportClient::ToggleShowControlVertices,
							&FMetaHumanEditorViewportClient::CanToggleShowControlVertices,
							&FMetaHumanEditorViewportClient::IsShowingControlVertices);

	ABCommandList.MapAction(ViewportCommands.LitMode,
							ViewportClient,
							&FMetaHumanEditorViewportClient::SetViewModeIndex,
							&FMetaHumanEditorViewportClient::CanChangeViewMode,
							&FMetaHumanEditorViewportClient::IsViewModeIndexEnabled,
							VMI_Lit, true);
	ABCommandList.MapAction(ViewportCommands.UnlitMode,
							ViewportClient,
							&FMetaHumanEditorViewportClient::SetViewModeIndex,
							&FMetaHumanEditorViewportClient::CanChangeViewMode,
							&FMetaHumanEditorViewportClient::IsViewModeIndexEnabled,
							VMI_Unlit, true);
	ABCommandList.MapAction(ViewportCommands.LightingOnlyMode,
							ViewportClient,
							&FMetaHumanEditorViewportClient::SetViewModeIndex,
							&FMetaHumanEditorViewportClient::CanChangeViewMode,
							&FMetaHumanEditorViewportClient::IsViewModeIndexEnabled,
							VMI_LightingOnly, true);
}

void SMetaHumanEditorViewport::PopulateViewportOverlays(TSharedRef<SOverlay> InOverlay)
{
	InOverlay->AddSlot()
	[
		TrackerImageViewer.ToSharedRef()
	];

	// Usually this is done in the MakeViewportToolbar override but because STrackerImageViewer is an overlay that
	// covers the whole screen we need control over the order in which the overlays are stacked in this viewport
	InOverlay->AddSlot()
	.VAlign(VAlign_Top)
	[
		SNew(SMetaHumanEditorViewportToolBar)
			.ViewportCommandList(CommandList)
			.ABCommandList(ABCommandList)
			.ViewportClient(GetMetaHumanViewportClient())
			.OnGetABMenuContents(OnGetABViewMenuContentsDelegate)
	];
}

void SMetaHumanEditorViewport::OnFocusViewportToSelection()
{
	GetMetaHumanViewportClient()->FocusViewportOnSelection();
}

FReply SMetaHumanEditorViewport::OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = FReply::Unhandled();

	if (GetTrackerImageViewer()->IsSingleView())
	{
		const EABImageViewMode ImageViewMode = GetTrackerImageViewer()->GetViewMode();
		if (ImageViewMode == EABImageViewMode::A || ImageViewMode == EABImageViewMode::B)
		{
			if (ABCommandList.GetCommandList(ImageViewMode)->ProcessCommandBindings(InKeyEvent))
			{
				Reply = FReply::Handled();
			}
		}
	}

	if (!Reply.IsEventHandled())
	{
		// Try the viewport default command list
		if (CommandList->ProcessCommandBindings(InKeyEvent))
		{
			Reply = FReply::Handled();
		}
	}

	if (Reply.IsEventHandled())
	{
		Client->Invalidate();
	}

	return Reply;
}

bool SMetaHumanEditorViewport::IsShowingCurvesForCurrentView() const
{
	TSharedRef<FMetaHumanEditorViewportClient> ViewportClient = GetMetaHumanViewportClient();

	if (ViewportClient->IsShowingSingleView() && !ViewportClient->IsMovingCamera())
	{
		return ViewportClient->ShouldShowCurves(ViewportClient->GetABViewMode());
	}

	return false;
}

bool SMetaHumanEditorViewport::IsShowingPointsForCurrentView() const
{
	TSharedRef<FMetaHumanEditorViewportClient> ViewportClient = GetMetaHumanViewportClient();

	if (ViewportClient->IsShowingSingleView() && !ViewportClient->IsMovingCamera())
	{
		return ViewportClient->ShouldShowControlVertices(ViewportClient->GetABViewMode());
	}

	return false;
}

TSharedRef<FMetaHumanEditorViewportClient> SMetaHumanEditorViewport::GetMetaHumanViewportClient() const
{
	return StaticCastSharedPtr<FMetaHumanEditorViewportClient>(Client).ToSharedRef();
}