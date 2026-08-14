// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorToolView.h"

#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h" 
#include "InteractiveTool.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/StyleColors.h"
#include "UI/Widgets/SUVColorPicker.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorToolView"

void SMetaHumanCharacterEditorToolView::Construct(const FArguments& InArgs, UInteractiveTool* InTool)
{
	if (!ensureMsgf(InTool, TEXT("Invalid interactive tool, can't constuct the tool view correctly.")))
	{
		return;
	}

	Tool = InTool;
	ToolViewHeightRatio = 0.6f;

	ChildSlot
		[
			SAssignNew(ToolViewMainBox, SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SBox)
				[
					SAssignNew(ToolViewScrollBox, SScrollBox)
					.Orientation(Orient_Vertical)
				]
			]
		];

	MakeToolView();
}

TSharedRef<SWidget> SMetaHumanCharacterEditorToolView::CreatePropertyNumericEntry(FProperty* InProperty, void* PropertyContainerPtr, const FString& InLabelOverride, const int32 InFractionalDigits)
{
	if (!InProperty || !PropertyContainerPtr || !CastField<FNumericProperty>(InProperty))
	{
		return SNullWidget::NullWidget;
	}

	if (CastField<FNumericProperty>(InProperty)->IsInteger())
		{
			return 
				SNew(SNumericEntryBox<int32>)
					.AllowSpin(true)
					.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.MinValue(this, &SMetaHumanCharacterEditorToolView::GetIntPropertyMinValue, InProperty)
					.MaxValue(this, &SMetaHumanCharacterEditorToolView::GetIntPropertyMaxValue, InProperty)
					.MinSliderValue(this, &SMetaHumanCharacterEditorToolView::GetIntPropertyMinValue, InProperty)
					.MaxSliderValue(this, &SMetaHumanCharacterEditorToolView::GetIntPropertyMaxValue, InProperty)
					.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
					.Value(this, &SMetaHumanCharacterEditorToolView::GetIntPropertyValue, InProperty, PropertyContainerPtr)
					.OnBeginSliderMovement(this, &SMetaHumanCharacterEditorToolView::OnPreEditChangeProperty, InProperty, InLabelOverride)
					.OnEndSliderMovement(this, &SMetaHumanCharacterEditorToolView::OnIntPropertyValueChanged, /* bIsInteractive */ false, InProperty, PropertyContainerPtr)
					.OnValueChanged(this, &SMetaHumanCharacterEditorToolView::OnIntPropertyValueChanged, /* bIsDragging */ true, InProperty, PropertyContainerPtr)
					.OnValueCommitted(this, &SMetaHumanCharacterEditorToolView::OnIntPropertyValueCommitted, InProperty, PropertyContainerPtr)
					.Visibility(this, &SMetaHumanCharacterEditorToolView::GetPropertyVisibility, InProperty, PropertyContainerPtr)
					.IsEnabled(this, &SMetaHumanCharacterEditorToolView::IsPropertyEnabled, InProperty, PropertyContainerPtr)
					.PreventThrottling(true)
					.LabelPadding(FMargin(3))
					.LabelLocation(SNumericEntryBox<int32>::ELabelLocation::Inside)
					.Label()
					[
						SNumericEntryBox<int32>::BuildNarrowColorLabel(FLinearColor::Transparent)
					];
		}
		else
		{
			return 
				SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.MinValue(this, &SMetaHumanCharacterEditorToolView::GetFloatPropertyMinValue, InProperty)
					.MaxValue(this, &SMetaHumanCharacterEditorToolView::GetFloatPropertyMaxValue, InProperty)
					.MinSliderValue(this, &SMetaHumanCharacterEditorToolView::GetFloatPropertyMinValue, InProperty)
					.MaxSliderValue(this, &SMetaHumanCharacterEditorToolView::GetFloatPropertyMaxValue, InProperty)
					.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
					.Value(this, &SMetaHumanCharacterEditorToolView::GetFloatPropertyValue, InProperty, PropertyContainerPtr)
					.OnBeginSliderMovement(this, &SMetaHumanCharacterEditorToolView::OnPreEditChangeProperty, InProperty, InLabelOverride)
					.OnEndSliderMovement(this, &SMetaHumanCharacterEditorToolView::OnFloatPropertyValueChanged, /* bIsInteractive */ false, InProperty, PropertyContainerPtr)
					.OnValueChanged(this, &SMetaHumanCharacterEditorToolView::OnFloatPropertyValueChanged, /* bIsDragging */ true, InProperty, PropertyContainerPtr)
					.OnValueCommitted(this, &SMetaHumanCharacterEditorToolView::OnFloatPropertyValueCommited, InProperty, PropertyContainerPtr)
					.Visibility(this, &SMetaHumanCharacterEditorToolView::GetPropertyVisibility, InProperty, PropertyContainerPtr)
					.IsEnabled(this, &SMetaHumanCharacterEditorToolView::IsPropertyEnabled, InProperty, PropertyContainerPtr)
					.PreventThrottling(true)
					.MaxFractionalDigits(InFractionalDigits)
					.LinearDeltaSensitivity(1.0)
					.LabelPadding(FMargin(3))
					.LabelLocation(SNumericEntryBox<float>::ELabelLocation::Inside)
					.Label()
					[
						SNumericEntryBox<float>::BuildNarrowColorLabel(FLinearColor::Transparent)
					];
		}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorToolView::CreatePropertyNumericEntryNormalized(FProperty* InProperty, void* InPropertyContainerPtr, float InMinValue, float InMaxValue, const FText& InLabelOverride)
{
	if (!InProperty || !InPropertyContainerPtr || !CastField<FNumericProperty>(InProperty))
	{
		return SNullWidget::NullWidget;
	}

	check(InMinValue < InMaxValue);

	return
		SNew(SNumericEntryBox<float>)
		.AllowSpin(true)
		.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.MinValue(0.0f)
		.MaxValue(1.0f)
		.MinSliderValue(0.0f)
		.MaxSliderValue(1.0f)
		.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
		.Value(this, &SMetaHumanCharacterEditorToolView::GetFloatPropertyValueNormalized, InProperty, InPropertyContainerPtr, InMinValue, InMaxValue)
		.OnBeginSliderMovement(this, &SMetaHumanCharacterEditorToolView::OnPreEditChangeProperty, InProperty, InLabelOverride.ToString())
		.OnEndSliderMovement(this, &SMetaHumanCharacterEditorToolView::OnFloatPropertyNormalizedValueChanged, InMinValue, InMaxValue, /* bIsInteractive */ false, InProperty, InPropertyContainerPtr)
		.OnValueChanged(this, &SMetaHumanCharacterEditorToolView::OnFloatPropertyNormalizedValueChanged, InMinValue, InMaxValue, /* bIsDragging */ true, InProperty, InPropertyContainerPtr)
		.OnValueCommitted(this, &SMetaHumanCharacterEditorToolView::OnFloatPropertyNormalizedValueCommited, InMinValue, InMaxValue, InProperty, InPropertyContainerPtr)
		.PreventThrottling(true)
		.MaxFractionalDigits(2)
		.LinearDeltaSensitivity(1.0)
		.LabelPadding(FMargin(3))
		.LabelLocation(SNumericEntryBox<float>::ELabelLocation::Inside)
		.Label()
		[
			SNumericEntryBox<float>::BuildNarrowColorLabel(FLinearColor::Transparent)
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorToolView::CreatePropertySpinBoxWidget(const FString Label, FProperty* Property, void* PropertyContainerPtr, const int32 FractionalDigits)
{
	if (!Property || !PropertyContainerPtr)
	{
		return SNullWidget::NullWidget;
	}

	return
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// SpinBox Label section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(.3f)
			.Padding(10.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]

			// SpinBox slider section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(.7f)
			.Padding(4.f, 2.f, 40.f, 2.f)
			[
				CreatePropertyNumericEntry(Property, PropertyContainerPtr, Label, FractionalDigits)
			]
		]
		
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Thickness(1.f)
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorToolView::CreatePropertySpinBoxWidgetNormalized(FProperty* InProperty, void* InPropertyContainerPtr, float InMinValue, float InMaxValue, const FText& InLabelOverride /*= FText::GetEmpty()*/)
{
	if (!InProperty || !InPropertyContainerPtr)
	{
		return SNullWidget::NullWidget;
	}

	check(InMinValue < InMaxValue);

	return
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// SpinBox Label section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(.3f)
			.Padding(10.f, 0.f)
			[
				SNew(STextBlock)
				.Text(InLabelOverride.IsEmptyOrWhitespace() ? InProperty->GetDisplayNameText() : InLabelOverride)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]

			// SpinBox slider section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(.7f)
			.Padding(4.f, 2.f, 40.f, 2.f)
			[
				CreatePropertyNumericEntryNormalized(InProperty, InPropertyContainerPtr, InMinValue, InMaxValue, InLabelOverride)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Thickness(1.f)
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorToolView::CreatePropertyCheckBoxWidget(const FString Label, FProperty* Property, void* PropertyContainerPtr)
{
	if (!Property || !PropertyContainerPtr)
	{
		return SNullWidget::NullWidget;
	}

	return
		SNew(SVerticalBox)
		.ToolTipText_Lambda([Property]
		{
			return Property->GetToolTipText();
		})

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(.35f)
			.Padding(10.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(.65f)
			.Padding(2.f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SMetaHumanCharacterEditorToolView::IsPropertyCheckBoxChecked, Property, PropertyContainerPtr)
				.OnCheckStateChanged(this, &SMetaHumanCharacterEditorToolView::OnPropertyCheckStateChanged, Property, PropertyContainerPtr)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Thickness(1.f)
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorToolView::CreatePropertyColorPickerWidget(const FString Label, FProperty * Property, void* PropertyContainerPtr)
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

			// Color Picker Label section
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

			// Color Picker block section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(.7f)
			.Padding(4.f, 2.f, 40.f, 2.f)
			[
				SNew(SColorBlock)
				.Color(this, &SMetaHumanCharacterEditorToolView::GetColorPropertyValue, Property, PropertyContainerPtr)
				.OnMouseButtonDown(this, &SMetaHumanCharacterEditorToolView::OnColorBlockMouseButtonDown, Property, PropertyContainerPtr, Label)
				.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
				.AlphaBackgroundBrush(FAppStyle::Get().GetBrush("ColorPicker.RoundedAlphaBackground"))
				.ShowBackgroundForAlpha(true)
				.CornerRadius(FVector4(2.f, 2.f, 2.f, 2.f))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Thickness(1.f)
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorToolView::CreatePropertyUVColorPickerWidget(FProperty* InPropertyU,
																						 FProperty* InPropertyV,
																						 void* InPropertyContainerPtr,
																						 const FText& InColorPickerLabel,
																						 TNotNull<UTexture2D*> InColorPickerTexture,
																						 const FText& InULabelOverride /*= FText::GetEmpty()*/,
																						 const FText& InVLabelOverride /*= FText::GetEmpty()*/)
{
	if (!InPropertyU || !InPropertyV || !InPropertyContainerPtr)
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// Color Picker Label
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(.3f)
			.Padding(10.f, 0.f)
			[
				SNew(STextBlock)
				.Text(InColorPickerLabel)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(0.7f)
			.Padding(4.f, 2.f, 40.f, 2.f)
			[
				SNew(SUVColorPicker)
				.UV_Lambda([this, InPropertyU, InPropertyV, InPropertyContainerPtr]
				{
					return FVector2f
					(
						GetFloatPropertyValue(InPropertyU, InPropertyContainerPtr).GetValue(),
						GetFloatPropertyValue(InPropertyV, InPropertyContainerPtr).GetValue()
					);
				})
				.OnUVChanged_Lambda([this, InPropertyU, InPropertyV, InPropertyContainerPtr](const FVector2f& InUV, bool bIsDragging)
				{
					OnPreEditChangeProperty(InPropertyU);
					OnPreEditChangeProperty(InPropertyV);
					OnFloatPropertyValueChanged(InUV.X, bIsDragging, InPropertyU, InPropertyContainerPtr);
					OnFloatPropertyValueChanged(InUV.Y, bIsDragging, InPropertyV, InPropertyContainerPtr);
				})
				.ColorPickerLabel(InColorPickerLabel)
				.ULabelOverride(InULabelOverride)
				.VLabelOverride(InVLabelOverride)
				.ColorPickerTexture(InColorPickerTexture)
			]		
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
				.Thickness(1.f)
		];
}

ECheckBoxState SMetaHumanCharacterEditorToolView::IsPropertyCheckBoxChecked(FProperty* Property, void* PropertyContainerPtr) const
{
	if (Property && PropertyContainerPtr)
	{
		const bool bIsChecked = GetBoolPropertyValue(Property, PropertyContainerPtr);
		return bIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void SMetaHumanCharacterEditorToolView::OnPropertyCheckStateChanged(ECheckBoxState CheckState, FProperty* Property, void* PropertyContainerPtr)
{
	const bool bIsChecked = CheckState == ECheckBoxState::Checked;
	if (Property && PropertyContainerPtr)
	{
		OnBoolPropertyValueChanged(bIsChecked, Property, PropertyContainerPtr);
	}
}

FReply SMetaHumanCharacterEditorToolView::OnColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FProperty* Property, void* PropertyContainerPtr, const FString Label)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	FColorPickerArgs Args;
	Args.bIsModal = false;
	Args.bOnlyRefreshOnMouseUp = false;
	Args.bOnlyRefreshOnOk = false;
	Args.bUseAlpha = true;
	Args.bOpenAsMenu = true;
	Args.InitialColor = GetColorPropertyValue(Property, PropertyContainerPtr);
	Args.OnInteractivePickBegin = FSimpleDelegate::CreateSP(this, &SMetaHumanCharacterEditorToolView::OnPreEditChangeProperty, Property, Label);
	Args.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SMetaHumanCharacterEditorToolView::OnColorPropertyValueChanged, /* bIsInteractive */ true, Property, PropertyContainerPtr);
	Args.OnInteractivePickEnd = FSimpleDelegate::CreateSP(this, &SMetaHumanCharacterEditorToolView::OnPostEditChangeProperty, Property, /* bIsInteractive */ false);
	Args.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &SMetaHumanCharacterEditorToolView::OnColorPropertyValueChanged, /* bIsInteractive */ false, Property, PropertyContainerPtr);

	OpenColorPicker(Args);

	return FReply::Handled();
}

TOptional<int32> SMetaHumanCharacterEditorToolView::GetIntPropertyValue(FProperty* Property, void* PropertyContainerPtr) const
{
	TOptional<int32> PropertyValue;
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (NumericProperty && PropertyContainerPtr)
	{
		const int32* PropertyValuePtr = NumericProperty->ContainerPtrToValuePtr<int32>(PropertyContainerPtr);
		PropertyValue = NumericProperty->GetSignedIntPropertyValue(PropertyValuePtr);
	}

	return PropertyValue;
}

TOptional<int32> SMetaHumanCharacterEditorToolView::GetIntPropertyMinValue(FProperty* Property) const
{
	TOptional<int32> PropertyMinValue;
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (NumericProperty)
	{
		const FString& MinValueAsString = Property->GetMetaData(TEXT("ClampMin"));
		PropertyMinValue = FCString::Atoi(*MinValueAsString);
	}

	return PropertyMinValue;
}

TOptional<int32> SMetaHumanCharacterEditorToolView::GetIntPropertyMaxValue(FProperty* Property) const
{
	TOptional<int32> PropertyMaxValue;
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (NumericProperty)
	{
		const FString& MaxValueAsString = Property->GetMetaData(TEXT("ClampMax"));
		PropertyMaxValue = FCString::Atoi(*MaxValueAsString);
	}

	return PropertyMaxValue;
}

TOptional<float> SMetaHumanCharacterEditorToolView::GetFloatPropertyValue(FProperty* Property, void* PropertyContainerPtr) const
{
	TOptional<float> PropertyValue;
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (NumericProperty && PropertyContainerPtr)
	{
		const float* PropertyValuePtr = NumericProperty->ContainerPtrToValuePtr<float>(PropertyContainerPtr);
		PropertyValue = NumericProperty->GetFloatingPointPropertyValue(PropertyValuePtr);
	}

	return PropertyValue;
}

TOptional<float> SMetaHumanCharacterEditorToolView::GetFloatPropertyValueNormalized(FProperty* InProperty, void* InPropertyContainerPtr, float InMinValue, float InMaxValue) const
{
	TOptional<float> PropertyValue = GetFloatPropertyValue(InProperty, InPropertyContainerPtr);
	if (PropertyValue.IsSet())
	{
		return (PropertyValue.GetValue() - InMinValue) / (InMaxValue - InMinValue);
	}
	return PropertyValue;
}

TOptional<float> SMetaHumanCharacterEditorToolView::GetFloatPropertyMinValue(FProperty* Property) const
{
	TOptional<float> PropertyMinValue;
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (NumericProperty)
	{
		const FString& MinValueAsString = Property->GetMetaData(TEXT("ClampMin"));
		PropertyMinValue = FCString::Atof(*MinValueAsString);
	}

	return PropertyMinValue;
}

TOptional<float> SMetaHumanCharacterEditorToolView::GetFloatPropertyMaxValue(FProperty* Property) const
{
	TOptional<float> PropertyMaxValue;
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (NumericProperty)
	{
		const FString& MaxValueAsString = Property->GetMetaData(TEXT("ClampMax"));
		PropertyMaxValue = FCString::Atof(*MaxValueAsString);
	}

	return PropertyMaxValue;
}

bool SMetaHumanCharacterEditorToolView::GetBoolPropertyValue(FProperty* Property, void* PropertyContainerPtr) const
{
	bool PropertyValue = false;
	const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property);
	if (BoolProperty && PropertyContainerPtr)
	{
		PropertyValue = BoolProperty->GetPropertyValue_InContainer(PropertyContainerPtr);
	}

	return PropertyValue;
}

uint64 SMetaHumanCharacterEditorToolView::GetEnumPropertyValue(FProperty* Property, void* PropertyContainerPtr) const
{
	uint64 PropertyValue = 0;

	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		FNumericProperty* UnderlyingProp = EnumProperty->GetUnderlyingProperty();
		void* ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(PropertyContainerPtr);
		PropertyValue = UnderlyingProp->GetUnsignedIntPropertyValue(ValuePtr);
	}
	else if (FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		void* ValuePtr = ByteProperty->ContainerPtrToValuePtr<void>(PropertyContainerPtr);
		PropertyValue = ByteProperty->GetPropertyValue(ValuePtr);
	}

	return PropertyValue;
}

FLinearColor SMetaHumanCharacterEditorToolView::GetColorPropertyValue(FProperty* Property, void* PropertyContainerPtr) const
{
	FLinearColor PropertyValue = FLinearColor::White;
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	if (StructProperty && StructProperty->Struct->GetFName() == NAME_LinearColor && PropertyContainerPtr)
	{
		const FLinearColor* ColorPtr = StructProperty->ContainerPtrToValuePtr<FLinearColor>(PropertyContainerPtr);
		PropertyValue = ColorPtr ? *ColorPtr : FLinearColor::White;
	}

	return PropertyValue;
}

void SMetaHumanCharacterEditorToolView::OnPreEditChangeProperty(FProperty* Property, const FString Label)
{
	UInteractiveToolPropertySet* ToolProperties = GetToolProperties();
	if (!ToolProperties || !Property)
	{
		return;
	}

	if (!PropertyChangeTransaction.IsValid())
	{
		const FString PropertyName = Label.IsEmpty() ? Property->GetName() : Label;
		const FText TransactionText = FText::Format(LOCTEXT("ToolPropertyChangeTransaction", "Edit {0}"), FText::FromString(PropertyName));
		PropertyChangeTransaction = MakeUnique<FScopedTransaction>(TransactionText);
	}
	
	ToolProperties->PreEditChange(Property);
}

void SMetaHumanCharacterEditorToolView::OnPostEditChangeProperty(FProperty* Property, bool bIsInteractive)
{
	UInteractiveToolPropertySet* ToolProperties = GetToolProperties();
	if (!ToolProperties || !Property)
	{
		return;
	}

	const EPropertyChangeType::Type PropertyFlags = bIsInteractive ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet;
	FPropertyChangedEvent PropertyChangedEvent(Property, PropertyFlags);
	ToolProperties->PostEditChangeProperty(PropertyChangedEvent);

	if (!bIsInteractive && PropertyChangeTransaction.IsValid() && PropertyChangeTransaction->IsOutstanding())
	{
		PropertyChangeTransaction.Reset();
	}
}

void SMetaHumanCharacterEditorToolView::OnIntPropertyValueChanged(const int32 Value, bool bIsInteractive, FProperty* Property, void* PropertyContainerPtr)
{
	UInteractiveToolPropertySet* ToolProperties = GetToolProperties();
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (ToolProperties && NumericProperty && PropertyContainerPtr)
	{
		const TOptional<int32> MinValue = GetIntPropertyMinValue(Property);
		const TOptional<int32> MaxValue = GetIntPropertyMaxValue(Property);
		const int32 ClampedValue = FMath::Clamp(Value, MinValue.GetValue(), MaxValue.GetValue());

		int32* PropertyValuePtr = NumericProperty->ContainerPtrToValuePtr<int32>(PropertyContainerPtr);
		NumericProperty->SetIntPropertyValue(PropertyValuePtr, static_cast<int64>(ClampedValue));

		OnPostEditChangeProperty(Property, bIsInteractive);
	}
}

void SMetaHumanCharacterEditorToolView::OnFloatPropertyValueChanged(const float Value, bool bIsInteractive, FProperty* Property, void* PropertyContainerPtr)
{
	UInteractiveToolPropertySet* ToolProperties = GetToolProperties();
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (ToolProperties && NumericProperty && PropertyContainerPtr)
	{
		const TOptional<float> MinValue = GetFloatPropertyMinValue(Property);
		const TOptional<float> MaxValue = GetFloatPropertyMaxValue(Property);
		float ClampedValue = Value;
		if (MinValue.IsSet() && MaxValue.IsSet())
		{
			ClampedValue = FMath::Clamp(Value, MinValue.GetValue(), MaxValue.GetValue());
		}
		else if (MinValue.IsSet())
		{
			ClampedValue = FMath::Max(Value, MinValue.GetValue());
		}
		else if (MaxValue.IsSet())
		{
			ClampedValue = FMath::Min(Value, MaxValue.GetValue());
		}

		float* PropertyValuePtr = NumericProperty->ContainerPtrToValuePtr<float>(PropertyContainerPtr);
		NumericProperty->SetFloatingPointPropertyValue(PropertyValuePtr, ClampedValue);

		OnPostEditChangeProperty(Property, bIsInteractive);
	}
}

void SMetaHumanCharacterEditorToolView::OnIntPropertyValueCommitted(int32 Value, ETextCommit::Type Type, FProperty* Property, void* PropertyContainerPtr)
{
	const bool bIsInteractive = false;
	OnIntPropertyValueChanged(Value, bIsInteractive, Property, PropertyContainerPtr);
}

void SMetaHumanCharacterEditorToolView::OnFloatPropertyNormalizedValueChanged(float Value, float InMinValue, float InMaxValue, bool bIsInteractive, FProperty* InProperty, void* InPropertyContainerPtr)
{
	UInteractiveToolPropertySet* ToolProperties = GetToolProperties();
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(InProperty);
	if (ToolProperties && NumericProperty && InPropertyContainerPtr)
	{
		const float ActualValue = FMath::Lerp(InMinValue, InMaxValue, Value);

		float* PropertyValuePtr = NumericProperty->ContainerPtrToValuePtr<float>(InPropertyContainerPtr);
		NumericProperty->SetFloatingPointPropertyValue(PropertyValuePtr, ActualValue);

		OnPostEditChangeProperty(InProperty, bIsInteractive);
	}
}

void SMetaHumanCharacterEditorToolView::OnFloatPropertyValueCommited(const float Value, ETextCommit::Type Type, FProperty* Property, void* PropertyContainerPtr)
{
	OnFloatPropertyValueChanged(Value, /*bIsInteractive*/false, Property, PropertyContainerPtr);
}

void SMetaHumanCharacterEditorToolView::OnFloatPropertyNormalizedValueCommited(float InValue, ETextCommit::Type InType, float InMinValue, float InMaxValue, FProperty* InProperty, void* InPropertyContainerPtr)
{
	const bool bIsInteractive = false;
	OnFloatPropertyNormalizedValueChanged(InValue, InMinValue, InMaxValue, bIsInteractive, InProperty, InPropertyContainerPtr);
}

void SMetaHumanCharacterEditorToolView::OnBoolPropertyValueChanged(const bool Value, FProperty* Property, void* PropertyContainerPtr)
{
	UInteractiveToolPropertySet* ToolProperties = GetToolProperties();
	const FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property);
	if (ToolProperties && BoolProperty && PropertyContainerPtr)
	{
		FString PropertyName = Property->GetName();
		PropertyName.RemoveFromStart(TEXT("b"));
		const FText TransactionText = FText::Format(LOCTEXT("ToolBoolPropertyChangeTransaction", "Edit {0}"), FText::FromString(PropertyName));
		const FScopedTransaction OnEnumPropertyChangedTransaction = FScopedTransaction(TransactionText);
		ToolProperties->PreEditChange(Property);

		BoolProperty->SetPropertyValue_InContainer(PropertyContainerPtr, Value);

		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
		ToolProperties->PostEditChangeProperty(PropertyChangedEvent);
	}
}

void SMetaHumanCharacterEditorToolView::OnEnumPropertyValueChanged(uint8 Value, FProperty* Property, void* PropertyContainerPtr)
{
	UInteractiveToolPropertySet* ToolProperties = GetToolProperties();
	const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property);
	if (ToolProperties && EnumProperty && PropertyContainerPtr)
	{
		const FText TransactionText = FText::Format(LOCTEXT("ToolEnumPropertyChangeTransaction", "Edit {0}"), FText::FromString(Property->GetName()));
		const FScopedTransaction OnEnumPropertyChangedTransaction = FScopedTransaction(TransactionText);
		ToolProperties->PreEditChange(Property);

		int64* PropertyValuePtr = EnumProperty->ContainerPtrToValuePtr<int64>(PropertyContainerPtr);
		EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(PropertyValuePtr, static_cast<int64>(Value));

		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
		ToolProperties->PostEditChangeProperty(PropertyChangedEvent);
	}
}

void SMetaHumanCharacterEditorToolView::OnColorPropertyValueChanged(FLinearColor Color, bool bIsInteractive, FProperty* Property, void* PropertyContainerPtr)
{
	UInteractiveToolPropertySet* ToolProperties = GetToolProperties();
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	if (!ToolProperties || !StructProperty || !PropertyContainerPtr)
	{
		return;
	}

	if (StructProperty->Struct->GetFName() != NAME_LinearColor)
	{
		return;
	}

	FLinearColor* ColorPtr = StructProperty->ContainerPtrToValuePtr<FLinearColor>(PropertyContainerPtr);
	if (ColorPtr)
	{
		*ColorPtr = Color;

		OnPostEditChangeProperty(Property, bIsInteractive);
	}
}

EVisibility SMetaHumanCharacterEditorToolView::GetPropertyVisibility(FProperty* Property, void* PropertyContainerPtr) const
{
	return EVisibility::Visible;
}

bool SMetaHumanCharacterEditorToolView::IsPropertyEnabled(FProperty* Property, void* PropertyContainerPtr) const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
