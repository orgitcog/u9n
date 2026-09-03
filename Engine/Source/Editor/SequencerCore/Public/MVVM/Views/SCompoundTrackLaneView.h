// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"
#include "Layout/Children.h"

struct FGeometry;

class FPaintArgs;
class FSlateWindowElementList;

namespace UE::Sequencer
{

class STrackLane;
class STrackAreaView;
class ITrackLaneWidget;
class ITrackLaneWidgetSpace;

/**
 * 
 */
class SEQUENCERCORE_API SCompoundTrackLaneView
	: public SPanel
{
public:

	SLATE_BEGIN_ARGS(SCompoundTrackLaneView){}
	SLATE_END_ARGS()

	SCompoundTrackLaneView();
	~SCompoundTrackLaneView();

	UE_DEPRECATED(5.6, "Please use the TSharedPtr<ITrackLaneWidgetSpace> overload")
	void Construct(const FArguments& InArgs)
	{
		Construct(InArgs, nullptr);
	}

	void Construct(const FArguments& InArgs, TSharedPtr<ITrackLaneWidgetSpace> InTrackLaneWidgetSpace);

	void AddWeakWidget(TSharedPtr<ITrackLaneWidget> InWidget, TWeakPtr<STrackLane> InOwningLane);
	void AddStrongWidget(TSharedPtr<ITrackLaneWidget> InWidget, TWeakPtr<STrackLane> InOwningLane);

	/*~ SPanel Interface */
	void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	FVector2D ComputeDesiredSize(float) const override;
	FChildren* GetChildren() override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:

	struct FSlot : TSlotBase<FSlot>
	{
		FSlot(TSharedPtr<ITrackLaneWidget> InInterface, TWeakPtr<STrackLane> InOwningLane);
		FSlot(TWeakPtr<ITrackLaneWidget> InWeakInterface, TWeakPtr<STrackLane> InOwningLane);

		TSharedPtr<ITrackLaneWidget> GetInterface() const
		{
			return Interface ? Interface : WeakInterface.Pin();
		}

		TSharedPtr<ITrackLaneWidget> Interface;
		TWeakPtr<ITrackLaneWidget> WeakInterface;

		TWeakPtr<STrackLane> WeakOwningLane;
	};

	/** All the widgets in the panel */
	TPanelChildren<FSlot> Children;

	TWeakPtr<ITrackLaneWidgetSpace> WeakTrackLaneWidgetSpace;
};

} // namespace UE::Sequencer

