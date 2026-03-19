// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaImageViewerOverlay.h"

#include "DetailLayoutBuilder.h"
#include "EditorViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ImageViewer/MediaImageViewer.h"
#include "ImageViewers/NullImageViewer.h"
#include "IMediaViewerModule.h"
#include "Library/MediaViewerLibrary.h"
#include "Library/MediaViewerLibraryItem.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "MediaViewer.h"
#include "MediaViewerCommands.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SMediaImageViewerStatusBar.h"
#include "Widgets/SMediaViewer.h"
#include "Widgets/SMediaViewerDropTarget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMediaImageViewerOverlay"

namespace UE::MediaViewer::Private
{

const FName DropTargetName = TEXT("DropTarget");

SMediaImageViewerOverlay::SMediaImageViewerOverlay()
	: CommandList(MakeShared<FUICommandList>())
	, DragButtonName(NAME_None)
	, bDragging(false)
	, bExternalDragging(false)
	, bOverlayEnabled(true)
{	
}

void SMediaImageViewerOverlay::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SMediaImageViewerOverlay::Construct(const FArguments& InArgs, EMediaImageViewerPosition InPosition, 
	const TSharedRef<FMediaViewerDelegates>& InDelegates)
{
	Position = InPosition;
	Delegates = InDelegates;
	bScaleToFit = InArgs._bScaleToFit;

	TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position);

	if (!ImageViewer.IsValid())
	{
		return;
	}

	CachedItem = ImageViewer->CreateLibraryItem();

	BindCommands();
	InDelegates->GetCommandList.Execute()->Append(CommandList);

	Delegates = InDelegates;

	ChildSlot
	[
		SNew(SScissorRectBox)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				CreateStatusBar(InDelegates)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				CreateOverlay(InArgs._bComparisonView)
			]
		]
	];
}

TSharedPtr<FMediaImageViewer> SMediaImageViewerOverlay::GetImageViewer() const
{
	return Delegates->GetImageViewer.Execute(Position);
}

const TSharedRef<FUICommandList>& SMediaImageViewerOverlay::GetCommandList() const
{
	return CommandList;
}

FIntPoint SMediaImageViewerOverlay::GetImageViewerPixelCoordinates() const
{
	const FVector2D CursorLocation = GetImageViewerPixelCoordinates_Exact();

	return FIntPoint(
		FMath::RoundToInt(CursorLocation.X),
		FMath::RoundToInt(CursorLocation.Y)
	);
}

FVector2D SMediaImageViewerOverlay::GetImageViewerPixelCoordinates_Exact() const
{
	TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position);

	if (!ImageViewer.IsValid())
	{
		return FVector2D(-1, -1);
	}

	const FVector2D ViewerPosition = Delegates->GetViewerPosition.Execute();
	const FVector2D ViewerSize = Delegates->GetViewerSize.Execute();
	const FVector2D TopLeft = ImageViewer->GetPaintOffset(ViewerSize, Delegates->GetViewerPosition.Execute());

	FVector2D CursorLocation = Delegates->GetCursorLocation.Execute() - ViewerPosition;
	CursorLocation.X -= TopLeft.X;
	CursorLocation.Y -= TopLeft.Y;
	CursorLocation.X += ViewerPosition.X;
	CursorLocation.Y += ViewerPosition.Y;
	CursorLocation /= ImageViewer->GetPaintSettings().Scale;

	return CursorLocation;
}

bool SMediaImageViewerOverlay::IsCursorOverImageViewer() const
{
	TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position);

	if (!ImageViewer.IsValid())
	{
		return false;
	}

	const FIntPoint PixelCoordinates = GetImageViewerPixelCoordinates();

	if (PixelCoordinates.X < 0 || PixelCoordinates.Y < 0)
	{
		return false;
	}

	const FIntPoint& ImageSize = ImageViewer->GetInfo().Size;

	if (PixelCoordinates.X >= ImageSize.X || PixelCoordinates.Y >= ImageSize.Y)
	{
		return false;
	}

	return true;
}

FCursorReply SMediaImageViewerOverlay::OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const
{
	if (bDragging || bExternalDragging)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	}

	if (IsCursorOverImageViewer())
	{
		return FCursorReply::Cursor(EMouseCursor::Crosshairs);
	}

	return SCompoundWidget::OnCursorQuery(InMyGeometry, InCursorEvent);
}

FReply SMediaImageViewerOverlay::OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(InMyGeometry, InKeyEvent);
}

FReply SMediaImageViewerOverlay::OnMouseButtonDown(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging && !bExternalDragging)
	{
		const FVector2D CursorLocation = Delegates->GetCursorLocation.Execute();

		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			DragStartCursor = CursorLocation;
			DragStartOffset = GetOffset();
			DragButtonName = InMouseEvent.GetEffectingButton().GetFName();
			bDragging = true;
			return FReply::Handled();
		}

		if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
		{
			const FInputEventState EventState(nullptr, InMouseEvent.GetEffectingButton(), IE_Pressed);

			if (ImageViewer->OnTrackingStarted(EventState, FIntPoint(FMath::RoundToInt(CursorLocation.X), FMath::RoundToInt(CursorLocation.Y))))
			{
				DragButtonName = InMouseEvent.GetEffectingButton().GetFName();
				bExternalDragging = true;
				return FReply::Handled();
			}
		}
	}

	return SCompoundWidget::OnMouseButtonDown(InMyGeometry, InMouseEvent);
}

FReply SMediaImageViewerOverlay::OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging && !bExternalDragging)
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			FSlateApplication::Get().PushMenu(
				SharedThis(this),
				FWidgetPath(),
				CreateMenu(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect::ContextMenu
			);

			return FReply::Handled();
		}
	}
	else
	{
		OnDragButtonUp(InMouseEvent.GetEffectingButton().GetFName());
	}

	return SCompoundWidget::OnMouseButtonUp(InMyGeometry, InMouseEvent);
}

FReply SMediaImageViewerOverlay::OnMouseWheel(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	static const float NormalMultiplier = FMath::Pow(2.0, 0.125);
	static const float FastMultiplier = FMath::Pow(2.0, 0.5);
	static const float SlowMultiplier = FMath::Pow(2.0, 0.03125);

	const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
	
	const float ModifierMultiplier = 
		ModifierKeys.IsLeftShiftDown() 
		? FastMultiplier
		: ((ModifierKeys.IsLeftControlDown() || ModifierKeys.IsLeftCommandDown())
			? SlowMultiplier
			: NormalMultiplier);
	
	const float ScaleMultiplier = InMouseEvent.GetWheelDelta() > 0
		? ModifierMultiplier
		: (1.f / ModifierMultiplier);

	if (Delegates->AreTransformsLocked.Execute())
	{
		Delegates->MultiplyScaleAroundCursorToAll.Execute(ScaleMultiplier);
	}
	else
	{
		MultiplyScaleAroundCursor(ScaleMultiplier);
	}

	return SCompoundWidget::OnMouseWheel(InMyGeometry, InMouseEvent);
}

bool SMediaImageViewerOverlay::SupportsKeyboardFocus() const
{
	return true;
}

void SMediaImageViewerOverlay::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (bScaleToFit)
	{
		const FVector2D LocalSize = Delegates->GetViewerSize.Execute();

		if (FMath::IsNearlyZero(LocalSize.X) || FMath::IsNearlyZero(LocalSize.Y))
		{
			return;
		}

		TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position);

		if (ImageViewer.IsValid() && ImageViewer->GetInfo().Size.X > 2)
		{
			ScaleToFit(/* Use transform lock */ false);
			bScaleToFit = false;
		}
	}
}

void SMediaImageViewerOverlay::UpdateMouse(const TOptional<FVector2D>& InMousePosition)
{
	if (!DragButtonName.IsNone())
	{
		if (!FSlateApplication::Get().GetPressedMouseButtons().Contains(DragButtonName))
		{
			OnDragButtonUp(DragButtonName);
		}
	}

	if (bDragging)
	{
		UpdateDragPosition();
	}

	if (InMousePosition.IsSet())
	{
		if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
		{
			ImageViewer->OnMouseMove(InMousePosition.GetValue());
		}
	}
}

void SMediaImageViewerOverlay::OnDragButtonUp(FName InKeyName)
{
	if (InKeyName != DragButtonName)
	{
		return;
	}

	if (bDragging)
	{
		UpdateDragPosition();
		DragButtonName = NAME_None;
		bDragging = false;
	}
	else if (bExternalDragging)
	{
		if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
		{
			const FVector2D CursorLocation = Delegates->GetCursorLocation.Execute();
			ImageViewer->OnTrackingStopped(FIntPoint(FMath::RoundToInt(CursorLocation.X), FMath::RoundToInt(CursorLocation.Y)));
		}

		DragButtonName = NAME_None;
		bExternalDragging = false;
	}
}

void SMediaImageViewerOverlay::UpdateDragPosition()
{
	const FVector2D CursorLocation = Delegates->GetCursorLocation.Execute();

	FVector NewOffset = DragStartOffset;
	NewOffset.X += CursorLocation.X - DragStartCursor.X;
	NewOffset.Y += CursorLocation.Y - DragStartCursor.Y;

	Try_AddOffset(NewOffset - GetOffset());
}

EVisibility SMediaImageViewerOverlay::GetDragDescriptionVisibility() const
{
	return FSlateApplication::Get().IsDragDropping()
		? EVisibility::HitTestInvisible
		: EVisibility::Collapsed;
}

void SMediaImageViewerOverlay::BindCommands()
{
	const FMediaViewerCommands& Commands = FMediaViewerCommands::Get();

	CommandList->MapAction(Commands.MoveLeft,     FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddOffset, FVector(-10,   0,   0)));
	CommandList->MapAction(Commands.MoveRight,    FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddOffset, FVector( 10,   0,   0)));
	CommandList->MapAction(Commands.MoveUp,       FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddOffset, FVector(  0, -10,   0)));
	CommandList->MapAction(Commands.MoveDown,     FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddOffset, FVector(  0,  10,   0)));
	CommandList->MapAction(Commands.MoveForward,  FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddOffset, FVector(  0,   0, -10)));
	CommandList->MapAction(Commands.MoveBackward, FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddOffset, FVector(  0,   0,  10)));

	CommandList->MapAction(Commands.RotateMinusPitch, FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddRotation, FRotator(-10,   0,   0)));
	CommandList->MapAction(Commands.RotatePlusPitch,  FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddRotation, FRotator( 10,   0,   0)));
	CommandList->MapAction(Commands.RotateMinusYaw,   FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddRotation, FRotator(  0, -10,   0)));
	CommandList->MapAction(Commands.RotatePlusYaw,    FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddRotation, FRotator(  0,  10,   0)));
	CommandList->MapAction(Commands.RotateMinusRoll,  FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddRotation, FRotator(  0,   0, -10)));
	CommandList->MapAction(Commands.RotatePlusRoll,   FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_AddRotation, FRotator(  0,   0,  10)));

	CommandList->MapAction(Commands.Scale12,  FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_SetScale, 0.125f));
	CommandList->MapAction(Commands.Scale25,  FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_SetScale, 0.25f));
	CommandList->MapAction(Commands.Scale50,  FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_SetScale, 0.5f));
	CommandList->MapAction(Commands.Scale100, FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_SetScale, 1.0f));
	CommandList->MapAction(Commands.Scale200, FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_SetScale, 2.0f));
	CommandList->MapAction(Commands.Scale400, FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_SetScale, 4.0f));
	CommandList->MapAction(Commands.Scale800, FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_SetScale, 8.0f));

	CommandList->MapAction(
		Commands.ScaleToFit, 
		FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::ScaleToFit, /* Use transform lock */ true)
	);

	CommandList->MapAction(
		Commands.ResetTransform, 
		FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::Try_ResetTransform),
		FCanExecuteAction(),
		FGetActionCheckState(),
		FIsActionButtonVisible::CreateSP(this, &SMediaImageViewerOverlay::CanResetTransform)
	);

	CommandList->MapAction(
		Commands.CopyTransform, 
		FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::CopyTransform),
		FCanExecuteAction(),
		FGetActionCheckState(),
		FIsActionButtonVisible::CreateSP(this, &SMediaImageViewerOverlay::CanCopyTransform)
	);

	CommandList->MapAction(Commands.MipMinus, FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::AdjustMipLevel, static_cast<int8>(-1)));
	CommandList->MapAction(Commands.MipPlus,  FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::AdjustMipLevel, static_cast<int8>(1)));

	CommandList->MapAction(Commands.CopyColor, FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::CopyColor));

	CommandList->MapAction(
		Commands.AddToLibrary,
		FExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::AddToLibrary),
		FCanExecuteAction::CreateSP(this, &SMediaImageViewerOverlay::CanAddToLibrary)
	);
}

TSharedRef<SWidget> SMediaImageViewerOverlay::CreateOverlay(bool bInComparisonView)
{
	TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position);

	if (!ImageViewer.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	Overlay = SNew(SOverlay);

	TSharedPtr<SWidget> ImageViewerOverlay = ImageViewer.IsValid() 
		? ImageViewer->GetOverlayWidget(Position, Delegates->GetTab.Execute()) 
		: nullptr;

	Overlay->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility(SMediaImageViewerOverlay::HintText_GetVisibility())
			.Text(LOCTEXT("DropTargetMessage", "Drop supported asset or library item here."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.TextStyle(FAppStyle::Get(), "HintText")
		];

	if (bInComparisonView)
	{
		Overlay->AddSlot()
			[
				SNew(SMediaViewerDropTarget, Delegates.ToSharedRef())
				.Position(Position)
				.bComparisonView(bInComparisonView)
				.Tag(DropTargetName)
			];
	}
	else
	{
		Overlay->AddSlot()
			[
				SNew(SHorizontalBox)
				.Tag(DropTargetName)
				+ SHorizontalBox::Slot()
				.FillWidth(0.25f)
				.Padding(0.f, 0.f, 0.f, 20.f)
				[
					SNew(SMediaViewerDropTarget, Delegates.ToSharedRef())
					.Position(EMediaImageViewerPosition::First)
					.bComparisonView(bInComparisonView)
					.bForceComparisonView(true)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.5f)
				.Padding(10.f, 0.f, 10.f, 20.f)
				[
					SNew(SMediaViewerDropTarget, Delegates.ToSharedRef())
					.Position(EMediaImageViewerPosition::First)
					.bComparisonView(bInComparisonView)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.25f)
				.Padding(0.f, 0.f, 0.f, 20.f)
				[
					SNew(SMediaViewerDropTarget, Delegates.ToSharedRef())
					.Position(EMediaImageViewerPosition::Second)
					.bComparisonView(bInComparisonView)
				]
			];
	}

	if (ImageViewerOverlay.IsValid())
	{
		Overlay->AddSlot()
			[
				ImageViewerOverlay.ToSharedRef()
			];
	}

	if (bInComparisonView)
	{
		const EHorizontalAlignment HAlign = (Position == EMediaImageViewerPosition::First
			&& Delegates->GetActiveView.Execute() == EMediaImageViewerActivePosition::Both
			&& Delegates->GetABOrientation.Execute() == Orient_Horizontal)
			? HAlign_Right
			: HAlign_Left;

		Overlay->AddSlot()
			[
				SNew(SBox)
				.HAlign(HAlign)
				.VAlign(VAlign_Top)
				.Padding(5.f)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
					.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
					.ShadowOffset(FVector2D(1.f, 1.f))
					.Text(Position == EMediaImageViewerPosition::First ? INVTEXT("A") : INVTEXT("B"))
				]
			];
	}

	Overlay->AddSlot()
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(5.f)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FStyleColors::Foreground.GetSpecifiedColor())
				.ShadowColorAndOpacity(FStyleColors::Panel.GetSpecifiedColor())
				.ShadowOffset(FVector2D(1.f, 1.f))
				.Text(ImageViewer->GetInfo().DisplayName)
			]
		];

	return Overlay.ToSharedRef();
}

TSharedRef<SWidget> SMediaImageViewerOverlay::CreateStatusBar(const TSharedRef<FMediaViewerDelegates>& InDelegates)
{
	return SAssignNew(StatusBar, SMediaImageViewerStatusBar, Position, InDelegates);
}

bool SMediaImageViewerOverlay::IsOverlayEnabled() const
{
	return bOverlayEnabled;
}

void SMediaImageViewerOverlay::ToggleOverlay()
{
	bOverlayEnabled = !bOverlayEnabled;
	const EVisibility NewVisibility = bOverlayEnabled ? EVisibility::Visible : EVisibility::Collapsed;

	if (StatusBar.IsValid())
	{
		StatusBar->SetVisibility(NewVisibility);
	}

	if (Overlay.IsValid())
	{
		FChildren* Children = Overlay->GetChildren();

		for (int32 Index = 0; Index < Children->Num(); ++Index)
		{
			TSharedRef<SWidget> Child = Children->GetChildAt(Index);

			if (Child->GetTag() != DropTargetName)
			{
				Child->SetVisibility(NewVisibility);
			}
		}
	}
}

void SMediaImageViewerOverlay::MultiplyScaleAroundCursor(float InMultipler)
{
	const FVector2D CursorLocationBefore = GetImageViewerPixelCoordinates_Exact();

	SetScale(GetScale() * InMultipler);

	const FVector2D CursorLocationAfter = GetImageViewerPixelCoordinates_Exact();

	if (!(CursorLocationBefore - CursorLocationAfter).IsNearlyZero())
	{
		FVector Offset;
		Offset.X = CursorLocationAfter.X - CursorLocationBefore.X;
		Offset.Y = CursorLocationAfter.Y - CursorLocationBefore.Y;
		Offset.Z = 0;

		Offset *= GetScale();

		AddOffset(Offset);
	}
}

void SMediaImageViewerOverlay::ResetTransform()
{
	SetOffset(FVector::ZeroVector);
	SetRotation(FRotator::ZeroRotator);
	ScaleToFit(/* Use transform lock */ false);
}

FVector SMediaImageViewerOverlay::GetOffset() const
{
	if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
	{
		return ImageViewer->GetPaintSettings().Offset;
	}

	return FVector::ZeroVector;
}

void SMediaImageViewerOverlay::Try_AddOffset(FVector InOffset)
{
	if (Delegates->AreTransformsLocked.Execute())
	{
		Delegates->AddOffsetToAll.Execute(InOffset);
	}
	else
	{
		AddOffset(InOffset);
	}
}

void SMediaImageViewerOverlay::AddOffset(const FVector& InOffset)
{
	SetOffset(GetOffset() + InOffset);
}

void SMediaImageViewerOverlay::SetOffset(const FVector& InOffset)
{
	if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
	{
		ImageViewer->GetPaintSettings().Offset = InOffset;
	}
}

FRotator SMediaImageViewerOverlay::GetRotation() const
{
	if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
	{
		return ImageViewer->GetPaintSettings().Rotation;
	}

	return FRotator::ZeroRotator;
}

void SMediaImageViewerOverlay::Try_AddRotation(FRotator InRotation)
{
	if (Delegates->AreTransformsLocked.Execute())
	{
		Delegates->AddRotationToAll.Execute(InRotation);
	}
	else
	{
		AddRotation(InRotation);
	}
}

void SMediaImageViewerOverlay::AddRotation(const FRotator& InRotation)
{
	SetRotation(GetRotation() + InRotation);
}

void SMediaImageViewerOverlay::SetRotation(const FRotator& InRotation)
{
	if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
	{
		ImageViewer->GetPaintSettings().Rotation = InRotation;
	}
}

float SMediaImageViewerOverlay::GetScale() const
{
	if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
	{
		return ImageViewer->GetPaintSettings().Scale;
	}
	
	return 1.f;
}

void SMediaImageViewerOverlay::Try_SetScale(float InScale)
{
	if (Delegates->AreTransformsLocked.Execute())
	{
		Delegates->MultiplyScaleToAll.Execute(InScale / GetScale());
	}
	else
	{
		SetScale(InScale);
	}
}

void SMediaImageViewerOverlay::SetScale(float InScale)
{
	if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
	{
		ImageViewer->GetPaintSettings().Scale = InScale;
	}
}

void SMediaImageViewerOverlay::MultiplyScale(float InMultiple)
{
	SetScale(GetScale() * InMultiple);
}

void SMediaImageViewerOverlay::ScaleToFit(bool bInUseTransformLock)
{
	TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position);

	if (!ImageViewer.IsValid())
	{
		return;
	}

	const FVector2D LocalSize = Delegates->GetViewerSize.Execute();

	if (FMath::IsNearlyZero(LocalSize.X) || FMath::IsNearlyZero(LocalSize.Y))
	{
		return;
	}

	const FIntPoint& ImageSize = ImageViewer->GetInfo().Size;

	const float ScaleX = LocalSize.X / static_cast<float>(ImageSize.X);
	const float ScaleY = LocalSize.Y / static_cast<float>(ImageSize.Y);

	if (ScaleX < ScaleY)
	{
		if (bInUseTransformLock)
		{
			Try_SetScale(ScaleX);
		}
		else
		{
			SetScale(ScaleX);
		}
	}
	else
	{
		if (bInUseTransformLock)
		{
			Try_SetScale(ScaleY);
		}
		else
		{
			SetScale(ScaleY);
		}
	}
}

bool SMediaImageViewerOverlay::CanResetTransform()
{
	return Delegates->GetActiveView.Execute() == EMediaImageViewerActivePosition::Both;
}

void SMediaImageViewerOverlay::Try_ResetTransform()
{
	if (Delegates->AreTransformsLocked.Execute())
	{
		Delegates->ResetTransformToAll.Execute();
	}
	else
	{
		ResetTransform();
	}
}

void SMediaImageViewerOverlay::Try_SetTransform(FVector InOffset, FRotator InRotation, float InScale)
{
	if (Delegates->AreTransformsLocked.Execute())
	{
		Delegates->SetTransformToAll.Execute(InOffset, InRotation, InScale);
	}
	else
	{
		SetOffset(InOffset);
		SetRotation(InRotation);
		SetScale(InScale);
	}
}

bool SMediaImageViewerOverlay::CanCopyTransform()
{
	return Delegates->GetActiveView.Execute() == EMediaImageViewerActivePosition::Both;
}

void SMediaImageViewerOverlay::CopyTransform()
{
	Delegates->SetTransformToAll.Execute(GetOffset(), GetRotation(), GetScale());
}

uint8 SMediaImageViewerOverlay::GetMipLevel() const
{
	if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
	{
		return ImageViewer->GetPaintSettings().MipLevel;
	}

	return 0;
}

void SMediaImageViewerOverlay::AdjustMipLevel(int8 InAdjustment)
{
	const uint8 CurrentMipLevel = GetMipLevel();

	if (-InAdjustment > CurrentMipLevel)
	{
		SetMipLevel(0);
	}
	else
	{
		SetMipLevel(CurrentMipLevel + InAdjustment);
	}
}

void SMediaImageViewerOverlay::SetMipLevel(uint8 InMipLevel)
{
	if (TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position))
	{
		if (InMipLevel < ImageViewer->GetInfo().MipCount)
		{
			ImageViewer->GetPaintSettings().MipLevel = InMipLevel;
		}
	}
}

void SMediaImageViewerOverlay::CopyColor()
{
	TSharedPtr<FMediaImageViewer> ImageViewer = Delegates->GetImageViewer.Execute(Position);

	if (!ImageViewer.IsValid() || !ImageViewer->IsValid())
	{
		return;
	}

	const FIntPoint PixelCoordinates = Delegates->GetPixelCoordinates.Execute(Position);

	if (PixelCoordinates.X < 0 || PixelCoordinates.Y < 0)
	{
		return;
	}

	const FIntPoint& ImageSize = ImageViewer->GetInfo().Size;

	if (PixelCoordinates.X >= ImageSize.X || PixelCoordinates.Y >= ImageSize.Y)
	{
		return;
	}

	const TOptional<TVariant<FColor, FLinearColor>> PixelColor = ImageViewer->GetPixelColor(PixelCoordinates, ImageViewer->GetPaintSettings().MipLevel);

	if (!PixelColor.IsSet())
	{
		return;
	}

	const TVariant<FColor, FLinearColor>& PixelColorValue = PixelColor.GetValue();

	if (const FColor* Color = PixelColorValue.TryGet<FColor>())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Color->ToString());
	}

	if (const FLinearColor* ColorLinear = PixelColorValue.TryGet<FLinearColor>())
	{
		FPlatformApplicationMisc::ClipboardCopy(*ColorLinear->ToString());
	}
}

bool SMediaImageViewerOverlay::CanAddToLibrary() const
{
	if (!CachedItem.IsValid())
	{
		return false;
	}

	TSharedRef<FMediaViewerLibrary> Library = Delegates->GetLibrary.Execute();

	return !Library->FindItemByValue(CachedItem->GetItemType(), CachedItem->GetStringValue()).IsValid();
}

void SMediaImageViewerOverlay::AddToLibrary()
{
	if (!CachedItem.IsValid())
	{
		return;
	}

	TSharedRef<FMediaViewerLibrary> Library = Delegates->GetLibrary.Execute();

	Library->AddItemToGroup(CachedItem.ToSharedRef());
}

TSharedRef<SWidget> SMediaImageViewerOverlay::CreateMenu()
{
	FMenuBuilder MenuBuilder(true, CommandList, nullptr, false, &FAppStyle::Get(), false);

	MenuBuilder.BeginSection(TEXT("Image"));
	{
		MenuBuilder.AddMenuEntry(FMediaViewerCommands::Get().ResetTransform);
		MenuBuilder.AddMenuEntry(FMediaViewerCommands::Get().CopyTransform);
		MenuBuilder.AddMenuEntry(FMediaViewerCommands::Get().AddToLibrary);
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.BeginSection(TEXT("Media Viewer"));
	{
		MenuBuilder.AddMenuEntry(FMediaViewerCommands::Get().ToggleOverlay);
		MenuBuilder.AddMenuEntry(FMediaViewerCommands::Get().ToggleLockedTransform);
		MenuBuilder.AddMenuEntry(FMediaViewerCommands::Get().ResetAllTransforms);
		MenuBuilder.AddMenuEntry(FMediaViewerCommands::Get().SwapAB);
	}

	return MenuBuilder.MakeWidget();
}

EVisibility SMediaImageViewerOverlay::HintText_GetVisibility() const
{
	if (FSlateApplication::Get().IsDragDropping())
	{
		return EVisibility::Collapsed;
	}

	TSharedPtr<FMediaImageViewer> ImageViewer = GetImageViewer();
	
	return (ImageViewer.IsValid() && ImageViewer->GetInfo().Id != FNullImageViewer::GetNullImageViewer()->GetInfo().Id)
		? EVisibility::Collapsed
		: EVisibility::Visible;
}

} // UE::MediaViewer::Private

#undef LOCTEXT_NAMESPACE
