// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Tools/MetaHumanCharacterEditorBodyEditingTools.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Views/SListView.h"
#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"

template<typename NumericType>
class SMetaHumanCharacterEditorParametricSpinBox : public SCompoundWidget
{
public:
	/** Notification for numeric value change */
	DECLARE_DELEGATE_TwoParams(FOnValueChanged, NumericType /*NewValue*/, bool bCommit);

	/** Optional customization of the display value based on the current value. */
	DECLARE_DELEGATE_RetVal_OneParam(TOptional<FText>, FOnGetDisplayValue, NumericType);

public:
	SLATE_BEGIN_ARGS( SMetaHumanCharacterEditorParametricSpinBox<NumericType> )
		: _MinValue(TNumericLimits<NumericType>::Lowest())
		, _MaxValue(TNumericLimits<NumericType>::Max())
		{}

		/** Font color and opacity */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
		/** Tooltip */
		SLATE_ATTRIBUTE(FText, ToolTip)
		/** Style to use for the spin box within this widget */
		SLATE_STYLE_ARGUMENT( FSpinBoxStyle, SpinBoxStyle )
		/** Is enabled */
		SLATE_ARGUMENT( bool, IsEnabled )

		/** The value that should be displayed.  This value is optional in the case where a value cannot be determined */
		SLATE_ATTRIBUTE( NumericType, Value )
		/** The minimum value that can be entered */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MinValue )
		/** The maximum value that can be entered */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MaxValue )

		/** Called whenever the text is changed programmatically or interactively by the user */
		SLATE_EVENT( FOnValueChanged, OnValueChanged )
		/** Called right before the slider begins to move */
		SLATE_EVENT( FSimpleDelegate, OnBeginSliderMovement )
		/** Called right after the slider handle is released by the user */
		SLATE_EVENT( FOnValueChanged, OnEndSliderMovement )
		/** Called to allow customization of what text is displayed when not typing. */
		SLATE_EVENT(FOnGetDisplayValue, OnGetDisplayValue)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	float GetSliderValue() const;
	void OnSliderValueChanged(float NewValue, bool bCommit) const;
	FText GetOutputValueText() const;
	TOptional<FText> GetDisplayText(float Value) const;

	/** Attribute for getting the value. */
	TAttribute<NumericType> ValueAttribute;
	/** Delegate to call when the value changes */
	FOnValueChanged OnValueChanged;
	/** Delegate to get display text */
	FOnGetDisplayValue OnGetDisplayValue;

	const float SliderDistance = 100.f;
	const float SliderMinValue = 0.0f;
	const float SliderMaxValue = 100.f;
	NumericType MinValue = 0.0;
	NumericType MaxValue = 100.0;

	TSharedPtr<INumericTypeInterface<NumericType>> InterfaceAttr;
	TSharedPtr<SSpinBox<float>> SpinBox;
};

/** Displays a widget for a parametric constraint in body parametric tool */
class SMetaHumanCharacterEditorParametricConstraintView : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_ThreeParams(FOnParametricConstraintChanged, float TargetMeasurement, bool bIsPinned, bool bCommit);

public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorParametricConstraintView)
		: _IsEnabled(true)
		{}

		SLATE_ATTRIBUTE(float, TargetMeasurement)
		
		SLATE_ATTRIBUTE(float, ActualMeasurement)

		SLATE_ATTRIBUTE(bool, IsPinned)

		SLATE_ARGUMENT(EVisibility, PinVisibility)

		SLATE_ARGUMENT(bool, IsEnabled)

		SLATE_ARGUMENT(FName, ConstraintName)
		
		SLATE_ARGUMENT(float, MinValue)

		SLATE_ARGUMENT(float, MaxValue)

		SLATE_EVENT(FSimpleDelegate, OnBeginConstraintEditing)

		SLATE_ATTRIBUTE(FText, ToolTip)

		/** Called whenever constraint changes*/
		SLATE_EVENT(FOnParametricConstraintChanged, OnParametricConstraintChanged)

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

private:
	TOptional<FText> GetDisplayText(float TargetValue) const;
	float GetParametricValue() const;
	void OnBeginConstraintEditing() const;
	void OnConstraintTargetChanged(const float Value, bool bCommit) const;
	ECheckBoxState GetConstraintChecked() const;
	void OnConstraintPinnedChanged(ECheckBoxState CheckState) const;

	FName ConstraintName;
	TAttribute<float> TargetMeasurement;
	TAttribute<float> ActualMeasurement;
	TAttribute<bool> IsPinned;
	FSimpleDelegate OnBeginConstraintEditingDelegate;
	FOnParametricConstraintChanged OnParametricConstraintChangedDelegate;
};

class SMetaHumanCharacterEditorParametricConstraintsPanel : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnConstraintChanged, bool /*bCommit*/);

public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorParametricConstraintsPanel)
		: _ListItemsSource(nullptr)
		, _Padding(FMargin(0.f))
	{}

		SLATE_ARGUMENT(const TArray<FMetaHumanCharacterBodyConstraintItemPtr>*, ListItemsSource)

		SLATE_ARGUMENT(bool, DiagnosticsView)

		SLATE_ATTRIBUTE(FText, Label)

		SLATE_ATTRIBUTE(FMargin, Padding)

		SLATE_EVENT(FSimpleDelegate, OnBeginConstraintEditing)
		SLATE_EVENT(FOnConstraintChanged, OnConstraintsChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<ITableRow> MakeConstraintRowWidget(FMetaHumanCharacterBodyConstraintItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGroupPinCheckStateChanged(ECheckBoxState CheckState);
	void OnBeginConstraintEditing();
	void OnConstraintChanged(bool bCommit);
	ECheckBoxState GetGroupPinCheckState() const;

	TSharedPtr<SListView<FMetaHumanCharacterBodyConstraintItemPtr>> ListView;
	TArray<FMetaHumanCharacterBodyConstraintItemPtr> ItemsSource;
	FSimpleDelegate OnBeginConstraintEditingDelegate;
	FOnConstraintChanged OnConstraintsChangedDelegate;
	bool DiagnosticView = false;
};
