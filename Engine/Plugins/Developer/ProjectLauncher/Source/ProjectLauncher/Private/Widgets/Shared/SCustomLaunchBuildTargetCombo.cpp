// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Shared/SCustomLaunchBuildTargetCombo.h"

#include "SlateOptMacros.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "DesktopPlatformModule.h"
#include "Misc/Paths.h"
#include "Settings/ProjectPackagingSettings.h"

#define LOCTEXT_NAMESPACE "SCustomLaunchBuildTargetCombo"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SCustomLaunchBuildTargetCombo::Construct(const FArguments& InArgs)
{
	OnSelectionChanged = InArgs._OnSelectionChanged;
	SelectedProject = InArgs._SelectedProject;
	SelectedBuildTarget = InArgs._SelectedBuildTarget;
	SupportedTargetTypes = InArgs._SupportedTargetTypes;

	FSlateFontInfo Font = InArgs._Font.IsSet() ? InArgs._Font.Get() : InArgs._TextStyle->Font;

	ChildSlot
	[
		SNew(SComboButton)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SCustomLaunchBuildTargetCombo::GetBuildTargetName)
			.Font(Font)
		]
		.OnGetMenuContent(this, &SCustomLaunchBuildTargetCombo::MakeBuildTargetSelectionWidget)
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION



BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SCustomLaunchBuildTargetCombo::MakeBuildTargetSelectionWidget()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	{
		FString DefaultBuildTarget = GetDefaultBuildTargetName();
		FText DefaultBuildTargetName = DefaultBuildTarget.IsEmpty() ? LOCTEXT("DefaultBuildTargetName", "Project Default") : FText::Format(LOCTEXT("CurProjectDefaultBuildTargetName", "{0} (Project Default)"), FText::FromString(DefaultBuildTarget));
		MenuBuilder.AddMenuEntry(
			DefaultBuildTargetName, 
			LOCTEXT("DefaultBuildTargetActionHint", "Use the project default build target."), 
			FSlateIcon(), 
			FUIAction(
				FExecuteAction::CreateSP(this, &SCustomLaunchBuildTargetCombo::SetBuildTargetName, FString()),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda( [this]() { return SelectedBuildTarget.Get().IsEmpty() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			), 
			NAME_None, 
			EUserInterfaceActionType::Check);


		if (SelectedProject.IsSet())
		{
			MenuBuilder.AddMenuSeparator();

			const TArray<FTargetInfo>& BuildTargets = FDesktopPlatformModule::Get()->GetTargetsForProject(SelectedProject.Get());
			for (const FTargetInfo& BuildTarget : BuildTargets)
			{
				if (BuildTarget.Type == EBuildTargetType::Program)
				{
					continue;
				}

				if (!SupportedTargetTypes.IsSet() || SupportedTargetTypes.Get().IsEmpty() || SupportedTargetTypes.Get().Contains(BuildTarget.Type))
				{
					
					MenuBuilder.AddMenuEntry(
						FText::FromString(BuildTarget.Name), 
						FText::FromString(BuildTarget.Path), 
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateSP(this, &SCustomLaunchBuildTargetCombo::SetBuildTargetName, BuildTarget.Name),
							FCanExecuteAction(),
							FGetActionCheckState::CreateLambda( [this, BuildTargetName = BuildTarget.Name]() { return (SelectedBuildTarget.Get() == BuildTargetName) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						),
						NAME_None,
						EUserInterfaceActionType::Check);
				}
				else
				{
					MenuBuilder.AddMenuEntry(
						FText::FromString(BuildTarget.Name), 
						LOCTEXT("NotCompatible", "The selected platform does not support this build target type"), 
						FSlateIcon(), 
						FUIAction(
							FExecuteAction::CreateSP(this, &SCustomLaunchBuildTargetCombo::SetBuildTargetName, BuildTarget.Name),
							FCanExecuteAction::CreateLambda([]() { return false; })
						));
				}
			}
		}
	}

	return MenuBuilder.MakeWidget();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


FText SCustomLaunchBuildTargetCombo::GetBuildTargetName() const
{
	FString BuildTargetName = SelectedBuildTarget.Get();

	if (BuildTargetName.IsEmpty())
	{
		FString DefaultBuildTarget = GetDefaultBuildTargetName();
		if (DefaultBuildTarget.IsEmpty())
		{
			return LOCTEXT("DefaultBuildTargetName", "Project Default");
		}
		else
		{
			return FText::Format(LOCTEXT("CurProjectDefaultBuildTargetName", "{0} (Project Default)"), FText::FromString(DefaultBuildTarget));
		}
	}

	return FText::FromString(BuildTargetName);
}

void SCustomLaunchBuildTargetCombo::SetBuildTargetName(FString BuildTargetName)
{
	OnSelectionChanged.ExecuteIfBound(BuildTargetName);
}

FString SCustomLaunchBuildTargetCombo::GetDefaultBuildTargetName() const
{
	if (SelectedProject.IsSet())
	{
		// if this is the current in-editor project, we can check the default setting
		if (FPaths::IsProjectFilePathSet() && SelectedProject.Get() == FPaths::GetProjectFilePath())
		{
			UProjectPackagingSettings* AllPlatformPackagingSettings = GetMutableDefault<UProjectPackagingSettings>();
			if (AllPlatformPackagingSettings != nullptr && !AllPlatformPackagingSettings->BuildTarget.IsEmpty())
			{
				return AllPlatformPackagingSettings->BuildTarget;
			}
		}

		// if there is only one game build target, it will be used by default
		const TArray<FTargetInfo>& BuildTargets = FDesktopPlatformModule::Get()->GetTargetsForProject(SelectedProject.Get());
		const TArray<FTargetInfo> GameBuildTargets = BuildTargets.FilterByPredicate([](const FTargetInfo& TargetInfo)
		{
			return TargetInfo.Type == EBuildTargetType::Game;
		});
		if (GameBuildTargets.Num() == 1)
		{
			return GameBuildTargets[0].Name;
		}

		// multiple (or no) game targets... would need to read the BuildTarget from the other project's ini file
		// ... fixme ...
	}

	return FString();
}


#undef LOCTEXT_NAMESPACE
