// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Animation/CurveSequence.h"


class SSegmentedProgressBar : public SCompoundWidget
{
public:
	enum class EState : uint8
	{
		None,
		Busy,
		Canceled,
		Completed,
		Failed,
		Pending,
	};


	struct FSlot : public TSlotBase<FSlot>
	{
	public:
		FSlot()
			: TSlotBase<FSlot>()
			, Image(nullptr)
		{}

		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
			SLATE_ATTRIBUTE(const FSlateBrush*, Image)
			SLATE_ATTRIBUTE(EState, State)
			SLATE_ATTRIBUTE(FText, ToolTipText)
		SLATE_SLOT_END_ARGS()

		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs)
		{
			TSlotBase<FSlot>::Construct(SlotOwner, MoveTemp(InArgs));
			if (InArgs._Image.IsSet())
			{
				Image = MoveTemp(InArgs._Image);
			}
			if (InArgs._State.IsSet())
			{
				State = MoveTemp(InArgs._State);
			}
			if (InArgs._ToolTipText.IsSet())
			{
				ToolTipText = MoveTemp(InArgs._ToolTipText);
			}
		}

	protected:
		TAttribute<const FSlateBrush*> Image;
		TAttribute<EState> State;
		TAttribute<FText> ToolTipText;

		friend SSegmentedProgressBar;
	};

	static FSlot::FSlotArguments Slot()
	{
		return FSlot::FSlotArguments(MakeUnique<FSlot>());
	}

	SLATE_BEGIN_ARGS(SSegmentedProgressBar){}
		SLATE_SLOT_ARGUMENT(FSlot, Slots)
	SLATE_END_ARGS()

	using FScopedWidgetSlotArguments = typename TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	FScopedWidgetSlotArguments AddSlot(bool bRebuildChildren = true)
	{
		return InsertSlot(INDEX_NONE, bRebuildChildren);
	}

	FScopedWidgetSlotArguments InsertSlot(int32 Index = INDEX_NONE, bool bRebuildChildren = true);

	FSlot& GetSlot(int32 SlotIndex)
	{
		return Children[SlotIndex];
	}

	const FSlot& GetSlot(int32 SlotIndex) const
	{
		return Children[SlotIndex];
	}

	int32 NumSlots() const
	{
		return Children.Num();
	}

	void ClearChildren()
	{
		Children.Empty();
	}

	SSegmentedProgressBar()
		: Children(this)
	{
		bCanSupportFocus = false;
	}



	void Construct(const FArguments& Arguments);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
	void RebuildChildren();
	void AddChildSeparatorBar( TSharedPtr<class SHorizontalBox> SlotBox, int32 ChildIndex ) const;
	TSharedRef<SWidget> ConstructChild( const FSlot& Slot, int32 ChildIndex ) const;

	void CacheChildStates();
	EState GetChildState( int32 ChildIndex ) const;

	FLinearColor GetSeparatorBarColor( int32 ChildIndex ) const;
	FSlateColor GetCircleColor( int32 ChildIndex ) const;
	FSlateColor GetIconColor( int32 ChildIndex ) const;
	EVisibility GetFullCircleVisibility( int32 ChildIndex ) const;
	EVisibility GetOuterCircleVisibility( int32 ChildIndex ) const;
	EVisibility GetProgressCircleVisibility( int32 ChildIndex ) const;
	EVisibility GetOverlayVisibility( int32 ChildIndex ) const;
	const FSlateBrush* GetOverlayIcon( int32 ChildIndex ) const;
	TOptional<FSlateRenderTransform> GetProgressCircleTransform( int32 ChildIndex ) const;

	TPanelChildren<FSlot> Children;
	TArray<EState> CachedChildStates;
	EState CachedOverallState = EState::None;

	FCurveSequence ThrobberAnimation;
	const float LineSize = 4;
};
