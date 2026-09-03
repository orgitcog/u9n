// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorTeethSlidersPanel.h"

#include "MetaHumanCharacterEditorStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorTeethSlidersPanel"

void SMetaHumanCharacterEditorTeethSlider::Construct(const FArguments& InArgs)
{
	MinValue = InArgs._MinValue;
	MaxValue = InArgs._MaxValue;

	Orientation = InArgs._Orientation;
	OnValueChangedDelegate = InArgs._OnValueChanged;
	OnMouseCaptureBeginDelegate = InArgs._OnMouseCaptureBegin;

	const FSlateRenderTransform RenderTransform = 
		Orientation == EOrientation::Orient_Horizontal ?
		FSlateRenderTransform() :
		FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-90.f)));

	ChildSlot
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(20.f, 20.f))
				.Image(this, &SMetaHumanCharacterEditorTeethSlider::GetElipseBrush)
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				.RenderTransform(RenderTransform)
				.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("Teeth.Arrow")))
				.Visibility(this, &SMetaHumanCharacterEditorTeethSlider::GetArrowVisibility)
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(90.f)
				.HeightOverride(20.f)
				[
					SAssignNew(Slider, SSlider)
					.ToolTipText(InArgs._ToolTipText)
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					.RenderTransform(RenderTransform)
					.MouseUsesStep(true)
					.StepSize(.001)
					.IndentHandle(false)
					.Style(FMetaHumanCharacterEditorStyle::Get(), TEXT("MetaHumanCharacterEditorTools.Teeth.Slider"))
					.MinValue(MinValue)
					.MaxValue(MaxValue)
					.PreventThrottling(true)
					.Value(InArgs._Value)
					.OnValueChanged(this, &SMetaHumanCharacterEditorTeethSlider::OnValueChanged)
					.SliderBarColor(FLinearColor::Transparent)
					.SliderHandleColor(FLinearColor::Transparent)
					.OnMouseCaptureBegin(this, &SMetaHumanCharacterEditorTeethSlider::OnMouseCaptureBegin)
					.OnMouseCaptureEnd(this, &SMetaHumanCharacterEditorTeethSlider::OnMouseCaptureEnd)
					.Orientation(Orient_Horizontal)
				]		
			]
		];
}

void SMetaHumanCharacterEditorTeethSlider::OnValueChanged(float NewValue)
{
	if (Slider.IsValid())
	{
		constexpr bool bIsInteractive = true;
		OnValueChangedDelegate.ExecuteIfBound(NewValue, bIsInteractive);
	}
}

void SMetaHumanCharacterEditorTeethSlider::OnMouseCaptureBegin()
{
	bIsDragging = true;

	if (Slider.IsValid())
	{
		Slider->SetSliderBarColor(FLinearColor::White);
		Slider->SetSliderHandleColor(FLinearColor::White);
		Slider->SetCursor(EMouseCursor::None);
	}

	OnMouseCaptureBeginDelegate.ExecuteIfBound();
}

void SMetaHumanCharacterEditorTeethSlider::OnMouseCaptureEnd()
{
	bIsDragging = false;

	if (Slider.IsValid())
	{
		Slider->SetSliderBarColor(FLinearColor::Transparent);
		Slider->SetSliderHandleColor(FLinearColor::Transparent);
		Slider->SetCursor(EMouseCursor::Default);

		constexpr bool bIsInteractive = false;
		OnValueChangedDelegate.ExecuteIfBound(Slider->GetValue(), bIsInteractive);
	}
}

const FSlateBrush* SMetaHumanCharacterEditorTeethSlider::GetElipseBrush() const
{
	return
		bIsDragging ?
		FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("Teeth.FullElipse")) :
		FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("Teeth.EmptyElipse"));
}

EVisibility SMetaHumanCharacterEditorTeethSlider::GetArrowVisibility() const
{
	return bIsDragging ? EVisibility::Hidden : EVisibility::Visible;
}

void SMetaHumanCharacterEditorTeethSlidersPanel::Construct(const FArguments& InArgs)
{
	OnGetTeethSliderPropertyValueDelegate = InArgs._OnGetTeethSliderValue;
	OnTeethSliderPropertyEditedDelegate = InArgs._OnTeethSliderPropertyEdited;
	OnTeethSliderValueChangedDelegate = InArgs._OnTeethSliderValueChanged;

	ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoHeight()
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SConstraintCanvas)
					+ SConstraintCanvas::Slot()
					.Anchors(FAnchors(0.5f, 0.5f))
					.Offset(FMargin(0.f, 0.f))
					.AutoSize(true)
					[
						SNew(SImage)
						.DesiredSizeOverride(FVector2D(320.f, 260.f))
						.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("Teeth.Preview")))
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(TeethSlidersCanvas, SConstraintCanvas)
				]
			]
		];

	MakeTeethSlidersCanvas();
}

void SMetaHumanCharacterEditorTeethSlidersPanel::MakeTeethSlidersCanvas()
{
	if (!TeethSlidersCanvas.IsValid())
	{
		return;
	}

	// Narrowness
	FProperty* NarrownessProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, Narrowness));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(-140.f, -30.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(NarrownessProperty, Orient_Horizontal)
		];

	// Receding Gums
	FProperty* RecedingGumsProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, RecedingGums));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(-75.f, -30.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(RecedingGumsProperty, Orient_Vertical)
		];

	// Polycanine
	FProperty* PolycanineProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, Polycanine));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(-25.f, -30.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(PolycanineProperty, Orient_Vertical)
		];

	// Tooth Length
	FProperty* ToothLengthProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, ToothLength));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(70.f, -15.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(ToothLengthProperty, Orient_Vertical)
		];
	
	// Upper Shift
	FProperty* UpperShiftProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, UpperShift));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(70.f, -75.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(UpperShiftProperty, Orient_Horizontal)
		];
	
	// Overbite
	FProperty* OverbiteProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, Overbite));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(120.f, -40.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(OverbiteProperty, Orient_Horizontal)
		];

	// Worn Down
	FProperty* WornDownProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, WornDown));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(-100.f, 40.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(WornDownProperty, Orient_Vertical)
		];

	// Tooth Spacing
	FProperty* ToothSpacingProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, ToothSpacing));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(-25.f, 70.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(ToothSpacingProperty, Orient_Horizontal)
		];

	// Lower Shift
	FProperty* LowerShiftProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, LowerShift));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(40.f, 80.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(LowerShiftProperty, Orient_Horizontal)
		];
	
	// Overjet
	FProperty* OverjetProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, Overjet));
	TeethSlidersCanvas->AddSlot()
		.Anchors(FAnchors(0.5f))
		.Offset(FMargin(130.f, 20.f))
		.AutoSize(true)
		[
			CreateTeethPropertySlider(OverjetProperty, Orient_Horizontal)
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorTeethSlidersPanel::CreateTeethPropertySlider(FProperty* Property, EOrientation Orientation)
{
	if (!Property)
	{
		return SNullWidget::NullWidget;
	}

	float MinValue = 0.f;
	float MaxValue = 1.f;
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
	if (NumericProperty)
	{
		const FString& MinValueAsString = Property->GetMetaData(TEXT("ClampMin"));
		const FString& MaxValueAsString = Property->GetMetaData(TEXT("ClampMax"));
		MinValue = FCString::Atoi(*MinValueAsString);
		MaxValue = FCString::Atoi(*MaxValueAsString);
	}

	const TSharedRef<SWidget> TeethPropertySlider =
		SNew(SMetaHumanCharacterEditorTeethSlider)
		.ToolTipText(FText::FromString(Property->GetName()))
		.Orientation(Orientation)
		.MinValue(MinValue)
		.MaxValue(MaxValue)
		.Value(this, &SMetaHumanCharacterEditorTeethSlidersPanel::GetTeethSliderValue, Property)
		.OnValueChanged(this, &SMetaHumanCharacterEditorTeethSlidersPanel::OnTeethSliderValueChanged, Property)
		.OnMouseCaptureBegin(this, &SMetaHumanCharacterEditorTeethSlidersPanel::OnTeethSliderMouseCaptureBegin, Property);

	return TeethPropertySlider;
}

float SMetaHumanCharacterEditorTeethSlidersPanel::GetTeethSliderValue(FProperty* Property) const
{
	if (Property && OnGetTeethSliderPropertyValueDelegate.IsBound())
	{
		return OnGetTeethSliderPropertyValueDelegate.Execute(Property).GetValue();
	}

	return 0.f;
}

void SMetaHumanCharacterEditorTeethSlidersPanel::OnTeethSliderValueChanged(const float Value, bool bIsInteractive, FProperty* Property)
{
	OnTeethSliderValueChangedDelegate.ExecuteIfBound(Value, bIsInteractive, Property);
}

void SMetaHumanCharacterEditorTeethSlidersPanel::OnTeethSliderMouseCaptureBegin(FProperty* Property)
{
	OnTeethSliderPropertyEditedDelegate.ExecuteIfBound(Property);
}

#undef LOCTEXT_NAMESPACE
