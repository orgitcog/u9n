// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorPreviewSettingsView.h"
#include "Misc/TransactionObjectEvent.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Animation/AnimSequence.h"
#include "UObject/SoftObjectPtr.h"
#include "MetaHumanCharacterEditorLog.h"

#include <Features/IModularFeatures.h>
#include <LiveLinkTypes.h>
#include "ILiveLinkClient.h"
#include "Logging/StructuredLog.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

void SMetaHumanCharacterEditorPreviewSettingsView::Construct(const FArguments& InArgs)
{
	PreviewSceneDescription = InArgs._SettingsObject;

	// Create an options property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;

	SettingsDetailsView = EditModule.CreateDetailView(DetailsViewArgs);

	check(PreviewSceneDescription);

	SettingsDetailsView->SetObject(PreviewSceneDescription.Get());
	

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(2.0f, 1.0f, 2.0f, 1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SettingsDetailsView->AsShared()
				]
			]
		];
}

void SMetaHumanCharacterEditorPreviewSettingsView::AddReferencedObjects(FReferenceCollector& Collector)
{
	if(PreviewSceneDescription != nullptr)
	{
		Collector.AddReferencedObject(PreviewSceneDescription);
	}
}

FString SMetaHumanCharacterEditorPreviewSettingsView::GetReferencerName() const
{
	return FString("SMetaHumanCharacterEditorPreviewSettingsView");
}

// UMetaHumanCharacterEditorPreviewSceneDescription

UMetaHumanCharacterEditorPreviewSceneDescription::UMetaHumanCharacterEditorPreviewSceneDescription()
{
	SetFlags(RF_Transactional);

	TemplateAnimationDataTable = NewObject<UDataTable>();
	TemplateAnimationDataTable->RowStruct = FMetaHumanTemplateAnimationRow::StaticStruct();
}

TArray<FName> UMetaHumanCharacterEditorPreviewSceneDescription::GetTemplateAnimationOptions() const
{
	TArray<FName> AnimTableRowNames = TemplateAnimationDataTable->GetRowNames();
	return AnimTableRowNames;
}

void UMetaHumanCharacterEditorPreviewSceneDescription::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo && TransactionEvent.HasPropertyChanges())
	{
		const TArray<FName>& PropertyNames = TransactionEvent.GetChangedProperties();

		for (const FName& PropertyName : PropertyNames)
		{
			SceneDescriptionPropertyChanged(PropertyName);
		}
	}
}


FName UMetaHumanCharacterEditorPreviewSceneDescription::GetFirstLiveLinkSubject() const
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

		TArray<FLiveLinkSubjectKey> SubjectKeys = LiveLinkClient.GetSubjects(/*bIncludeDisabledSubject*/false, /*bIncludeVirtualSubject*/false);
		if (!SubjectKeys.IsEmpty())
		{
			return SubjectKeys[0].SubjectName;
		}
	}

	return {};
}

UAnimSequence* UMetaHumanCharacterEditorPreviewSceneDescription::GetTemplateAnimation(const bool bIsFaceAnimation, const FName& AnimationName)
{
	if(!TemplateAnimationDataTable)
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Template Animation data table missing.");
		return nullptr;
	}

	TArray<FName> AnimTableRowNames = TemplateAnimationDataTable->GetRowNames();
	const bool bWarnIfMissing = false;
	FMetaHumanTemplateAnimationRow* AnimTableRow = TemplateAnimationDataTable->FindRow<FMetaHumanTemplateAnimationRow>(AnimationName, {}, bWarnIfMissing);
	
	if(AnimTableRow)
	{
		return bIsFaceAnimation ? AnimTableRow->FaceAnimation.LoadSynchronous() : AnimTableRow->BodyAnimation.LoadSynchronous();
	}

	return nullptr;
}

void UMetaHumanCharacterEditorPreviewSceneDescription::SetAnimationController(EMetaHumanCharacterAnimationController InAnimationController)
{
	AnimationController = InAnimationController;

	// Get current animations
	UAnimSequence* FaceAnim = FaceAnimationType == EMetaHumanAnimationType::SpecificAnimation ? FaceSpecificAnimation.Get() : GetTemplateAnimation(true, FaceTemplateAnimation);
	UAnimSequence* BodyAnim = BodyAnimationType == EMetaHumanAnimationType::SpecificAnimation ? BodySpecificAnimation.Get() : GetTemplateAnimation(false, BodyTemplateAnimation);

	OnAnimationControllerChanged.ExecuteIfBound(AnimationController, FaceAnim, BodyAnim);

	// Auto-select the first LiveLink subject in case we haven't selected one yet.
	if (AnimationController == EMetaHumanCharacterAnimationController::LiveLink && LiveLinkSubjectName.Name.IsNone())
	{
		LiveLinkSubjectName = GetFirstLiveLinkSubject();
		OnLiveLinkSubjectChanged.ExecuteIfBound(LiveLinkSubjectName);
	}
}

void UMetaHumanCharacterEditorPreviewSceneDescription::SceneDescriptionPropertyChanged(const FName& PropertyName)
{
	UAnimSequence* FaceAnim = FaceAnimationType == EMetaHumanAnimationType::SpecificAnimation ? FaceSpecificAnimation.Get() : GetTemplateAnimation(true, FaceTemplateAnimation);
	UAnimSequence* BodyAnim = BodyAnimationType == EMetaHumanAnimationType::SpecificAnimation ? BodySpecificAnimation.Get() : GetTemplateAnimation(false, BodyTemplateAnimation);

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, AnimationController))
	{
		SetAnimationController(AnimationController);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, FaceSpecificAnimation))
	{
		OnAnimationChanged.ExecuteIfBound(FaceSpecificAnimation, BodyAnim);
	}
	else
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, FaceTemplateAnimation))
		{
			OnAnimationChanged.ExecuteIfBound(GetTemplateAnimation(true, FaceTemplateAnimation), BodyAnim);
		}
		// We don't need else here because when AnimationType changes it only affects visibility
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, FaceAnimationType))
	{
		OnAnimationChanged.ExecuteIfBound(FaceAnim, BodyAnim);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, BodySpecificAnimation))
	{
		OnAnimationChanged.ExecuteIfBound(FaceAnim, BodySpecificAnimation);
	}
	else
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, BodyTemplateAnimation))
		{
			OnAnimationChanged.ExecuteIfBound(FaceAnim, GetTemplateAnimation(false, BodyTemplateAnimation));
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, BodyAnimationType))
	{
		OnAnimationChanged.ExecuteIfBound(FaceAnim, BodyAnim);
	}


	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, PlayRate))
	{
		OnPlayRateChanged.ExecuteIfBound(PlayRate);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, LiveLinkSubjectName))
	{
		OnLiveLinkSubjectChanged.ExecuteIfBound(LiveLinkSubjectName);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, PreviewAssemblyGrooms))
	{
		OnGroomHiddenChanged.ExecuteIfBound(PreviewAssemblyGrooms);
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorPreviewSceneDescription, PreviewAssemblyClothing))
	{
		OnClothingHiddenChanged.ExecuteIfBound(PreviewAssemblyClothing);
	}
}

void UMetaHumanCharacterEditorPreviewSceneDescription::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();

	SceneDescriptionPropertyChanged(PropertyName);
}

void UMetaHumanCharacterEditorPreviewSceneDescription::AddTemplateAnimationsFromDataTable(const FSoftObjectPath& DataTableObjectPath)
{
	TSoftObjectPtr<class UDataTable> DataTablePtr(DataTableObjectPath);
	UDataTable* DataTable = DataTablePtr.LoadSynchronous();
	if (DataTable)
	{
		for (const FName& RowName : DataTable->GetRowNames())
		{
			const bool bWarnIfMissing = false;
			FMetaHumanTemplateAnimationRow* RowToCopy = DataTable->FindRow<FMetaHumanTemplateAnimationRow>(RowName, {}, bWarnIfMissing);
			if (RowToCopy)
			{
				FMetaHumanTemplateAnimationRow NewRow;
				NewRow.BodyAnimation = RowToCopy->BodyAnimation;
				NewRow.FaceAnimation = RowToCopy->FaceAnimation;

				if (DefaultBodyTemplateAnimationName.IsNone())
				{
					DefaultBodyTemplateAnimationName = RowName;
				}

				if (DefaultFaceTemplateAnimationName.IsNone())
				{
					DefaultFaceTemplateAnimationName = RowName;
				}

				TemplateAnimationDataTable->AddRow(RowName, NewRow);
			}
		}
	}
}

void UMetaHumanCharacterEditorPreviewSceneDescription::OnRiggingStateChanged(TNotNull<const UMetaHumanCharacter*> Character, EMetaHumanCharacterRigState State)
{
	UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	if (MetaHumanCharacterSubsystem)
	{
		if (State == EMetaHumanCharacterRigState::Rigged)
		{
			bAnimationControllerEnabled = true;
			SetAnimationController(EMetaHumanCharacterAnimationController::AnimSequence);
		}
		else if (State == EMetaHumanCharacterRigState::Unrigged)
		{
			SetAnimationController(EMetaHumanCharacterAnimationController::None);
			bAnimationControllerEnabled = false;
		}
	}
}

#undef LOCTEXT_NAMESPACE
