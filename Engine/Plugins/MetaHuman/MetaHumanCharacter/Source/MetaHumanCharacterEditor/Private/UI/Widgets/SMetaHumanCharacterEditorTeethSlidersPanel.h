// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterEditorSubsystem.h"
#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
struct FSlateBrush;
class SConstraintCanvas;
class SSlider;

DECLARE_DELEGATE_RetVal_OneParam(TOptional<float>, FOnGetTeethSliderPropertyValue, FProperty*);
DECLARE_DELEGATE_ThreeParams(FOnTeethSliderPropertyValueChanged, float InValue, bool bInIsInteractive, FProperty*);
DECLARE_DELEGATE_TwoParams(FOnTeethSliderValueChanged, float InValue, bool bInIsInteractive);
DECLARE_DELEGATE_OneParam(FOnTeethSliderPropertyEdited, FProperty*);

/** Slider used for specifically handling the teeth properties value change. */
class SMetaHumanCharacterEditorTeethSlider : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorTeethSlider)
		: _MinValue(0.f)
		, _MaxValue(1.f)
		, _Orientation(EOrientation::Orient_Horizontal)
		{}

		/** A value that drives where the slider handle appears. */
		SLATE_ATTRIBUTE(float, Value)

		/** The minimum value that can be specified by using the slider. */
		SLATE_ARGUMENT(float, MinValue)

		/** The maximum value that can be specified by using the slider. */
		SLATE_ARGUMENT(float, MaxValue)

		/** The slider's orientation. */
		SLATE_ARGUMENT(EOrientation, Orientation)

		/** Called when mouse capure begins. */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

		/** Called when the slider value has changed. */
		SLATE_EVENT(FOnTeethSliderValueChanged, OnValueChanged)

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

private:
	/** Called when the value of the slider changes. */
	void OnValueChanged(float NewValue);

	/** Called when mouse capture on the slider begins. */
	void OnMouseCaptureBegin();

	/** Called when mouse capture on the slider ends. */
	void OnMouseCaptureEnd();

	/** Gets the slider's elipse brush. */
	const FSlateBrush* GetElipseBrush() const;

	/** Gets the visibility of slider's arrow. */
	EVisibility GetArrowVisibility() const;

	/** Reference to this widget's slider. */
	TSharedPtr<SSlider> Slider;

	/** True if the slider is being dragged. */
	bool bIsDragging =  false;

	/** Slate arguments. */
	FOnTeethSliderValueChanged OnValueChangedDelegate;
	FSimpleDelegate OnMouseCaptureBeginDelegate;
	EOrientation Orientation;
	float MinValue;
	float MaxValue;
};

/** Widget used to display the Teeth Sliders for the different editable properties. */
class SMetaHumanCharacterEditorTeethSlidersPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorTeethSlidersPanel)
		{}
		
		/** Called to get the teeth slider value. */
		SLATE_EVENT(FOnGetTeethSliderPropertyValue, OnGetTeethSliderValue)

		/** Called when a teeth slider value has changed. */
		SLATE_EVENT(FOnTeethSliderPropertyValueChanged, OnTeethSliderValueChanged)

		/** Called when a teeth property is edited. */
		SLATE_EVENT(FOnTeethSliderPropertyEdited, OnTeethSliderPropertyEdited)

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

private:
	/** Makes the Teeth Slider main canvas widget. */
	void MakeTeethSlidersCanvas();

	/** Creates the slider for showing the Teeth properties. */
	TSharedRef<SWidget> CreateTeethPropertySlider(FProperty* Property, EOrientation Orientation);

	/** Gets the value of the teeth slider assigned to the given property. */
	float GetTeethSliderValue(FProperty* Property) const;

	/** Called when the value of a teeth slider has changed. */
	void OnTeethSliderValueChanged(const float Value, bool bIsInteractive, FProperty* Property);

	/** Called when the mouse capture of a teeth slider begins. */
	void OnTeethSliderMouseCaptureBegin(FProperty* Property);

	/** Reference to the canvas which contains all the Teeth Sliders. */
	TSharedPtr<SConstraintCanvas> TeethSlidersCanvas;

	/** Slate arguments. */
	FOnGetTeethSliderPropertyValue OnGetTeethSliderPropertyValueDelegate;
	FOnTeethSliderPropertyValueChanged OnTeethSliderValueChangedDelegate;
	FOnTeethSliderPropertyEdited OnTeethSliderPropertyEditedDelegate;
};
