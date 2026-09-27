// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"
#include "UI/Widgets/SMetaHumanCharacterEditorComboBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

enum class ECheckBoxState : uint8;
class FProperty;
class FReply;
class FScopedTransaction;
class SScrollBox;
class SVerticalBox;
class UInteractiveTool;
class UInteractiveToolPropertySet;

/** View for displaying an interactive tool in the MetahHumanCharacter editor */
class SMetaHumanCharacterEditorToolView
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorToolView)
		{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UInteractiveTool* InTool);

protected:
	/** Gets the Tool this view is based on. */
	virtual UInteractiveTool* GetTool() const { return Tool.Get(); }

	/** Gets the properties of the Tool this view is based on. To implement in inherited classes. */
	virtual UInteractiveToolPropertySet* GetToolProperties() const = 0;

	/** Called in the default constructor. Contains the tool view specific implementation. To implement in inherited classes. */
	virtual void MakeToolView() = 0;

	// TODO: Convert these labels to FText

	/** Creates a numeric entry widget for the given Numeric Property, if valid. */
	TSharedRef<SWidget> CreatePropertyNumericEntry(FProperty* InProperty, void* PropertyContainerPtr, const FString& InLabelOverride = TEXT(""), const int32 FractionalDigits = 2);

	/** Creates a numeric entry widget for the given Numeric Property that displays the normalized range [InMinValue, InMaxValue] */
	TSharedRef<SWidget> CreatePropertyNumericEntryNormalized(FProperty* InProperty, void* InPropertyContainerPtr, float InMinValue, float InMaxValue, const FText& InLabelOverride = FText::GetEmpty());
	
	/** Creates a spin box widget for the given Numeric Property, if valid. */
	TSharedRef<SWidget> CreatePropertySpinBoxWidget(const FString Label, FProperty* Property, void* PropertyContainerPtr, const int32 FractionalDigits = 2);

	/** Creates a spin box widget for the given Numeric Property showing the normalized value in the UI. When the value changes the value is converted to the range [InMinValue, InMaxValue] for storage */
	TSharedRef<SWidget> CreatePropertySpinBoxWidgetNormalized(FProperty* InProperty, void* InPropertyContainerPtr, float InMinValue, float InMaxValue, const FText& InLabelOverride = FText::GetEmpty());

	/** Creates a check box widget for the given Bool Property, if valid. */
	TSharedRef<SWidget> CreatePropertyCheckBoxWidget(const FString Label, FProperty* Property, void* PropertyContainerPtr);

	/** Creates a combo box widget for the given Enum Property, if valid. */
	template<typename TEnum>
	TSharedRef<SWidget> CreatePropertyComboBoxWidget(const FString Label, TEnum SelectedItem, FProperty* Property, void* PropertyContainerPtr)
	{
		if (!Property || !PropertyContainerPtr)
		{
			return SNullWidget::NullWidget;
		}

		return
			SNew(SVerticalBox)
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.FillWidth(.3f)
				.Padding(10.f, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Label))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.FillWidth(.7f)
				.Padding(4.f, 2.f, 40.f, 2.f)
				[
					SNew(SMetaHumanCharacterEditorComboBox<TEnum>)
					.InitiallySelectedItem(SelectedItem)
					.OnSelectionChanged(this, &SMetaHumanCharacterEditorToolView::OnEnumPropertyValueChanged, Property, PropertyContainerPtr)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Thickness(1.f)
			];
	}

	/** Creates a color picker widget for the given Color Property, if valid. */
	TSharedRef<SWidget> CreatePropertyColorPickerWidget(const FString Label, FProperty* Property, void* PropertyContainerPtr);
	
	/** Creates a color picker widget using the two properties that represent the UV values to sample a texture */
	TSharedRef<SWidget> CreatePropertyUVColorPickerWidget(FProperty* InPropertyU,
														  FProperty* InPropertyV,
														  void* InPropertyContainerPtr,
														  const FText& InColorPickerLabel,
														  TNotNull<UTexture2D*> InColorPickerTexture,
														  const FText& InULabelOverride = FText::GetEmpty(),
														  const FText& InVLabelOverride = FText::GetEmpty());

	/** Gets the check box state according to the given Bool Property, if valid. */
	ECheckBoxState IsPropertyCheckBoxChecked(FProperty* Property, void* PropertyContainerPtr) const;

	/** Called when the check box state changes according to the given Bool Property, if valid. */
	void OnPropertyCheckStateChanged(ECheckBoxState CheckState, FProperty* Property, void* PropertyContainerPtr);

	/** Called when the mouse button is pressed on the color block of a given Color Property, if valid. */
	FReply OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FProperty* Property, void* PropertyContainerPtr, const FString Label = TEXT(""));

	/** Gets the value of the given Numeric Property as an integer, if valid. */
	TOptional<int32> GetIntPropertyValue(FProperty* Property, void* PropertyContainerPtr) const;

	/** Gets the min value of the given Numeric Property as an integer, if valid. */
	TOptional<int32> GetIntPropertyMinValue(FProperty* Property) const;

	/** Gets the max value of the given Numeric Property as an integer, if valid. */
	TOptional<int32> GetIntPropertyMaxValue(FProperty* Property) const;

	/** Gets the value of the given Numeric Property as a float, if valid. */
	TOptional<float> GetFloatPropertyValue(FProperty* Property, void* PropertyContainerPtr) const;

	/** Gets the value of the given Numeric Property as a normalized float */
	TOptional<float> GetFloatPropertyValueNormalized(FProperty* InProperty, void* InPropertyContainerPtr, float InMinValue, float InMaxValue) const;

	/** Gets the min value of the given Numeric Property as a float, if valid. */
	TOptional<float> GetFloatPropertyMinValue(FProperty* Property) const;

	/** Gets the max value of the given Numeric Property as a float, if valid. */
	TOptional<float> GetFloatPropertyMaxValue(FProperty* Property) const;

	/** Gets the value of the given Bool Property, if valid. */
	bool GetBoolPropertyValue(FProperty* Property, void* PropertyContainerPtr) const;

	/** Gets the value of the given Enum or Byte Property, if valid. */
	uint64 GetEnumPropertyValue(FProperty* Property, void* PropertyContainerPtr) const;

	/** Gets the value of the given Color Property, if valid. */
	FLinearColor GetColorPropertyValue(FProperty* Property, void* PropertyContainerPtr) const;

	/** Called before the value of a Property gets changed. */
	void OnPreEditChangeProperty(FProperty* Property, const FString Label = TEXT(""));

	/** Called after the value of a Property has been changed. */
	void OnPostEditChangeProperty(FProperty* Property, bool bIsInteractive);

	/** Called when the value of a Numeric Property is changed, if valid. */
	void OnIntPropertyValueChanged(const int32 Value, bool bIsInteractive, FProperty* Property, void* PropertyContainerPtr);

	/** Called when the value of a Numeric Property is changed, if valid. */
	void OnFloatPropertyValueChanged(const float Value, bool bIsInteractive, FProperty* Property, void* PropertyContainerPtr);

	/** Called when the value of the Numeric Property is committed */
	void OnIntPropertyValueCommitted(int32 Value, ETextCommit::Type Type, FProperty* Property, void* PropertyContainerPtr);

	/** called when the value of a Normalized Numeric Property is changed */
	void OnFloatPropertyNormalizedValueChanged(float Value, float InMinValue, float InMaxValue, bool bIsInteractive, FProperty* InProperty, void* InPropertyContainerPtr);

	/** Called when the value of a Numeric Property is committed, if valid. */
	void OnFloatPropertyValueCommited(const float Value, ETextCommit::Type Type, FProperty* Property, void* PropertyContainerPtr);

	/** Called when the value of a Normalized Numeric Property is committed */
	void OnFloatPropertyNormalizedValueCommited(float InValue, ETextCommit::Type InType, float InMinValue, float InMaxValue, FProperty* InProperty, void* InPropertyContainerPtr);

	/** Called when the value of a Bool Property is changed, if valid. */
	void OnBoolPropertyValueChanged(const bool Value, FProperty* Property, void* PropertyContainerPtr);

	/** Called when the value of an Enum Property is changed, if valid. */
	void OnEnumPropertyValueChanged(uint8 Value, FProperty* Property, void* PropertyContainerPtr);

	/** Called when the value of a Color Property is changed, if valid. */
	void OnColorPropertyValueChanged(FLinearColor Color, bool bIsInteractive, FProperty* Property, void* PropertyContainerPtr);

	/** Returns the visibility of a property. */
	virtual EVisibility GetPropertyVisibility(FProperty* Property, void* PropertyContainerPtr) const;
	
	/** Returns whether the property is enabled */
	virtual bool IsPropertyEnabled(FProperty* Property, void* PropertyContainerPtr) const;

	/** Unique pointer to the property change transaction. */
	TUniquePtr<FScopedTransaction> PropertyChangeTransaction;

	/** Reference to the tool view main vertical box. */
	TSharedPtr<SVerticalBox> ToolViewMainBox;

	/** Reference to the tool view scroll box. */
	TSharedPtr<SScrollBox> ToolViewScrollBox;

	/** Weak reference to the Tool this view is based on. */
	TWeakObjectPtr<UInteractiveTool> Tool;

	/** Desired Height ratio multiplier for adjusting the tool view size. */
	float ToolViewHeightRatio = 1.f;
};
