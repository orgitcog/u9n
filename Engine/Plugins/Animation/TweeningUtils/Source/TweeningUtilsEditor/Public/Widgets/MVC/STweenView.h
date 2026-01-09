// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

template<typename OptionalType> struct TOptional;

namespace UE::TweeningUtilsEditor
{
class FTweenModel;
enum class ETweenScaleMode : uint8;
	
/**
 * It bridges the STweenSlider, which acts as the view, and the model, FTweenModel, by interchanging data between them.
 * Acts as view in a Model-View-Controller architecture.
 */
class TWEENINGUTILSEDITOR_API STweenView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STweenView) {}
		/** Gets the tween model this view is driving. */
		SLATE_ATTRIBUTE(FTweenModel*, TweenModel)
		
		/** The icon to place in the slider button. */
		SLATE_ATTRIBUTE(const FSlateBrush*, SliderIcon)
		/** The main color. It tints the slider button and the points. */
		SLATE_ATTRIBUTE(FLinearColor, SliderColor)
		/** If set, an indication where to position the slider. Range [-1,1]. If unset, defaults to 0. Ignored if the user is dragging the slider. */
		SLATE_ATTRIBUTE(TOptional<float>, OverrideSliderPosition)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	/** The model that these controls are driving */
	TAttribute<FTweenModel*> TweenModelAttr;
	
	ETweenScaleMode GetBarRenderMode() const;
	
	void OnDragStarted() const;
	void OnDragEnded() const;
	void OnDragValueUpdated(float Value) const;
	void OnPointPicked(float Value) const;
	float MapSliderValueToBlendValue(float Value) const;
};
}
