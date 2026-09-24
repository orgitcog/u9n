// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAnimDetailsOptions.h"

#include "AnimDetails/AnimDetailsSettings.h"
#include "ToolMenuEntry.h"
#include "ToolMenus.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SAnimDetailsOptions"

namespace UE::ControlRigEditor
{
	void SAnimDetailsOptions::Construct(const FArguments& InArgs)
	{
		OnOptionsChangedDelegate = InArgs._OnOptionsChanged;

		ChildSlot
		[
			SNew(SComboButton)
			.HasDownArrow(false)
			.ContentPadding(0.0f)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewOptions")))
			.MenuContent()
			[
				MakeOptionsMenu()
			]
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("DetailsView.ViewOptions"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}

	TSharedRef<SWidget> SAnimDetailsOptions::MakeOptionsMenu()
	{
		constexpr const TCHAR* AnimDetailsOptionsMenuName = TEXT("AnimDetails.Options");

		if (!UToolMenus::Get()->IsMenuRegistered(AnimDetailsOptionsMenuName))
		{
			UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(AnimDetailsOptionsMenuName);

			FToolMenuSection& OptionsSection = Menu->AddSection("Options");
			{
				constexpr bool bIndent = false;
				constexpr bool bSearchable = false;
				constexpr bool bNoPadding = true;

				// Num fractional digits option
				{
					const TSharedRef<SWidget> NumFractionalDigitsOptionWidget =
						SNew(SEditableTextBox)
						.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
						.Justification(ETextJustify::Right)
						.MinDesiredWidth(40.f)
						.SelectAllTextWhenFocused(true)
						.Text(this, &SAnimDetailsOptions::GetNumFractionalDigitsText)
						.OnTextCommitted(this, &SAnimDetailsOptions::OnNumFractionalDigitsCommitted);

					OptionsSection.AddEntry(
						FToolMenuEntry::InitWidget(
							"NumFractionalDigitsOptionWidget", 
							NumFractionalDigitsOptionWidget,
							LOCTEXT("NumFractionalDigitsLabel", "Num Fractional Digits"),
							bIndent,
							bSearchable,
							bNoPadding,
							LOCTEXT("NumFractionalDigitsTooltip", "Sets the displayed num fractional digits"))
					);
				}

				// LMB selects range option
				{
					const TSharedRef<SWidget> LMBSelectsRangeOptionWidget =
						SNew(SBox)
						.MinDesiredWidth(40.f)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SCheckBox)
							.HAlign(HAlign_Right)
							.IsChecked(this, &SAnimDetailsOptions::GetLMBSelectsRangeCheckState)
							.OnCheckStateChanged(this, &SAnimDetailsOptions::OnLMBSelectsRangeCheckStateChanged)
						];

					OptionsSection.AddEntry(
						FToolMenuEntry::InitWidget(
							"LMBSelectsRangeOptionWidget",
							LMBSelectsRangeOptionWidget,
							LOCTEXT("LMBSelectsRangeLabel", "LMB selects range"),
							bIndent,
							bSearchable,
							bNoPadding,
							LOCTEXT("LMBSelectsRangeTooltip", "When checked, selects a range when the left mouse button is down"))
						);
				}
			}
		}

		return UToolMenus::Get()->GenerateWidget(AnimDetailsOptionsMenuName, FToolMenuContext());
	}

	FText SAnimDetailsOptions::GetNumFractionalDigitsText() const
	{
		const int32 NumFractionalDigits = GetDefault<UAnimDetailsSettings>()->NumFractionalDigits;
		return FText::FromString(FString::FromInt(NumFractionalDigits));
	}

	void SAnimDetailsOptions::OnNumFractionalDigitsCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
	{
		uint8 Value = 0.f;
		if (LexTryParseString(Value, *NewText.ToString()))
		{
			UAnimDetailsSettings* Settings = GetMutableDefault<UAnimDetailsSettings>();
			Settings->NumFractionalDigits = Value;

			Settings->SaveConfig();

			OnOptionsChangedDelegate.ExecuteIfBound();
		}
	}

	ECheckBoxState SAnimDetailsOptions::GetLMBSelectsRangeCheckState() const
	{
		return GetDefault<UAnimDetailsSettings>()->bLMBSelectsRange ?
			ECheckBoxState::Checked :
			ECheckBoxState::Unchecked;
	}

	void SAnimDetailsOptions::OnLMBSelectsRangeCheckStateChanged(ECheckBoxState CheckBoxState)
	{
		UAnimDetailsSettings* Settings = GetMutableDefault<UAnimDetailsSettings>();
		Settings->bLMBSelectsRange = CheckBoxState == ECheckBoxState::Checked ? true : false;

		Settings->SaveConfig();

		OnOptionsChangedDelegate.ExecuteIfBound();
	}
}

#undef LOCTEXT_NAMESPACE
