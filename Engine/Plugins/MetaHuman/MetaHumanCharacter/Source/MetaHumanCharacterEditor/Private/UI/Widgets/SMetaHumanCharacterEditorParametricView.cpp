// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorParametricView.h"

#include "MetaHumanCharacterEditorStyle.h"
#include "Modules/ModuleManager.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorParametricView"

template<typename NumericType>
struct TParametricTypeInterface : TDefaultNumericTypeInterface<NumericType>
{
	TParametricTypeInterface(NumericType InMinValue, NumericType InMaxValue, float InSliderDistance)
		: MinValue(InMinValue), MaxValue(InMaxValue), SliderDistance(InSliderDistance)
	{
	}
	
	/** Convert the type to/from a string */
	virtual FString ToString(const NumericType& SliderValue) const override
	{
		float Fraction = SliderValue / SliderDistance;
		NumericType OutValue = Fraction * (MaxValue - MinValue) + MinValue;
		return TDefaultNumericTypeInterface<NumericType>::ToString(OutValue);
	}

	virtual TOptional<NumericType> FromString(const FString& InString, const NumericType& InExistingValue) override
	{
		TOptional<NumericType> Result = TDefaultNumericTypeInterface<NumericType>::FromString(InString, InExistingValue);
		if (Result.IsSet())
		{
			float Fraction = FMath::GetRangePct(FVector2f(MinValue, MaxValue), Result.GetValue());
			Result = static_cast<NumericType>(Fraction * SliderDistance);
		}
		return Result;
	}

	NumericType MinValue;
	NumericType MaxValue;
	float SliderDistance = 100.f;
};

template<typename NumericType>
void SMetaHumanCharacterEditorParametricSpinBox<NumericType>::Construct(const FArguments& InArgs)
{
	ValueAttribute = InArgs._Value;
	OnValueChanged = InArgs._OnValueChanged;
	OnGetDisplayValue = InArgs._OnGetDisplayValue;

	MinValue = InArgs._MinValue.Get().GetValue();
	MaxValue = InArgs._MaxValue.Get().GetValue();

	InterfaceAttr = MakeShared<TParametricTypeInterface<NumericType>>(MinValue, MaxValue, SliderDistance);

	ChildSlot
	[
		SAssignNew(SpinBox, SSpinBox<float>)
		.Style(InArgs._SpinBoxStyle)
		.Font(InArgs._Font)
		.Value(this, &SMetaHumanCharacterEditorParametricSpinBox::GetSliderValue)
		.OnValueChanged(this, &SMetaHumanCharacterEditorParametricSpinBox::OnSliderValueChanged, false)
		.MaxFractionalDigits(2)
		.MaxValue(SliderMaxValue)
		.MinValue(SliderMinValue)
		.OnBeginSliderMovement(InArgs._OnBeginSliderMovement)
		.OnEndSliderMovement(this, &SMetaHumanCharacterEditorParametricSpinBox::OnSliderValueChanged, true)
		.OnGetDisplayValue(this, &SMetaHumanCharacterEditorParametricSpinBox::GetDisplayText)
		.ToolTipText(InArgs._ToolTip)
		.TypeInterface(InterfaceAttr)
		.IsEnabled(InArgs._IsEnabled)
	];
}

template<typename NumericType>
float SMetaHumanCharacterEditorParametricSpinBox<NumericType>::GetSliderValue() const
{
	float Fraction = FMath::GetRangePct(FVector2f(MinValue, MaxValue), ValueAttribute.Get());
	return Fraction * SliderDistance;
}

template<typename NumericType>
void SMetaHumanCharacterEditorParametricSpinBox<NumericType>::OnSliderValueChanged(float NewValue, bool bCommit) const
{
	float PrevValue = SpinBox->GetValue();
	if (FMath::Abs(NewValue- PrevValue) > UE_KINDA_SMALL_NUMBER || bCommit)
	{
		float Fraction = NewValue / SliderDistance;
		NumericType OutValue = Fraction * (MaxValue - MinValue) + MinValue;
		OnValueChanged.ExecuteIfBound(OutValue, bCommit);
	}	
}

template<typename NumericType>
FText SMetaHumanCharacterEditorParametricSpinBox<NumericType>::GetOutputValueText() const
{
	return FText::AsNumber(ValueAttribute.Get());
}

template<typename NumericType>
TOptional<FText> SMetaHumanCharacterEditorParametricSpinBox<NumericType>::GetDisplayText(float Value) const
{
	float Fraction = Value / SliderDistance;
	NumericType OutValue = Fraction * (MaxValue - MinValue) + MinValue;

	if (OnGetDisplayValue.IsBound())
	{
		return OnGetDisplayValue.Execute(OutValue);
	}

	return FText::AsNumber(OutValue);
}

void SMetaHumanCharacterEditorParametricConstraintView::Construct(const FArguments& InArgs)
{
	ConstraintName = InArgs._ConstraintName;
	TargetMeasurement = InArgs._TargetMeasurement;
	ActualMeasurement = InArgs._ActualMeasurement;
	IsPinned = InArgs._IsPinned;
	OnBeginConstraintEditingDelegate = InArgs._OnBeginConstraintEditing;
	OnParametricConstraintChangedDelegate = InArgs._OnParametricConstraintChanged;

	ChildSlot
		[
			SNew(SHorizontalBox)
			.ToolTipText(InArgs._ToolTip)
			
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(.2f)
			.Padding(10.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromName(ConstraintName))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
			
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(.8f)
			.Padding(3.f, 0.f)
			[
				SNew(SMetaHumanCharacterEditorParametricSpinBox<float>)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.MinValue(InArgs._MinValue)
					.MaxValue(InArgs._MaxValue)
					.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
					.Value(this, &SMetaHumanCharacterEditorParametricConstraintView::GetParametricValue)
					.OnBeginSliderMovement(this, &SMetaHumanCharacterEditorParametricConstraintView::OnBeginConstraintEditing)
					.OnValueChanged(this, &SMetaHumanCharacterEditorParametricConstraintView::OnConstraintTargetChanged)
					.OnEndSliderMovement(this, &SMetaHumanCharacterEditorParametricConstraintView::OnConstraintTargetChanged)
					.OnGetDisplayValue(this, &SMetaHumanCharacterEditorParametricConstraintView::GetDisplayText)
					.IsEnabled(InArgs._IsEnabled)
			]
		
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(2.f, 2.f)
			[
				SNew(SCheckBox)
				.Style(FMetaHumanCharacterEditorStyle::Get(), TEXT("MetaHumanCharacterEditorTools.ParametricBody.CheckBox"))
				.Visibility(InArgs._PinVisibility)
				.IsChecked(this, &SMetaHumanCharacterEditorParametricConstraintView::GetConstraintChecked)
				.OnCheckStateChanged(this, &SMetaHumanCharacterEditorParametricConstraintView::OnConstraintPinnedChanged)
			]
		];
}

TOptional<FText> SMetaHumanCharacterEditorParametricConstraintView::GetDisplayText(float TargetValue) const
{
	FText DisplayText;

	FNumberFormattingOptions FormatOptions;
	FormatOptions.MinimumIntegralDigits = 1;
	FormatOptions.MaximumFractionalDigits = 2;
	FormatOptions.MinimumFractionalDigits = 2;

	if (IsPinned.Get() && ActualMeasurement.IsSet())
	{
		FText TargetValueText = FText::AsNumber(TargetValue, &FormatOptions);
		FText ActualValueText = FText::AsNumber(ActualMeasurement.Get(), &FormatOptions);
		DisplayText = FText::Format(LOCTEXT("ParametricConstraintValueDisplay", "{0} ({1} actual)"), TargetValue, ActualValueText);
	}
	else if(ActualMeasurement.IsSet())
	{
		DisplayText = FText::AsNumber(ActualMeasurement.Get(), &FormatOptions);
	}
	else
	{
		DisplayText = FText::AsNumber(TargetValue, &FormatOptions);
	}

	return DisplayText;
}

float SMetaHumanCharacterEditorParametricConstraintView::GetParametricValue() const
{
	float MeasurementValue = IsPinned.Get() ? TargetMeasurement.Get() : ActualMeasurement.Get();
	return MeasurementValue;
}

void SMetaHumanCharacterEditorParametricConstraintView::OnBeginConstraintEditing() const
{
	OnBeginConstraintEditingDelegate.ExecuteIfBound();
}

void SMetaHumanCharacterEditorParametricConstraintView::OnConstraintTargetChanged(const float Value, bool bCommit) const
{
	const bool bIsPinned = true;
	OnParametricConstraintChangedDelegate.ExecuteIfBound(Value, bIsPinned, bCommit);
}

ECheckBoxState SMetaHumanCharacterEditorParametricConstraintView::GetConstraintChecked() const
{
	return IsPinned.Get() ? (ECheckBoxState::Checked) : ECheckBoxState::Unchecked;
}

void SMetaHumanCharacterEditorParametricConstraintView::OnConstraintPinnedChanged(ECheckBoxState CheckState) const
{
	bool bIsChecked = CheckState == ECheckBoxState::Checked;
	const bool bCommit = true;
	OnParametricConstraintChangedDelegate.ExecuteIfBound(TargetMeasurement.Get(), bIsChecked, bCommit);
}

void SMetaHumanCharacterEditorParametricConstraintsPanel::Construct(const FArguments& InArgs)
{
	if (InArgs._ListItemsSource)
	{
		ItemsSource = *InArgs._ListItemsSource;
	}

	OnBeginConstraintEditingDelegate = InArgs._OnBeginConstraintEditing;
	OnConstraintsChangedDelegate = InArgs._OnConstraintsChanged;
	DiagnosticView = InArgs._DiagnosticsView;

	ChildSlot
	[
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(InArgs._Label)
		.Padding(InArgs._Padding)
		.Content()
		[
			SAssignNew(ListView, SListView<FMetaHumanCharacterBodyConstraintItemPtr>)
			.ListItemsSource(&ItemsSource)
			.SelectionMode(ESelectionMode::None)
			.ListViewStyle(FMetaHumanCharacterEditorStyle::Get(), TEXT("MetaHumanCharacterEditorTools.ParametricBody.TableView"))
			.OnGenerateRow(this, &SMetaHumanCharacterEditorParametricConstraintsPanel::MakeConstraintRowWidget)
		]
		.HeaderContent()
		[
			SNew(SCheckBox)
			.Style(FMetaHumanCharacterEditorStyle::Get(), TEXT("MetaHumanCharacterEditorTools.ParametricBody.CheckBox"))
			.Visibility(DiagnosticView ? EVisibility::Hidden : EVisibility::Visible)
			.OnCheckStateChanged(this, &SMetaHumanCharacterEditorParametricConstraintsPanel::OnGroupPinCheckStateChanged)
			.IsChecked(this, &SMetaHumanCharacterEditorParametricConstraintsPanel::GetGroupPinCheckState)
		]
	];
}

static FText GetToolTipForConstraintName(const FName& ConstraintName)
{
	FText ToolTipText;
	if (ConstraintName == "Masculine/Feminine")
	{
		ToolTipText = LOCTEXT("Masculine/FeminineToolTipText", "Broadly define masculine or feminine traits");
	}
	else if (ConstraintName == "Muscularity")
	{
		ToolTipText = LOCTEXT("MuscularityToolTipText", "Makes changes to global muscle mass");
	}
	else if (ConstraintName == "Fat")
	{
		ToolTipText = LOCTEXT("FatToolTipText", "Makes changes to global fat mass");
	}
	else if (ConstraintName == "Height")
	{
		ToolTipText = LOCTEXT("HeightToolTipText", "Specify height (cm)");
	}
	else if (ConstraintName == "Across Shoulder")
	{
		ToolTipText = LOCTEXT("AcrossShoulderToolTipText", "Specify shoulder width (cm). When used in conjunction with Front Interscye, can help define shoulder shaping.");
	}
	else if (ConstraintName == "Shoulder to Apex")
	{
		ToolTipText = LOCTEXT("ShoulderToApexToolTipText", "Specify shoulder to apex (cm). Effects chest shaping.");
	}
	else if (ConstraintName == "Front Interscye")
	{
		ToolTipText = LOCTEXT("FrontInterscyeToolTipText", "Specify front interscye width (cm). When used in conjunction with Across Shoulders, can help define chest shaping.");
	}
	else if (ConstraintName == "Bust " || ConstraintName == "Chest")
	{
		ToolTipText = LOCTEXT("ChestToolTipText", "Specify chest/bust circumference (cm). When used in conjunction with Underbust helps separate back and cup measurements.");
	}
	else if (ConstraintName == "Bust Span")
	{
		ToolTipText = LOCTEXT("BustSpanToolTipText", "Specify bust span (cm)");
	}
	else if (ConstraintName == "Underbust")
	{
		ToolTipText = LOCTEXT("UnderbustToolTipText", "Specify underbust circumference (cm). When used in conjunction with Bust helps separate back and cup measurements.");
	}
	else if (ConstraintName == "Neck to Waist")
	{
		ToolTipText = LOCTEXT("NeckToWaistToolTipText", "Specify neck to waist length (cm).");
	}
	else if (ConstraintName == "Waist")
	{
		ToolTipText = LOCTEXT("WaistToolTipText", "Specify waist circumference (cm)");
	}
	else if (ConstraintName == "High Hip")
	{
		ToolTipText = LOCTEXT("HighHipToolTipText", "Specify high hip circumference (cm). Useful as a shaping modifier in conjunction with Hip.");
	}
	else if (ConstraintName == "Hip")
	{
		ToolTipText = LOCTEXT("HipToolTipText", "Specify hip circumference (cm)");
	}
	else if (ConstraintName == "Neck")
	{
		ToolTipText = LOCTEXT("NeckToolTipText", "Specify neck circumference (cm)");
	}
	else if (ConstraintName == "Neck Base")
	{
		ToolTipText = LOCTEXT("NeckBaseToolTipText", "Specify neck base circumference (cm)");
	}
	else if (ConstraintName == "Neck Length")
	{
		ToolTipText = LOCTEXT("NeckLengthToolTipText", "Specify neck length (cm)");
	}
	else if (ConstraintName == "Upper Arm Length")
	{
		ToolTipText = LOCTEXT("UpperArmLengthToolTipText", "Specify upper arm length (cm)");
	}
	else if (ConstraintName == "Lower Arm Length")
	{
		ToolTipText = LOCTEXT("LowerArmLengthToolTipText", "Specify lower arm length (cm)");
	}
	else if (ConstraintName == "Forearm")
	{
		ToolTipText = LOCTEXT("ForearmToolTipText", "Specify forearm circumference (cm)");
	}
	else if (ConstraintName == "Bicep")
	{
		ToolTipText = LOCTEXT("BicepToolTipText", "Specify bicep circumference (cm)");
	}
	else if (ConstraintName == "Elbow")
	{
		ToolTipText = LOCTEXT("ElbowToolTipText", "Specify elbow circumference (cm)");
	}
	else if (ConstraintName == "Wrist")
	{
		ToolTipText = LOCTEXT("WristToolTipText", "Specify wrist circumference (cm)");
	}
	else if (ConstraintName == "Inseam")
	{
		ToolTipText = LOCTEXT("InseamToolTipText", "Specify floor to crotch length (cm). When used in conjunction with Height, can be used to define upper/lower body height ratio.");
	}
	else if (ConstraintName == "Thigh")
	{
		ToolTipText = LOCTEXT("ThighToolTipText", "Specify thigh circumference (cm)");
	}
	else if (ConstraintName == "Knee")
	{
		ToolTipText = LOCTEXT("KneeToolTipText", "Specify knee circumference (cm)");
	}
	else if (ConstraintName == "Calf")
	{
		ToolTipText = LOCTEXT("CalfToolTipText", "Specify calf circumference (cm)");
	}
	else if (ConstraintName == "Shoulder Height")
	{
		ToolTipText = LOCTEXT("ShoulderHeightToolTipText", "Floor to shoulder height (read only)");
	}
	else if (ConstraintName == "Rise")
	{
		ToolTipText = LOCTEXT("RiseToolTipText", "Top of waistband in front, to top of waistband at the back (read only)");
	}
	else
	{
		// Default to constraint name
		ToolTipText = FText::FromName(ConstraintName);
	}

	return ToolTipText;
}

class SParametricConstraintTableRow
		: public STableRow<FMetaHumanCharacterBodyConstraintItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SParametricConstraintTableRow) { }

		SLATE_ARGUMENT(FMetaHumanCharacterBodyConstraintItemPtr, ConstraintItem)

		SLATE_ARGUMENT(EVisibility, PinVisibility)

		SLATE_ARGUMENT(bool, IsEnabled)

		SLATE_EVENT(FSimpleDelegate, OnBeginConstraintEditing)
		SLATE_EVENT(SMetaHumanCharacterEditorParametricConstraintsPanel::FOnConstraintChanged, OnConstraintsChanged)

		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		ConstraintItem = InArgs._ConstraintItem;
		OnBeginConstraintEditingDelegate = InArgs._OnBeginConstraintEditing;
		OnConstraintsChangedDelegate = InArgs._OnConstraintsChanged;

		
		if (ConstraintItem.IsValid())
		{
			STableRow<FMetaHumanCharacterBodyConstraintItemPtr>::Construct(
			STableRow<FMetaHumanCharacterBodyConstraintItemPtr>::FArguments()
			.ShowSelection(false)
			.Style(InArgs._Style)
			.Content()
			[
				SNew(SMetaHumanCharacterEditorParametricConstraintView)
					.ConstraintName(ConstraintItem->Name)
					.ToolTip(GetToolTipForConstraintName(ConstraintItem->Name))
					.PinVisibility(InArgs._PinVisibility)
					.IsEnabled(InArgs._IsEnabled)
					.MinValue(ConstraintItem->MinMeasurement)
					.MaxValue(ConstraintItem->MaxMeasurement)
					.TargetMeasurement_Lambda([this]()
					{
						return ConstraintItem->TargetMeasurement;
					})
					.OnBeginConstraintEditing_Lambda([this]()
					{
						OnBeginConstraintEditingDelegate.ExecuteIfBound();
					})
					.OnParametricConstraintChanged_Lambda([this](float NewValue, bool bIsPinned, bool bCommit)
					{
						ConstraintItem->TargetMeasurement = NewValue;
						ConstraintItem->bIsActive = bIsPinned;
						OnConstraintsChangedDelegate.ExecuteIfBound(bCommit);
					})
					.ActualMeasurement_Lambda([this]()
					{
						return ConstraintItem->ActualMeasurement;
					})
					.IsPinned_Lambda([this]()
					{
						return ConstraintItem->bIsActive;
					})
			], InOwnerTableView);
		}
	}
private:
	FMetaHumanCharacterBodyConstraintItemPtr ConstraintItem;
	FSimpleDelegate OnBeginConstraintEditingDelegate;
	SMetaHumanCharacterEditorParametricConstraintsPanel::FOnConstraintChanged OnConstraintsChangedDelegate;
	FTableRowStyle TransparentTableRowStyle;
};

TSharedRef<ITableRow> SMetaHumanCharacterEditorParametricConstraintsPanel::MakeConstraintRowWidget(FMetaHumanCharacterBodyConstraintItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SParametricConstraintTableRow, OwnerTable)
		.ConstraintItem(Item)
		.Style(FMetaHumanCharacterEditorStyle::Get(), TEXT("MetaHumanCharacterEditorTools.ParametricBody.TableRow"))
		.OnBeginConstraintEditing(this, &SMetaHumanCharacterEditorParametricConstraintsPanel::OnBeginConstraintEditing)
		.OnConstraintsChanged(this, &SMetaHumanCharacterEditorParametricConstraintsPanel::OnConstraintChanged)
		.PinVisibility(DiagnosticView ? EVisibility::Hidden : EVisibility::Visible)
		.IsEnabled(!DiagnosticView);
}

void SMetaHumanCharacterEditorParametricConstraintsPanel::OnGroupPinCheckStateChanged(ECheckBoxState CheckState)
{
	bool bGroupHasPinnedItems = false;
	bool bGroupHasUnpinnedItems = false;
	for (const FMetaHumanCharacterBodyConstraintItemPtr& Item : ItemsSource)
	{
		if (Item->bIsActive)
		{
			bGroupHasPinnedItems = true;
		}
		else
		{
			bGroupHasUnpinnedItems = true;
		}
	}

	// Pin group if checked or if current state has mix of pinned and unpinned always pin 
	bool bGroupActive = CheckState == ECheckBoxState::Checked;
	if (bGroupHasPinnedItems && bGroupHasUnpinnedItems)
	{
		bGroupActive = true;
	}

	for (const FMetaHumanCharacterBodyConstraintItemPtr& Item : ItemsSource)
	{
		Item->bIsActive = bGroupActive;
	}

	const bool bCommit = true;
	OnConstraintChanged(bCommit);
}

void SMetaHumanCharacterEditorParametricConstraintsPanel::OnBeginConstraintEditing()
{
	OnBeginConstraintEditingDelegate.ExecuteIfBound();
}

void SMetaHumanCharacterEditorParametricConstraintsPanel::OnConstraintChanged(bool bCommit)
{
	OnConstraintsChangedDelegate.ExecuteIfBound(bCommit);
}

ECheckBoxState SMetaHumanCharacterEditorParametricConstraintsPanel::GetGroupPinCheckState() const
{
	ECheckBoxState GroupPinState = ECheckBoxState::Unchecked;
	bool bAllPinned = true;
	for (const FMetaHumanCharacterBodyConstraintItemPtr& Item : ItemsSource)
	{
		if (Item->bIsActive)
		{
			GroupPinState = ECheckBoxState::Undetermined;
		}
		bAllPinned = bAllPinned & Item->bIsActive;
	}

	if (bAllPinned)
	{
		GroupPinState = ECheckBoxState::Checked;
	}

	return GroupPinState;
}
#undef LOCTEXT_NAMESPACE
