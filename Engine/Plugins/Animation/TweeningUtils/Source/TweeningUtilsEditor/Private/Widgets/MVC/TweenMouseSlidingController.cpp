// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/MVC/TweenMouseSlidingController.h"

#include "HAL/IConsoleManager.h"
#include "Math/Models/TweenModel.h"
#include "Misc/Attribute.h"
#include "TweeningUtilsCommands.h"
#include "TweeningUtilsStyle.h"
#include "Widgets/TweenSliderStyle.h"

namespace UE::TweeningUtilsEditor
{
static TAutoConsoleVariable<float> CVarOverrideTweenSliderWidth(
	TEXT("CurveEditor.SliderMouseWidth"),
	-1.f,
	TEXT("Specify positive value to override the sliding width. Non-positive will result in the default being used.")
	);
	
FTweenMouseSlidingController::FTweenMouseSlidingController(
	TAttribute<float> InMaxSlideWidthAttr,
	TAttribute<FTweenModel*> InTweenModelAttr,
	const TSharedRef<FUICommandList>& InCommandList,
	TSharedPtr<FUICommandInfo> InDragSliderCommand
	)
	: FMouseSlidingController(
		TAttribute<float>::CreateLambda([Attr = MoveTemp(InMaxSlideWidthAttr)]()
		{
			const float CVarOverride = CVarOverrideTweenSliderWidth.GetValueOnGameThread();
			return CVarOverride <= 0.f ? Attr.Get() : CVarOverride;
		}),
		InCommandList, MoveTemp(InDragSliderCommand)
		)
	, TweenModelAttr(MoveTemp(InTweenModelAttr))
{
	OnStartSliding().AddRaw(this, &FTweenMouseSlidingController::OnSliderStartMove);
	OnStopSliding().AddRaw(this, &FTweenMouseSlidingController::OnSliderEndMove);
	OnUpdateSliding().AddRaw(this, &FTweenMouseSlidingController::OnSliderMove);
}

FTweenMouseSlidingController::FTweenMouseSlidingController(TAttribute<FTweenModel*> InTweenModelAttr, const TSharedRef<FUICommandList>& InCommandList)
	: FTweenMouseSlidingController(
		FTweeningUtilsStyle::Get().GetWidgetStyle<FTweenSliderStyle>("TweenSlider").BarDimensions.X,
		MoveTemp(InTweenModelAttr),
		InCommandList,
		FTweeningUtilsCommands::Get().DragAnimSliderTool
	)
{}

void FTweenMouseSlidingController::OnSliderStartMove()
{
	TweenModelAttr.Get()->StartBlendOperation();
}

void FTweenMouseSlidingController::OnSliderEndMove()
{
	CurrentSliderPosition.Reset();
	TweenModelAttr.Get()->StopBlendOperation();
}
	
void FTweenMouseSlidingController::OnSliderMove(float InValue)
{
	CurrentSliderPosition = InValue;
	TweenModelAttr.Get()->BlendValues(InValue);
}
}
