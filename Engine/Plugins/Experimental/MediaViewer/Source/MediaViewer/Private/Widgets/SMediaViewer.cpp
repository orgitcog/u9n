// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaViewer.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageViewer/MediaImageViewer.h"
#include "ImageViewers/NullImageViewer.h"
#include "IMediaViewerModule.h"
#include "Library/MediaViewerLibrary.h"
#include "Library/MediaViewerLibraryIni.h"
#include "Library/MediaViewerLibraryGroup.h"
#include "MediaViewerCommands.h"
#include "Misc/MessageDialog.h"
#include "Sidebar/SSidebar.h"
#include "Sidebar/SSidebarContainer.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/MediaViewerDelegates.h"
#include "Widgets/SMediaImageViewerOverlay.h"
#include "Widgets/SMediaViewerLibrary.h"
#include "Widgets/SMediaViewerLibraryPrivate.h"
#include "Widgets/SMediaViewerToolbar.h"

#define LOCTEXT_NAMESPACE "SMediaViewer"

namespace UE::MediaViewer::Private
{

const FSlateColorBrush SMediaViewer::BackgroundColorBrush(FLinearColor::Black);

SMediaViewer::SMediaViewer()
	: CommandList(MakeShared<FUICommandList>())
	, BackgroundTextureBrush(static_cast<UObject*>(nullptr), FVector2D(1), FLinearColor::White, ESlateBrushTileType::NoTile)
	, ViewerSize(FVector2D::ZeroVector)
	, ViewerPosition(FVector2D::ZeroVector)
	, bInvalidated(true)
{
}

SMediaViewer::~SMediaViewer()
{
}

void SMediaViewer::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SMediaViewer::Construct(const FArguments& InArgs, const TSharedRef<SMediaViewerTab>& InTab, const FMediaViewerArgs& InMediaViewerArgs,
	const TSharedRef<FMediaImageViewer>& InImageViewerFirst, const TSharedRef<FMediaImageViewer>& InImageViewerSecond)
{
	MediaViewerArgs = InMediaViewerArgs;
	Tab = InTab;
	ImageViewers[static_cast<int32>(EMediaImageViewerPosition::First)] = InImageViewerFirst;
	ImageViewers[static_cast<int32>(EMediaImageViewerPosition::Second)] = InImageViewerSecond;
	ActiveView = EMediaImageViewerActivePosition::Single;
	RequestedView = EMediaImageViewerActivePosition::Single;
	ScaleToFit[static_cast<int32>(EMediaImageViewerPosition::First)] = false;
	ScaleToFit[static_cast<int32>(EMediaImageViewerPosition::Second)] = false;
	ContentSlot = nullptr;
	CursorLocalPosition = {0, 0};

	BindCommands();
	CreateDelegates();

	Library = SNew(SMediaViewerLibraryPrivate, Delegates.ToSharedRef());

	Layout = SNew(SVerticalBox);

	if (MediaViewerArgs.bShowToolbar)
	{
		Layout->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.AutoHeight()
			.Expose(ToolbarSlot)
			[
				SNullWidget::NullWidget
			];
	}

	Layout->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillHeight(1.f)
		.Expose(ContentSlot)
		[
			SNullWidget::NullWidget
		];

	if (!MediaViewerArgs.bShowSidebar)
	{
		ChildSlot
		[
			Layout.ToSharedRef()
		];

		CreateView();
		return;
	}

	TSharedRef<SSidebarContainer> SidebarContainer = SNew(SSidebarContainer);

	TSharedRef<SSidebar> Sidebar = SNew(SSidebar, SidebarContainer)
		.TabLocation(ESidebarTabLocation::Left)
		.InitialDrawerSize(0.25f)
		.OnGetContent(FOnGetContent::CreateSPLambda(
			this,
			[this]
			{ 
				return Layout.ToSharedRef();
			})
		);

	FSidebarState SidebarState;
	SidebarState.SetDrawerSizes(0.25f, 0.25f);

	SidebarContainer->RebuildSidebar(Sidebar, SidebarState);

	const FName DrawerId = "Library";

	FSidebarDrawerConfig LibraryDrawerConfig;
	LibraryDrawerConfig.UniqueId = DrawerId;
	LibraryDrawerConfig.ButtonText = LOCTEXT("Library", "Library");
	LibraryDrawerConfig.ToolTipText = LOCTEXT("LibraryTooltip", "Open the Library side bar.");
	LibraryDrawerConfig.Icon = FAppStyle::GetBrush("Icons.FolderOpen");
	LibraryDrawerConfig.InitialState = SidebarState.FindOrAddDrawerState(DrawerId);
	LibraryDrawerConfig.OverrideContentWidget = Library;

	Sidebar->RegisterDrawer(MoveTemp(LibraryDrawerConfig));

	ChildSlot
	[
		SidebarContainer
	];

	CreateView();
}

TSharedRef<FMediaViewerLibrary> SMediaViewer::GetLibrary() const
{
	return Library->GetLibrary();
}

TSharedPtr<FMediaImageViewer> SMediaViewer::GetImageViewer(EMediaImageViewerPosition InPosition) const
{
	return ImageViewers[static_cast<int32>(InPosition)];
}

void SMediaViewer::SetImageViewer(EMediaImageViewerPosition InPosition, const TSharedRef<FMediaImageViewer>& InImageViewer)
{
	const int32 Index = static_cast<int32>(InPosition);

	if (ImageViewers[Index].IsValid() && ImageViewers[Index]->GetInfo().Id == InImageViewer->GetInfo().Id)
	{
		return;
	}

	ImageViewers[Index] = InImageViewer;
	ScaleToFit[Index] = true;

	if (InImageViewer != FNullImageViewer::GetNullImageViewer())
	{
		Library->OnImageViewerOpened(InImageViewer);
	}

	if (InPosition == EMediaImageViewerPosition::Second)
	{
		RequestedView = EMediaImageViewerActivePosition::Both;
	}

	InvalidateView();
}

void SMediaViewer::ClearImageViewer(EMediaImageViewerPosition InPosition)
{
	SetImageViewer(InPosition, FNullImageViewer::GetNullImageViewer());
}

void SMediaViewer::BindCommands()
{
	const FMediaViewerCommands& Commands = FMediaViewerCommands::Get();

	CommandList->MapAction(
		Commands.ToggleOverlay, 
		FExecuteAction::CreateSP(this, &SMediaViewer::ToggleOverlays),
		FCanExecuteAction(),
		FGetActionCheckState::CreateSP(this, &SMediaViewer::AreOverlaysEnabled)
	);

	CommandList->MapAction(
		Commands.ToggleLockedTransform,
		FExecuteAction::CreateSP(this, &SMediaViewer::ToggleLockedTransform),
		FCanExecuteAction::CreateSP(this, &SMediaViewer::IsShowingBothImageViewers),
		FGetActionCheckState::CreateSP(this, &SMediaViewer::AreLockedTransformEnabled)
	);
	
	CommandList->MapAction(
		Commands.SecondImageOpacityMinus,
		FExecuteAction::CreateSP(this, &SMediaViewer::AdjustSecondImageOpacity, -10.f),
		FCanExecuteAction::CreateSP(this, &SMediaViewer::IsShowingBothImageViewers)
	);
	
	CommandList->MapAction(
		Commands.SecondImageOpacityPlus, 
		FExecuteAction::CreateSP(this, &SMediaViewer::AdjustSecondImageOpacity, 10.f),
		FCanExecuteAction::CreateSP(this, &SMediaViewer::IsShowingBothImageViewers)
	);

	CommandList->MapAction(
		Commands.SwapAB, 
		FExecuteAction::CreateSP(this, &SMediaViewer::SwapABImageViewers),
		FCanExecuteAction::CreateSP(this, &SMediaViewer::IsShowingBothImageViewers)
	);

	CommandList->MapAction(Commands.ResetAllTransforms, FExecuteAction::CreateSP(this, &SMediaViewer::ResetTransformToAll));
}

bool SMediaViewer::IsShowingBothImageViewers() const
{
	return ActiveView == EMediaImageViewerActivePosition::Both;
}

void SMediaViewer::SwapABImageViewers()
{
	TSharedPtr<FMediaImageViewer> OldSecond = ImageViewers[static_cast<int32>(EMediaImageViewerPosition::Second)];	
	ImageViewers[static_cast<int32>(EMediaImageViewerPosition::Second)] = ImageViewers[static_cast<int32>(EMediaImageViewerPosition::First)];
	ImageViewers[static_cast<int32>(EMediaImageViewerPosition::First)] = OldSecond;

	const FVector OldOffset = ImageViewers[static_cast<int32>(EMediaImageViewerPosition::First)]->GetPaintSettings().Offset;
	ImageViewers[static_cast<int32>(EMediaImageViewerPosition::First)]->GetPaintSettings().Offset = ImageViewers[static_cast<int32>(EMediaImageViewerPosition::Second)]->GetPaintSettings().Offset;
	ImageViewers[static_cast<int32>(EMediaImageViewerPosition::Second)]->GetPaintSettings().Offset = OldOffset;

	InvalidateView();
}

void SMediaViewer::SetSingleView()
{
	if (ActiveView == EMediaImageViewerActivePosition::Single)
	{
		return;
	}

	RequestedView = EMediaImageViewerActivePosition::Single;

	InvalidateView();
}

void SMediaViewer::SetABView()
{
	if (ActiveView == EMediaImageViewerActivePosition::Both)
	{
		return;
	}

	RequestedView = EMediaImageViewerActivePosition::Both;

	InvalidateView();
}

void SMediaViewer::SetABOrientation(EOrientation InOrientation)
{
	if (MediaViewerSettings.ABOrientation == InOrientation
		&& ActiveView == EMediaImageViewerActivePosition::Both)
	{
		return;
	}

	MediaViewerSettings.ABOrientation = InOrientation;
	RequestedView = EMediaImageViewerActivePosition::Both;
	InvalidateView();
}

void SMediaViewer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FVector2D ScreenSpacePosition = FSlateApplication::Get().GetCursorPos();
	const FVector2D LocalPosition = AllottedGeometry.AbsoluteToLocal(ScreenSpacePosition);

	bool bNewLocation = (CursorLocalPosition - LocalPosition).IsNearlyZero() == false;

	if (bNewLocation)
	{
		CursorLocalPosition = LocalPosition;
	}

	constexpr int32 TickCheckValue = 2;
	constexpr int32 TickStopCheckValue = 3;

	if (TickCount < TickStopCheckValue)
	{
		++TickCount;

		if (TickCount == TickCheckValue)
		{
			CheckLoadState();
		}
	}

	if (bInvalidated)
	{
		CreateView();
		return;
	}

	const TOptional<FVector2D> UpdatedMousePosition = bNewLocation
		? LocalPosition
		: TOptional<FVector2D>();
	
	if (TSharedPtr<SMediaImageViewerOverlay> FirstOverlay = GetOverlay(EMediaImageViewerPosition::First))
	{
		FirstOverlay->UpdateMouse(UpdatedMousePosition);
	}

	if (TSharedPtr<SMediaImageViewerOverlay> SecondOverlay = GetOverlay(EMediaImageViewerPosition::Second))
	{
		SecondOverlay->UpdateMouse(UpdatedMousePosition);
	}
}

int32 SMediaViewer::OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InMyCullingRect,
	FSlateWindowElementList& InOutDrawElements, int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool bInParentEnabled) const
{
	const FSlateLayoutTransform ViewerTransform = FSlateLayoutTransform(ViewerPosition);
	const FPaintGeometry ViewerPaintGeometry = InAllottedGeometry.ToPaintGeometry(ViewerSize, ViewerTransform);

	FSlateDrawElement::MakeBox(
		InOutDrawElements,
		InLayerId,
		ViewerPaintGeometry,
		&BackgroundColorBrush,
		ESlateDrawEffect::NoPixelSnapping,
		MediaViewerSettings.ClearColor
	);

	++InLayerId;

	if (UTexture* BackgroundTexture = MediaViewerSettings.Texture.LoadSynchronous())
	{
		BackgroundTextureBrush.SetResourceObject(BackgroundTexture);
		BackgroundTextureBrush.ImageSize.X = BackgroundTexture->GetSurfaceWidth();
		BackgroundTextureBrush.ImageSize.Y = BackgroundTexture->GetSurfaceHeight();

		const FVector2D ImageSize = BackgroundTextureBrush.ImageSize * MediaViewerSettings.Scale;

		TArray<FSlateVertex> Verts;
		Verts.Reserve(4);

		FSlateVertex BaseVert;
		BaseVert.Color = FColor::White;
		BaseVert.SecondaryColor = FColor::White;
		BaseVert.Position[0] = ViewerPosition.X;
		BaseVert.Position[1] = ViewerPosition.Y;
		BaseVert.TexCoords[0] = 0.f;
		BaseVert.TexCoords[1] = 0.f;
		BaseVert.TexCoords[2] = 1.f;
		BaseVert.TexCoords[3] = 1.f;

		FSlateVertex& TopLeft = Verts.Add_GetRef(BaseVert);

		FSlateVertex& TopRight = Verts.Add_GetRef(BaseVert);
		TopRight.TexCoords[0] = 1.f;

		FSlateVertex& BottomLeft = Verts.Add_GetRef(BaseVert);
		BottomLeft.TexCoords[1] = 1.f;

		FSlateVertex& BottomRight = Verts.Add_GetRef(BaseVert);
		BottomRight.TexCoords[0] = 1.f;
		BottomRight.TexCoords[1] = 1.f;

		for (int32 Index = 0; Index < Verts.Num(); ++Index)
		{
			Verts[Index].MaterialTexCoords[0] = Verts[Index].TexCoords[0];
			Verts[Index].MaterialTexCoords[1] = Verts[Index].TexCoords[1];
		}

		TArray<SlateIndex> Indices = {0, 2, 3, 0, 3, 1};

		FVector2D Offset = MediaViewerSettings.Offset;
		Offset.X = FMath::Fmod(Offset.X, BackgroundTextureBrush.ImageSize.X);
		Offset.Y = FMath::Fmod(Offset.Y, BackgroundTextureBrush.ImageSize.Y);

		FVector2D Start = FVector2D::ZeroVector;
		Start.X = (Offset.X > 0) ? (-BackgroundTextureBrush.ImageSize.X + Offset.X) : Offset.X;
		Start.Y = (Offset.Y > 0) ? (-BackgroundTextureBrush.ImageSize.Y + Offset.Y) : Offset.Y;

		for (float X = Start.X; X < ViewerSize.X; X += ImageSize.X)
		{
			for (float Y = Start.Y; Y < ViewerSize.Y; Y += ImageSize.Y)
			{
				TopLeft.Position.X = BaseVert.Position.X + X;
				TopLeft.Position.Y = BaseVert.Position.Y + Y;

				TopRight.Position.X = BaseVert.Position.X + X + ImageSize.X;
				TopRight.Position.Y = BaseVert.Position.Y + Y;

				BottomLeft.Position.X = BaseVert.Position.X + X;
				BottomLeft.Position.Y = BaseVert.Position.Y + Y + ImageSize.Y;

				BottomRight.Position.X = BaseVert.Position.X + X + ImageSize.X;
				BottomRight.Position.Y = BaseVert.Position.Y + Y + ImageSize.Y;

				FSlateDrawElement::MakeCustomVerts(
					InOutDrawElements,
					InLayerId,
					BackgroundTextureBrush.GetRenderingResource(),
					Verts,
					Indices,
					nullptr,
					0,
					0,
					ESlateDrawEffect::NoPixelSnapping
				);
			}
		}

		++InLayerId;
	}

	FFloatRange UVRange = FFloatRange(0.f, 1.f);

	FMediaImagePaintParams PaintParams = {
		InArgs,
		InAllottedGeometry,
		InMyCullingRect,
		InWidgetStyle,
		bInParentEnabled,
		UVRange,
		GetDPIScale(),
		MediaViewerSettings.ABOrientation,
		ViewerSize,
		ViewerPosition,
		1.f,
		InLayerId,
		InOutDrawElements
	};

	const float ABSplitterLocation = MediaViewerSettings.ABSplitterLocation / 100.f;
	const float SecondImageOpacity = MediaViewerSettings.SecondImageOpacity / 100.f;

	switch (ActiveView)
	{
		case EMediaImageViewerActivePosition::Single:
			GetImageViewer(EMediaImageViewerPosition::First)->Paint(PaintParams);
			break;

		case EMediaImageViewerActivePosition::Both:
			if (!FMath::IsNearlyZero(ABSplitterLocation))
			{
				UVRange.SetLowerBound(0.f);
				UVRange.SetUpperBound(1.f);
				GetImageViewer(EMediaImageViewerPosition::First)->Paint(PaintParams);
			}

			if (!FMath::IsNearlyZero(ABSplitterLocation - 1.f))
			{
				UVRange.SetLowerBound(ABSplitterLocation);
				UVRange.SetUpperBound(1.f);
				PaintParams.ImageOpacity = SecondImageOpacity;
				GetImageViewer(EMediaImageViewerPosition::Second)->Paint(PaintParams);
			}
			break;
	}

	const int32 NewLayerId = SCompoundWidget::OnPaint(InArgs, InAllottedGeometry, InMyCullingRect, InOutDrawElements, PaintParams.LayerId, InWidgetStyle, bInParentEnabled);

	// Doing this after rendering or it creates flicker.
	if (ContentSlot)
	{
		// InAllowedGeometry.GetAbsolutePosition() is relative to the parent.
		// GetTickSpaceGeometry().GetAbsolutePosition() is in desktop space.
		ViewerPosition = (ContentSlot->GetWidget()->GetTickSpaceGeometry().GetAbsolutePosition() - GetTickSpaceGeometry().GetAbsolutePosition()) / InAllottedGeometry.Scale;
		ViewerSize = ContentSlot->GetWidget()->GetTickSpaceGeometry().GetAbsoluteSize() / InAllottedGeometry.Scale;
	}

	return NewLayerId;
}

FReply SMediaViewer::OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(InMyGeometry, InKeyEvent);
}

void SMediaViewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddPropertyReferencesWithStructARO(
		FMediaViewerSettings::StaticStruct(),
		&MediaViewerSettings
	);
}

FString SMediaViewer::GetReferencerName() const
{
	static const FString ReferencerName = TEXT("SMediaViewer");
	return ReferencerName;
}

void SMediaViewer::CreateDelegates()
{
	Delegates = MakeShared<FMediaViewerDelegates>();

	// Global viewer delegates
	Delegates->SetSingleView.BindSP(this, &SMediaViewer::SetSingleView);
	Delegates->SetABView.BindSP(this, &SMediaViewer::SetABView);
	Delegates->GetABOrientation.BindSPLambda(this, [this] { return MediaViewerSettings.ABOrientation; });
	Delegates->SetABOrientation.BindSP(this, &SMediaViewer::SetABOrientation);
	Delegates->GetActiveView.BindSPLambda(this, [this] { return ActiveView; });
	Delegates->GetSettings.BindSPLambda(this, [this]{ return &MediaViewerSettings; });
	Delegates->AreTransformsLocked.BindSPLambda(this, [this] { return MediaViewerSettings.bAreTransformsLocked; });
	Delegates->ToggleLockedTransform.BindSPLambda(this, [this] { MediaViewerSettings.bAreTransformsLocked = !MediaViewerSettings.bAreTransformsLocked; });
	Delegates->GetViewerSize.BindSPLambda(this, [this] { return FIntPoint(ViewerSize.X, ViewerSize.Y); });
	Delegates->GetViewerPosition.BindSPLambda(this, [this] { return FIntPoint(ViewerPosition.X, ViewerPosition.Y); });
	Delegates->SwapAB.BindSP(this, &SMediaViewer::SwapABImageViewers);
	Delegates->GetCursorLocation.BindSP(this, &SMediaViewer::GetLocalCursorPosition);
	Delegates->AddOffsetToAll.BindSP(this, &SMediaViewer::AddOffsetToAll);
	Delegates->AddRotationToAll.BindSP(this, &SMediaViewer::AddRotationToAll);
	Delegates->MultiplyScaleToAll.BindSP(this, &SMediaViewer::MultiplyScaleToAll);
	Delegates->MultiplyScaleAroundCursorToAll.BindSP(this, &SMediaViewer::MultiplyScaleAroundCursorToAll);
	Delegates->SetTransformToAll.BindSP(this, &SMediaViewer::SetTransformToAll);
	Delegates->ResetTransformToAll.BindSP(this, &SMediaViewer::ResetTransformToAll);
	Delegates->GetSecondImageViewerOpacity.BindSPLambda(this, [this] { return MediaViewerSettings.SecondImageOpacity; });
	Delegates->SetSecondImageViewerOpacity.BindSPLambda(this, [this](float InValue) { MediaViewerSettings.SecondImageOpacity = InValue; });
	Delegates->GetABSplitterLocation.BindSPLambda(this, [this] { return MediaViewerSettings.ABSplitterLocation; });
	Delegates->SetABSplitterLocation.BindSPLambda(this, [this](float InValue) { OnABResized(InValue * 0.01f); });
	Delegates->GetLibrary.BindSPLambda(this, [this] { return Library->GetLibrary(); });
	Delegates->RefreshView.BindSPLambda(this, [this] { InvalidateView(); });
	Delegates->GetCommandList.BindSPLambda(this, [this] { return CommandList; });
	Delegates->IsOverViewer.BindSPLambda(this, [this] { return IsHovered(); });
	Delegates->GetTab.BindSPLambda(this, [this] { return Tab.Pin(); });

	// Per panel delegates
	Delegates->GetImageViewer.BindSP(this, &SMediaViewer::GetImageViewer);
	Delegates->SetImageViewer.BindSP(this, &SMediaViewer::SetImageViewer);
	Delegates->ClearImageViewer.BindSP(this, &SMediaViewer::ClearImageViewer);
	Delegates->GetPixelCoordinates.BindSP(this, &SMediaViewer::GetPixelCoordinates);
	Delegates->IsOverImage.BindSP(this, &SMediaViewer::IsOverImage);
	Delegates->CopyTransformToAll.BindSP(this, &SMediaViewer::CopyTransformToAll);
	Delegates->GetCommandListForPosition.BindSP(this, &SMediaViewer::GetOverlayCommandList);
}

TSharedRef<SMediaImageViewerOverlay> SMediaViewer::CreateOverlay(EMediaImageViewerPosition InPosition, bool bInComparisonView)
{
	bool bScaleToFitImage = false;
	const int32 Index = static_cast<int32>(InPosition);

	if (ScaleToFit[Index])
	{
		bScaleToFitImage = true;
		ScaleToFit[Index] = false;
	}

	return SNew(SMediaImageViewerOverlay, InPosition, Delegates.ToSharedRef())
		.bComparisonView(bInComparisonView)
		.bScaleToFit(bScaleToFitImage);
}

TSharedRef<SWidget> SMediaViewer::CreateToolbar()
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SColorBlock)
			.Color(FStyleColors::Panel.GetSpecifiedColor())
		]
		+ SOverlay::Slot()
		[
			SNew(SMediaViewerToolbar, Delegates.ToSharedRef())
		];
}

void SMediaViewer::CreateView()
{
	ActiveView = RequestedView;

	switch (ActiveView)
	{
		case EMediaImageViewerActivePosition::Single:
			CreateSingleView(EMediaImageViewerPosition::First);
			break;

		case EMediaImageViewerActivePosition::Both:
			CreateABView();
			break;
	}

	if (MediaViewerArgs.bShowToolbar)
	{
		ToolbarSlot->AttachWidget(CreateToolbar());
	}

	bInvalidated = false;
}

void SMediaViewer::CreateSingleView(EMediaImageViewerPosition InPosition)
{
	// Attach the single overlay
	ContentSlot->AttachWidget(CreateOverlay(InPosition, /* Show Panel Name */ false));
}

void SMediaViewer::CreateABView()
{
	MediaViewerSettings.ABSplitterLocation = 50.f;

	ContentSlot->AttachWidget(
		SNew(SSplitter)
		.Orientation(MediaViewerSettings.ABOrientation)
		.PhysicalSplitterHandleSize(3.f)
		.HitDetectionSplitterHandleSize(3.f)

		+ SSplitter::Slot()
		.Value(0.5f)
		.MinSize(10.f)
		.Resizable(true)
		.SizeRule(SSplitter::FractionOfParent)
		.OnSlotResized(this, &SMediaViewer::OnABResized)
		[
			CreateOverlay(EMediaImageViewerPosition::First, /* Show Panel Name */ true)
		]

		+ SSplitter::Slot()
		.Value(0.5f)
		.MinSize(10.f)
		.Resizable(true)
		.SizeRule(SSplitter::FractionOfParent)
		[
			CreateOverlay(EMediaImageViewerPosition::Second, /* Show Panel Name */ true)
		]
	);	
}

TSharedPtr<SMediaImageViewerOverlay> SMediaViewer::GetOverlay(EMediaImageViewerPosition InPosition) const
{
	TSharedRef<SWidget> ContentWidget = ContentSlot->GetWidget();

	switch (ActiveView)
	{
		case EMediaImageViewerActivePosition::Single:
			if (InPosition == EMediaImageViewerPosition::First)
			{
				return StaticCastSharedRef<SMediaImageViewerOverlay>(ContentWidget);
			}
			break;

		case EMediaImageViewerActivePosition::Both:
		{
			TSharedRef<SSplitter> Splitter = StaticCastSharedRef<SSplitter>(ContentSlot->GetWidget());

			if (Splitter->NumSlots() == static_cast<int32>(EMediaImageViewerPosition::COUNT))
			{
				return StaticCastSharedRef<SMediaImageViewerOverlay>(
					Splitter->SlotAt(static_cast<int32>(InPosition)).GetWidget()
				);
			}
		}
	}

	return nullptr;
}

float SMediaViewer::GetDPIScale() const
{
	if (const TSharedPtr<SWindow> TopLevelWindow = FSlateApplication::Get().FindWidgetWindow(SharedThis(this)))
	{
		if (TopLevelWindow.IsValid())
		{
			return TopLevelWindow->GetDPIScaleFactor();
		}
	}

	return 1.0f;
}

void SMediaViewer::OnABResized(float InSize)
{
	if (!IsImageViewerNull(EMediaImageViewerPosition::First))
	{
		StaticCastSharedRef<SSplitter>(ContentSlot->GetWidget())->SlotAt(0).SetSizeValue(InSize);
	}

	if (!IsImageViewerNull(EMediaImageViewerPosition::Second))
	{
		StaticCastSharedRef<SSplitter>(ContentSlot->GetWidget())->SlotAt(1).SetSizeValue(1.f - InSize);
	}

	MediaViewerSettings.ABSplitterLocation = InSize * 100.f;
}

EMediaImageViewerPosition SMediaViewer::GetHoveredImageViewer() const
{
	const float ABSplitterLocation = MediaViewerSettings.ABSplitterLocation / 100.f;

	switch (MediaViewerSettings.ABOrientation)
	{
		default:
		case Orient_Horizontal:
			return (ABSplitterLocation * GetTickSpaceGeometry().GetLocalSize().X) < CursorLocalPosition.X
				? EMediaImageViewerPosition::First
				: EMediaImageViewerPosition::Second;

		case Orient_Vertical:
			return (ABSplitterLocation * GetTickSpaceGeometry().GetLocalSize().Y) < CursorLocalPosition.Y
				? EMediaImageViewerPosition::First
				: EMediaImageViewerPosition::Second;
	}
}

FVector2D SMediaViewer::GetLocalCursorPosition() const
{
	return CursorLocalPosition;
}

TSharedRef<FMediaViewerDelegates> SMediaViewer::GetDelegates() const
{
	return Delegates.ToSharedRef();
}

void SMediaViewer::SaveStates() const
{
	FMediaViewerState State;
	State.ViewerSettings = MediaViewerSettings;
	State.ActiveView = ActiveView;

	for (int32 Index = 0; Index < static_cast<int32>(EMediaImageViewerPosition::COUNT); ++Index)
	{
		bool bAdded = false;

		if (ImageViewers[Index].IsValid())
		{
			if (TSharedPtr<FMediaViewerLibraryItem> LibraryItem = ImageViewers[Index]->CreateLibraryItem())
			{
				State.Images.Add({
					LibraryItem->GetItemType(),
					LibraryItem->GetStringValue(),
					ImageViewers[Index]->GetPanelSettings(),
					ImageViewers[Index]->GetPaintSettings()
				});
				bAdded = true;
			}
		}

		if (!bAdded)
		{
			State.Images.Add({});
		}
	}

	UMediaViewerLibraryIni& Ini = UMediaViewerLibraryIni::Get();
	Ini.SetSavedStates({State});
	Ini.SaveConfig();
}

void SMediaViewer::LoadState(int32 InIndex)
{
	TConstArrayView<FMediaViewerState> SavedStates = UMediaViewerLibraryIni::Get().GetSavedStates();

	if (!SavedStates.IsValidIndex(InIndex))
	{
		return;
	}

	const FMediaViewerState& State = SavedStates[InIndex];

	TArray<TSharedPtr<FMediaViewerLibraryItem>> Items;
	Items.Reserve(State.Images.Num());
	bool bHasValidItem = false;

	for (int32 Index = 0; Index < State.Images.Num(); ++Index)
	{
		TSharedPtr<FMediaViewerLibraryItem> Item = Library->GetLibrary()->FindItemByValue(
			State.Images[Index].ImageType, 
			State.Images[Index].StringValue
		);

		Items.Add(Item);

		if (Item.IsValid())
		{
			bHasValidItem = true;
		}
	}

	if (!bHasValidItem)
	{
		return;
	}

	const EAppReturnType::Type Result = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("LoadOldState", "Attempt to open previous images?")
	);

	if (Result != EAppReturnType::Yes)
	{
		return;
	}

	MediaViewerSettings = State.ViewerSettings;
	RequestedView = State.ActiveView;

	for (int32 Index = 0; Index < State.Images.Num(); ++Index)
	{
		if (Items[Index].IsValid())
		{
			if (TSharedPtr<FMediaImageViewer> Viewer = Items[Index]->CreateImageViewer())
			{
				ImageViewers[Index] = Viewer.ToSharedRef();
				Viewer->GetPanelSettings() = State.Images[Index].PanelSettings;
				Viewer->GetPaintSettings() = State.Images[Index].PaintSettings;
			}
		}
	}

	InvalidateView();
}

void SMediaViewer::InvalidateView()
{
	bInvalidated = true;
}

bool SMediaViewer::IsImageViewerNull(EMediaImageViewerPosition InPosition) const
{
	return !ImageViewers[static_cast<int32>(InPosition)].IsValid()
		|| ImageViewers[static_cast<int32>(InPosition)]->GetInfo().Id == FNullImageViewer::GetNullImageViewer()->GetInfo().Id;
}

void SMediaViewer::AddOffsetToAll(const FVector& InOffset)
{
	for (const TSharedPtr<FMediaImageViewer>& ImageViewer : ImageViewers)
	{
		if (ImageViewer.IsValid())
		{
			ImageViewer->GetPaintSettings().Offset += InOffset;
		}
	}
}

void SMediaViewer::AddRotationToAll(const FRotator& InRotation)
{
	for (const TSharedPtr<FMediaImageViewer>& ImageViewer : ImageViewers)
	{
		if (ImageViewer.IsValid())
		{
			ImageViewer->GetPaintSettings().Rotation += InRotation;
		}
	}
}

void SMediaViewer::MultiplyScaleToAll(float InMultiple)
{
	for (const TSharedPtr<FMediaImageViewer>& ImageViewer : ImageViewers)
	{
		if (ImageViewer.IsValid())
		{
			ImageViewer->GetPaintSettings().Scale *= InMultiple;
		}
	}
}

void SMediaViewer::MultiplyScaleAroundCursorToAll(float InMultiple)
{
	for (int32 Index = 0; Index < static_cast<int32>(EMediaImageViewerPosition::COUNT); ++Index)
	{
		if (TSharedPtr<SMediaImageViewerOverlay> Overlay = GetOverlay(static_cast<EMediaImageViewerPosition>(Index)))
		{
			Overlay->MultiplyScaleAroundCursor(InMultiple);
		}
	}
}

void SMediaViewer::SetTransformToAll(const FVector& InOffset, const FRotator& InRotation, float InScale)
{
	for (const TSharedPtr<FMediaImageViewer>& ImageViewer : ImageViewers)
	{
		if (ImageViewer.IsValid())
		{
			ImageViewer->GetPaintSettings().Offset = InOffset;
			ImageViewer->GetPaintSettings().Rotation = InRotation;
			ImageViewer->GetPaintSettings().Scale = InScale;
		}
	}
}

void SMediaViewer::ResetTransformToAll()
{
	SetTransformToAll(FVector::ZeroVector, FRotator::ZeroRotator, 1.f);
}

FIntPoint SMediaViewer::GetPixelCoordinates(EMediaImageViewerPosition InPosition) const
{
	if (TSharedPtr<SMediaImageViewerOverlay> Overlay = GetOverlay(InPosition))
	{
		return Overlay->GetImageViewerPixelCoordinates();
	}

	return FIntPoint(-1, -1);
}

bool SMediaViewer::IsOverImage(EMediaImageViewerPosition InPosition) const
{
	if (TSharedPtr<SMediaImageViewerOverlay> Overlay = GetOverlay(InPosition))
	{
		return Overlay->IsCursorOverImageViewer();
	}

	return false;
}

void SMediaViewer::CopyTransformToAll(EMediaImageViewerPosition InPosition) const
{
	const int32 SourceIndex = static_cast<int32>(InPosition);
	const FMediaImagePaintSettings& SourcePaintSettings = ImageViewers[SourceIndex]->GetPaintSettings();

	for (int32 Index = 0; Index < static_cast<int32>(EMediaImageViewerPosition::COUNT); ++Index)
	{
		if (Index != SourceIndex)
		{
			FMediaImagePaintSettings& PaintSettings = ImageViewers[Index]->GetPaintSettings();
			PaintSettings.Offset = SourcePaintSettings.Offset;
			PaintSettings.Rotation = SourcePaintSettings.Rotation;
			PaintSettings.Scale = SourcePaintSettings.Scale;
		}
	}
}

TSharedPtr<FUICommandList> SMediaViewer::GetOverlayCommandList(EMediaImageViewerPosition InPosition) const
{
	if (TSharedPtr<SMediaImageViewerOverlay> Overlay = GetOverlay(InPosition))
	{
		return Overlay->GetCommandList();
	}

	return nullptr;
}

float SMediaViewer::GetSecondImageOpacity() const
{
	return MediaViewerSettings.SecondImageOpacity;
}

void SMediaViewer::AdjustSecondImageOpacity(float InAdjustment)
{
	SetSecondImageOpacity(GetSecondImageOpacity() + InAdjustment);
}

void SMediaViewer::SetSecondImageOpacity(float InOpacity)
{
	MediaViewerSettings.SecondImageOpacity = InOpacity;
}

ECheckBoxState SMediaViewer::AreOverlaysEnabled() const
{
	for (int32 Index = 0; Index < static_cast<int32>(EMediaImageViewerPosition::COUNT); ++Index)
	{
		if (TSharedPtr<SMediaImageViewerOverlay> Overlay = GetOverlay(static_cast<EMediaImageViewerPosition>(Index)))
		{
			if (Overlay->IsOverlayEnabled())
			{
				return ECheckBoxState::Checked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}

void SMediaViewer::ToggleOverlays()
{
	for (int32 Index = 0; Index < static_cast<int32>(EMediaImageViewerPosition::COUNT); ++Index)
	{
		if (TSharedPtr<SMediaImageViewerOverlay> Overlay = GetOverlay(static_cast<EMediaImageViewerPosition>(Index)))
		{
			Overlay->ToggleOverlay();
		}
	}
}

ECheckBoxState SMediaViewer::AreLockedTransformEnabled() const
{
	return MediaViewerSettings.bAreTransformsLocked
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SMediaViewer::ToggleLockedTransform()
{
	MediaViewerSettings.bAreTransformsLocked = !MediaViewerSettings.bAreTransformsLocked;
}

void SMediaViewer::CheckLoadState()
{
	const FGuid& NullImageId = FNullImageViewer::GetNullImageViewer()->GetInfo().Id;

	for (int32 Index = 0; Index < static_cast<int32>(EMediaImageViewerPosition::COUNT); ++Index)
	{
		if (ImageViewers[Index].IsValid() && ImageViewers[Index]->GetInfo().Id != NullImageId)
		{
			return;
		}
	}

	LoadState(0);
}

} // UE::MediaViewer::Private

#undef LOCTEXT_NAMESPACE
