// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanIdentityPartsClassCombo.h"

#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"

#include "SPositiveActionButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentity.h"

#define LOCTEXT_NAMESPACE "MetaHumanIdentityComponentsClassCombo"


void SMetaHumanIdentityPartsClassCombo::Construct(const FArguments& InArgs)
{
	Identity = InArgs._Identity;
	OnIdentityPartClassSelectedDelegate = InArgs._OnIdentityPartClassSelected;
	OnIdentityPoseClassSelectedDelegate = InArgs._OnIdentityPoseClassSelected;
	OnIsIdentityPartClassEnabledDelegate = InArgs._OnIsIdentityPartClassEnabled;
	OnIsIdentityPoseClassEnabledDelegate = InArgs._OnIsIdentityPoseClassEnabled;

	ChildSlot
	[
		SNew(SPositiveActionButton)
		.Text(LOCTEXT("AddLabel", "Add"))
		.OnGetMenuContent(this, &SMetaHumanIdentityPartsClassCombo::MakeAddPartMenuWidget)
	];
}

TSharedRef<SWidget> SMetaHumanIdentityPartsClassCombo::MakeAddPartMenuWidget() const
{
	const bool bShouldCloseAfterMenuSelection = true;
	FMenuBuilder MenuBuilder{ bShouldCloseAfterMenuSelection, MakeShared<FUICommandList>() };

	MenuBuilder.BeginSection(TEXT("AddNewPart"), LOCTEXT("AddNewPartMenuSection", "Create"));
	{
		MenuBuilder.AddSubMenu(LOCTEXT("AddPart", "Add Part"),
							   LOCTEXT("AddPartTooltip", "Add a new part to this MetaHuman Identity"),
							   FNewMenuDelegate::CreateSP(this, &SMetaHumanIdentityPartsClassCombo::MakeAddPartSubMenu));

		MenuBuilder.AddSubMenu(LOCTEXT("AddPose", "Add Pose"),
							   LOCTEXT("AddPoseTooltip", "Add a new pose for this MetaHuman Identity"),
							   FNewMenuDelegate::CreateSP(this, &SMetaHumanIdentityPartsClassCombo::MakeAddPoseSubMenu));

		// TODO: Enable this when needed
		// MenuBuilder.AddSubMenu(LOCTEXT("AddPoseGroup", "Add Pose Group"),
		// 					   LOCTEXT("AddPoseGroupTooltip", "Add a new pose group for this MetaHuman Identity"),
		// 					   FNewMenuDelegate::CreateSP(this, &SMetaHumanIdentityPartsClassCombo::MakeAddPoseGroupSubMenu));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SMetaHumanIdentityPartsClassCombo::MakeAddPartSubMenu(FMenuBuilder& InMenuBuilder) const
{
	InMenuBuilder.BeginSection(TEXT("AddNewPart"), LOCTEXT("AddNewPartSubmenuSection", "Create Part"));
	{
		// Get all classes that derive from UMetaHumanIdentityPart and create a menu entry
		TArray<UClass*> IdentityPartClasses;
		GetDerivedClasses(UMetaHumanIdentityPart::StaticClass(), IdentityPartClasses);

		// TODO: Remove this when more classes are allowed to be created by the editor
		static const TArray<UClass*> AllowedPartClasses = { UMetaHumanIdentityFace::StaticClass() };

		for (UClass* IdentityPartClass : IdentityPartClasses)
		{
			if (!AllowedPartClasses.Contains(IdentityPartClass))
			{
				continue;
			}

			// Get the CDO for the Part class so we can query the Part's display name
			const UMetaHumanIdentityPart* IdentityPartCDO = GetDefault<UMetaHumanIdentityPart>(IdentityPartClass);

			const FString EntryLabel = FString::Printf(TEXT("Add %s"), *IdentityPartCDO->GetPartName().ToString());

			InMenuBuilder.AddMenuEntry(FText::FromString(EntryLabel),
										TAttribute<FText>::CreateSP(this, &SMetaHumanIdentityPartsClassCombo::GetAddPartTooltip, IdentityPartClass),
										FSlateIcon(),
										FUIAction(FExecuteAction::CreateLambda([this, IdentityPartClass]
											{
												OnIdentityPartClassSelectedDelegate.ExecuteIfBound(IdentityPartClass);
											}),
											FCanExecuteAction::CreateLambda([this, IdentityPartClass]
											{
												if (OnIsIdentityPartClassEnabledDelegate.IsBound())
												{
													return OnIsIdentityPartClassEnabledDelegate.Execute(IdentityPartClass);
												}

												return false;
											}))
									   );
		}
	}
	InMenuBuilder.EndSection();
}

void SMetaHumanIdentityPartsClassCombo::MakeAddPoseSubMenu(FMenuBuilder& InMenuBuilder) const
{
	InMenuBuilder.BeginSection(TEXT("AddNewPose"), LOCTEXT("CreateNewPose", "Create Pose"));
	{
		// TODO: Add other poses
		InMenuBuilder.AddMenuEntry(LOCTEXT("AddNeutralLabel", "Add Neutral"),
								   TAttribute<FText>::CreateSP(this, &SMetaHumanIdentityPartsClassCombo::GetAddNeutralTooltip),
								   FSlateIcon(),
								   FUIAction(FExecuteAction::CreateLambda([this]
											{
												OnIdentityPoseClassSelectedDelegate.ExecuteIfBound(UMetaHumanIdentityPose::StaticClass(), EIdentityPoseType::Neutral);
											}),
											FCanExecuteAction::CreateLambda([this]
											{
												if (OnIsIdentityPoseClassEnabledDelegate.IsBound())
												{
													return OnIsIdentityPoseClassEnabledDelegate.Execute(UMetaHumanIdentityPose::StaticClass(), EIdentityPoseType::Neutral);
												}

												return false;
											}))
									);

		InMenuBuilder.AddMenuEntry(LOCTEXT("AddTeethLabel", "Add Teeth"),
								   TAttribute<FText>::CreateSP(this, &SMetaHumanIdentityPartsClassCombo::GetAddTeethTooltip),
								   FSlateIcon(),
								   FUIAction(FExecuteAction::CreateLambda([this]
											{
												OnIdentityPoseClassSelectedDelegate.ExecuteIfBound(UMetaHumanIdentityPose::StaticClass(), EIdentityPoseType::Teeth);
											}),
											FCanExecuteAction::CreateLambda([this]
											{
												if (OnIsIdentityPoseClassEnabledDelegate.IsBound())
												{
													return OnIsIdentityPoseClassEnabledDelegate.Execute(UMetaHumanIdentityPose::StaticClass(), EIdentityPoseType::Teeth);
												}

												return false;
											}))
									);
	}
	InMenuBuilder.EndSection();
}

FText SMetaHumanIdentityPartsClassCombo::GetAddPartTooltip(TSubclassOf<UMetaHumanIdentityPart> InPart) const
{
	// Get the CDO for the Part class so we can query the Part's display name
	const UMetaHumanIdentityPart* IdentityPartCDO = GetDefault<UMetaHumanIdentityPart>(InPart);
	FText PartTooltipText = IdentityPartCDO->GetPartDescription();

	if (OnIsIdentityPartClassEnabledDelegate.IsBound())
	{
		if (OnIsIdentityPartClassEnabledDelegate.Execute(InPart))
		{
			return PartTooltipText;
		}
		else
		{
			return FText::Format(LOCTEXT("AddPartDeletionNeededTooltip", "{0}\n\nDelete current {1} Part to enable this option."), PartTooltipText, IdentityPartCDO->GetPartName());
		}
	}
	return FText();
}

FText SMetaHumanIdentityPartsClassCombo::GetAddNeutralTooltip() const
{
	FText TooltipText = LOCTEXT("IdentityPartsAddNeutralDescription", "Add a Pose with Neutral facial expression to the MetaHuman Identity");

	if (Identity.IsValid())
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (!Face->FindPoseByType(EIdentityPoseType::Neutral))
			{
				return TooltipText;
			}
			else
			{
				return FText::Format(LOCTEXT("IdentityPartsAddNeutralTooltipDisabled", "{0}\n\nTo enable this option, delete the existing Neutral Pose"), TooltipText);
			}
		}
		else
		{
			return FText::Format(LOCTEXT("IdentityPartsAddTeethFaceMissing", "{0}\n\nTo enable this option, first add Face Part to MetaHuman Identity by using\n+Add->Add Part->Add Face in MetaHuman Identity Parts Tree View,\nor Create Components button on the Toolbar"), TooltipText);
		}
	}
	return FText();
}

FText SMetaHumanIdentityPartsClassCombo::GetAddTeethTooltip() const
{
	FText TooltipText = LOCTEXT("AddTeethDescription", "Add a Pose with Show Teeth facial expression to the MetaHuman Identity\n\nUsed by Fit Teeth command after obtaining a Skeletal Mesh with MetaHuman\nDNA through Mesh to MetaHuman command");

	if (Identity.IsValid())
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			//first check if there is a neutral pose, if not, notify the user to add it
			if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
			{
				if (Face->bIsConformed)
				{
					if (Face->bIsAutoRigged)
					{
						if (!Face->FindPoseByType(EIdentityPoseType::Teeth))
						{
							return TooltipText;
						}
						else
						{
							return FText::Format(LOCTEXT("IdentityPartsAddTeethTooltipDisabled", "{0}\n\nTo enable this option, delete the existing Teeth Pose"), TooltipText);
						}
					}
					else
					{
						return FText::Format(LOCTEXT("IdentityPartsAddFaceNotConformed", "{0}\n\nTo enable this option, first use Mesh To MetaHuman command\non the toolbar with Neutral Pose data for the face"), TooltipText);
					}
				}
				else
				{
					return FText::Format(LOCTEXT("IdentityPartsAddDNANotObtained", "{0}\n\nTo enable this option, first conform a Template Mesh to Capture Data\nusing MetaHuman Identity Solve command on the toolbar"), TooltipText);
				}
			}
			else
			{
				if (Face->FindPoseByType(EIdentityPoseType::Teeth))
				{
					return FText::Format(LOCTEXT("IdentityPartsTeethExistNeutralMissing", "{0}\n\nTo enable this option, delete the existing Teeth Pose"), TooltipText);
				}
				else
				{
					return FText::Format(LOCTEXT("IdentityPartsNeutralPoseMissing", "{0}\n\nTo enable this option, first add Neutral Pose to MetaHuman Identity Parts Tree and process it\nusing Mesh to MetaHuman command on the toolbar"), TooltipText);
				}
			}
		}
		else
		{
			return FText::Format(LOCTEXT("IdentityPartsAddTeethFaceMissing", "{0}\n\nTo enable this option, first add Face Part to MetaHuman Identity by using\n+Add->Add Part->Add Face in MetaHuman Identity Parts Tree View,\nor Create Components button on the Toolbar"), TooltipText);
		}
	}
	
	return FText();
}



void SMetaHumanIdentityPartsClassCombo::MakeAddPoseGroupSubMenu(FMenuBuilder& InMenuBuilder) const
{
	InMenuBuilder.BeginSection(TEXT("AddNewPoseGroup"), LOCTEXT("CreateNewPoseGroup", "CreatePoseGroup"));
	{
		// TODO: Add pose group options
	}
	InMenuBuilder.EndSection();
}

#undef LOCTEXT_NAMESPACE