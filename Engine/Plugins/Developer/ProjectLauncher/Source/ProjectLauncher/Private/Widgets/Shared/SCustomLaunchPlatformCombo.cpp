// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Shared/SCustomLaunchPlatformCombo.h"

#include "SlateOptMacros.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "PlatformInfo.h"


#define LOCTEXT_NAMESPACE "SCustomLaunchPlatformCombo"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SCustomLaunchPlatformCombo::Construct(const FArguments& InArgs)
{
	OnSelectionChanged = InArgs._OnSelectionChanged;
	SelectedPlatforms = InArgs._SelectedPlatforms;
	bBasicPlatformsOnly = InArgs._BasicPlatformsOnly;

	FSlateFontInfo Font = InArgs._Font.IsSet() ? InArgs._Font.Get() : InArgs._TextStyle->Font;

	ChildSlot
	[
		SAssignNew(PlatformsComboBox, SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&PlatformsList)
		.OnGenerateWidget( this, &SCustomLaunchPlatformCombo::OnGeneratePlatformListWidget )
		.OnSelectionChanged( this, &SCustomLaunchPlatformCombo::OnPlatformSelectionChanged )
		[
			SNew(SHorizontalBox)
					
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4,0)
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(16,16))
				.Image(this, &SCustomLaunchPlatformCombo::GetSelectedPlatformBrush)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4,0)
			[
				SNew(STextBlock)
				.Text(this, &SCustomLaunchPlatformCombo::GetSelectedPlatformName)
				.Font(Font)
			]
		]
	];

	for (const PlatformInfo::FTargetPlatformInfo* PlatformInfo : PlatformInfo::GetPlatformInfoArray())
	{
		if( bBasicPlatformsOnly && PlatformInfo->PlatformType != EBuildTargetType::Game)
		{
			continue;
		}

		PlatformsList.Add(MakeShared<FString>(PlatformInfo->Name.ToString()));
	}

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION



BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SCustomLaunchPlatformCombo::OnGeneratePlatformListWidget( TSharedPtr<FString> Platform ) const
{
	const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(*Platform));
	if (PlatformInfo != nullptr)
	{
		int32 Indent = PlatformInfo->IsVanilla() ? 0 : 16;

		return SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4+Indent,4,4,4)
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(16,16))
				.Image(FAppStyle::GetBrush(PlatformInfo->GetIconStyleName(EPlatformIconSize::Normal)))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(PlatformInfo->DisplayName)
			]
		;
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION



void SCustomLaunchPlatformCombo::OnPlatformSelectionChanged( TSharedPtr<FString> Platform, ESelectInfo::Type InSelectInfo )
{
	TArray<FString> Platforms;
	if (Platform.IsValid() && !Platform->IsEmpty())
	{
		Platforms.Add(*Platform);
	}

	SelectedPlatforms.Set(Platforms);
	OnSelectionChanged.ExecuteIfBound(SelectedPlatforms.Get());
}



const FSlateBrush* SCustomLaunchPlatformCombo::GetSelectedPlatformBrush() const
{
	const TArray<FString>& Platforms = SelectedPlatforms.Get();

	if (Platforms.Num() == 1)
	{
		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(Platforms[0]));
		if (PlatformInfo != nullptr)
		{
			return FAppStyle::GetBrush(PlatformInfo->GetIconStyleName(EPlatformIconSize::Normal));
		}
	}
	else if (Platforms.Num() > 1)
	{
		return FAppStyle::Get().GetBrush("Icons.WarningWithColor");
	}

	return FStyleDefaults::GetNoBrush();
}



FText SCustomLaunchPlatformCombo::GetSelectedPlatformName() const
{
	const TArray<FString>& Platforms = SelectedPlatforms.Get();

	if (Platforms.Num() == 1)
	{
		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(Platforms[0]));
		if (PlatformInfo != nullptr)
		{
			return PlatformInfo->DisplayName;
		}
	}
	else if (Platforms.Num() > 1)
	{
		return LOCTEXT("TooManyPlatforms", "Multiple platforms (unsupported)");
	}

	return LOCTEXT("NoPlatform", "(no platform)");
}


#undef LOCTEXT_NAMESPACE
