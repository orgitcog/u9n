// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSegmentedProgressBar.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Styling/ProjectLauncherStyle.h"
#include "Styling/StyleColors.h"
#include "Layout/LayoutUtils.h"

#define LOCTEXT_NAMESPACE "SSegmentedProgressBar"

SSegmentedProgressBar::FScopedWidgetSlotArguments SSegmentedProgressBar::InsertSlot(int32 Index, bool bRebuildChildren)
{
	if (bRebuildChildren)
	{
		TWeakPtr<SSegmentedProgressBar> AsWeak = SharedThis(this);
		return FScopedWidgetSlotArguments { MakeUnique<FSlot>(), this->Children, Index, [AsWeak](const FSlot*, int32)
		{
			if (TSharedPtr<SSegmentedProgressBar> SharedThis = AsWeak.Pin())
			{
				SharedThis->RebuildChildren();
			}
		}};
	}
	else
	{
		return FScopedWidgetSlotArguments(MakeUnique<FSlot>(), this->Children, Index);
	}
}



void SSegmentedProgressBar::Construct( const FArguments& InArgs )
{
	ThrobberAnimation = FCurveSequence(0.0f, 1.0f);
	ThrobberAnimation.Play(AsShared(), true);

	Children.AddSlots(MoveTemp(const_cast<TArray<typename FSlot::FSlotArguments>&>(InArgs._Slots)));	
	RebuildChildren();
}



void SSegmentedProgressBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	CacheChildStates();
}



void SSegmentedProgressBar::CacheChildStates()
{
	CachedChildStates.Reset();
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ChildIndex++)
	{
		EState ChildState = Children[ChildIndex].State.Get();

		// show us as busy as soon as the previous task is completed - just looks nicer
		if (ChildState == EState::Pending && (ChildIndex == 0 || CachedChildStates[ChildIndex-1] == EState::Completed))
		{
			ChildState = EState::Busy;
		}

		CachedChildStates.Add( ChildState );
	}

	if (CachedChildStates.Num() == 0 )
	{
		CachedOverallState = EState::None;
	}
	else
	{
		CachedOverallState = CachedChildStates.Last();
	}
}



SSegmentedProgressBar::EState SSegmentedProgressBar::GetChildState( int32 ChildIndex ) const
{
	if (CachedChildStates.IsValidIndex(ChildIndex))
	{
		return CachedChildStates[ChildIndex];
	}
	
	return EState::None;
}



void SSegmentedProgressBar::RebuildChildren()
{
	CacheChildStates();

	TSharedPtr<SHorizontalBox> SlotBox;
	ChildSlot
	[
		SAssignNew(SlotBox, SHorizontalBox)
	];
	

	// construct child widgets as needed
	const int32 NumChildren = Children.Num();
	for ( int32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++ )
	{
		TSharedRef<SWidget> Child = Children[ChildIndex].GetWidget();
		FSlot* ChildSlotPtr = &Children[ChildIndex];

		if (Child == SNullWidget::NullWidget)
		{
			Child = ConstructChild(*ChildSlotPtr, ChildIndex);
		}


		// separator bar between previous and current
		if (ChildIndex > 0)
		{
			AddChildSeparatorBar(SlotBox, ChildIndex);
		}

		// task item
		SlotBox->AddSlot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.AutoWidth()
		[
			Child
		];
	}
}



void SSegmentedProgressBar::AddChildSeparatorBar( TSharedPtr<SHorizontalBox> SlotBox, int32 ChildIndex ) const
{
	SlotBox->AddSlot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Fill)
	.FillWidth(1)
	[
		SNew(SColorBlock)
		.Color( this, &SSegmentedProgressBar::GetSeparatorBarColor, ChildIndex )
		.Size( FVector2D(32, LineSize) )
	];
}



TSharedRef<SWidget> SSegmentedProgressBar::ConstructChild( const FSlot& Slot, int32 ChildIndex ) const
{
	return SNew(SOverlay)

	// full circle (only shown when fully complete or cancelled)
	+SOverlay::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SImage)
		.Image( FProjectLauncherStyle::Get().GetBrush("FullCircle") )
		.ColorAndOpacity( this, &SSegmentedProgressBar::GetCircleColor, ChildIndex)
		.Visibility( this, &SSegmentedProgressBar::GetFullCircleVisibility, ChildIndex )

	]

	// outer circle
	+SOverlay::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SImage)
		.DesiredSizeOverride(FVector2D(36,36))
		.Image( FProjectLauncherStyle::Get().GetBrush("OuterCircle") )
		.ColorAndOpacity( this, &SSegmentedProgressBar::GetCircleColor, ChildIndex )
		.Visibility( this, &SSegmentedProgressBar::GetOuterCircleVisibility, ChildIndex )
	]

	// outer busy circle
	+SOverlay::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SImage)
		.DesiredSizeOverride(FVector2D(36,36))
		.Image( FProjectLauncherStyle::Get().GetBrush("OuterCircle.Busy") )
		.ColorAndOpacity( FProjectLauncherStyle::Get().GetSlateColor("State.Busy") )
		.Visibility( this, &SSegmentedProgressBar::GetProgressCircleVisibility, ChildIndex )
		.RenderTransform( this, &SSegmentedProgressBar::GetProgressCircleTransform, ChildIndex)
		.RenderTransformPivot(FVector2D(.5f,.5f))
	]

	// task icon
	+SOverlay::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SImage)
		.Image(Slot.Image)
		.ColorAndOpacity(this, &SSegmentedProgressBar::GetIconColor, ChildIndex)
		.DesiredSizeOverride(FVector2D(16,16))
		.ToolTipText(Slot.ToolTipText)
	]

	// task state overlay
	+SOverlay::Slot()
	.VAlign(VAlign_Bottom)
	.HAlign(HAlign_Right)
	[
		SNew(SImage)
		.Image(this, &SSegmentedProgressBar::GetOverlayIcon, ChildIndex)
		.Visibility( this, &SSegmentedProgressBar::GetOverlayVisibility, ChildIndex)
	]
	;
}


FLinearColor SSegmentedProgressBar::GetSeparatorBarColor( int32 ChildIndex ) const
{
	if (CachedOverallState == EState::Completed)
	{
		return FProjectLauncherStyle::Get().GetSlateColor("State.AllComplete").GetSpecifiedColor();
	}

	EState State = GetChildState(ChildIndex);
	switch (State)
	{
		case EState::Busy:		return FProjectLauncherStyle::Get().GetSlateColor("State.Busy").GetSpecifiedColor();
		case EState::Canceled:  return FProjectLauncherStyle::Get().GetSlateColor("State.Canceled").GetSpecifiedColor();
		case EState::Completed:
		case EState::Failed:	return FProjectLauncherStyle::Get().GetSlateColor("State.Complete").GetSpecifiedColor(); // line indicates previous step succeeded in this case
		case EState::Pending:	return FProjectLauncherStyle::Get().GetSlateColor("State.Pending").GetSpecifiedColor();
	}

	return FSlateColor::UseForeground().GetSpecifiedColor();
}

FSlateColor SSegmentedProgressBar::GetCircleColor( int32 ChildIndex ) const
{
	if (CachedOverallState == EState::Completed)
	{
		return FProjectLauncherStyle::Get().GetSlateColor("State.AllComplete");
	}

	EState State = GetChildState(ChildIndex);
	switch (State)
	{
		case EState::Busy:		return FProjectLauncherStyle::Get().GetSlateColor("State.Pending");
		case EState::Canceled:  return FProjectLauncherStyle::Get().GetSlateColor("State.Canceled");
		case EState::Completed:	return FProjectLauncherStyle::Get().GetSlateColor("State.Complete");
		case EState::Failed:	return FProjectLauncherStyle::Get().GetSlateColor("State.Error");
		case EState::Pending:	return FProjectLauncherStyle::Get().GetSlateColor("State.Pending");
	}

	return FSlateColor::UseForeground();
}





FSlateColor SSegmentedProgressBar::GetIconColor( int32 ChildIndex ) const
{
	if (CachedOverallState == EState::Completed)
	{
		return FStyleColors::White;
	}

	EState State = GetChildState(ChildIndex);
	if (State == EState::Canceled)
	{
		return FStyleColors::Hover2;
	}

	return FStyleColors::Foreground;
}


EVisibility SSegmentedProgressBar::GetFullCircleVisibility( int32 ChildIndex ) const
{
	if (CachedOverallState == EState::Completed || GetChildState(ChildIndex) == EState::Canceled)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}



TOptional<FSlateRenderTransform> SSegmentedProgressBar::GetProgressCircleTransform( int32 ChildIndex ) const
{
	if (GetChildState(ChildIndex) == EState::Busy)
	{
		const float DeltaAngle = ThrobberAnimation.GetLerp()*2*PI;
		return FSlateRenderTransform(FQuat2D(DeltaAngle));
	}

	FSlateRenderTransform Result;
	return Result;
}



EVisibility SSegmentedProgressBar::GetOuterCircleVisibility( int32 ChildIndex ) const
{
	if (CachedOverallState != EState::Completed && GetChildState(ChildIndex) != EState::Canceled)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}



EVisibility SSegmentedProgressBar::GetProgressCircleVisibility( int32 ChildIndex ) const
{
	if (CachedOverallState != EState::Completed && GetChildState(ChildIndex) == EState::Busy)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}




EVisibility SSegmentedProgressBar::GetOverlayVisibility( int32 ChildIndex ) const
{
	EState State = GetChildState(ChildIndex);
	if (State == EState::Completed || State == EState::Failed)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}



const FSlateBrush* SSegmentedProgressBar::GetOverlayIcon( int32 ChildIndex ) const
{
	if (CachedOverallState == EState::Completed)
	{
		return FProjectLauncherStyle::Get().GetBrush("BadgeOutlined.AllComplete");
	}

	EState State = GetChildState(ChildIndex);
	switch (State)
	{
		case EState::Completed: return FProjectLauncherStyle::Get().GetBrush("BadgeOutlined.Success");
		case EState::Failed:	return FProjectLauncherStyle::Get().GetBrush("BadgeOutlined.Error");
	}

	return FStyleDefaults::GetNoBrush();
}

#undef LOCTEXT_NAMESPACE