// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MetaHumanIdentity.h"
#include "ImageSequencePathChecker.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityLog.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanCoreEditorModule.h"
#include "MetaHumanIdentityAssetEditor.h"
#include "MetaHumanContourData.h"
#include "MetaHumanContourDataVersion.h"
#include "MetaHumanSupportedRHI.h"
#include "MetaHumanMinSpec.h"
#include "Misc/MessageDialog.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"

#define LOCTEXT_NAMESPACE "MetaHuman"

static void UpdateImageSequencePathChecker(
	const UMetaHumanIdentity& InIdentity, 
	UE::CaptureData::FImageSequencePathChecker& OutImageSequencePathChecker
)
{
	const UMetaHumanIdentityFace* Face = InIdentity.FindPartOfClass<UMetaHumanIdentityFace>();

	if (!Face)
	{
		return;
	}

	for (const UMetaHumanIdentityPose* Pose : Face->GetPoses())
	{
		if (Pose)
		{
			const UCaptureData* CaptureData = Pose->GetCaptureData();

			if (CaptureData && CaptureData->IsA<UFootageCaptureData>())
			{
				const UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(CaptureData);

				if (FootageCaptureData)
				{
					OutImageSequencePathChecker.Check(*FootageCaptureData);
				}
			}
		}
	}
}

FText UAssetDefinition_MetaHumanIdentity::GetAssetDisplayName() const
{
	return LOCTEXT("MetaHumanIdentityAssetName", "MetaHuman Identity");
}

FLinearColor UAssetDefinition_MetaHumanIdentity::GetAssetColor() const
{
	return FColor::Cyan;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanIdentity::GetAssetClass() const
{
	return UMetaHumanIdentity::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanIdentity::GetAssetCategories() const
{
	return FModuleManager::GetModuleChecked<IMetaHumanCoreEditorModule>(TEXT("MetaHumanCoreEditor")).GetMetaHumanAssetCategoryPath();
}

UThumbnailInfo* UAssetDefinition_MetaHumanIdentity::LoadThumbnailInfo(const FAssetData& InAssetData) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), UMetaHumanIdentityThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_MetaHumanIdentity::OpenAssets(const FAssetOpenArgs& InOpenArgs) const
{
	UE::CaptureData::FImageSequencePathChecker ImageSequencePathChecker(GetAssetDisplayName());

	for (UMetaHumanIdentity* Identity : InOpenArgs.LoadObjects<UMetaHumanIdentity>())
	{
		if (UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			FString RigCompatibilityMsg;
			bool bRigCompatible = Face->CheckRigCompatible(RigCompatibilityMsg);
			bool UpdatePromotedFrameVersion = false;
			bool bContoursCompatible = ContourDataIsCompatible(Face, UpdatePromotedFrameVersion);

			if (!bRigCompatible || !bContoursCompatible)
			{
				if (!bRigCompatible)
				{
					UE_LOG(LogMetaHumanIdentity, Warning, TEXT("Identity %s uses face skel mesh DNA that is incompatible with the Face archetype:\n%s"), *Identity->GetName(), *RigCompatibilityMsg);
				}

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("IdentityName"), FText::FromString(Identity->GetName()));
				Arguments.Add(TEXT("CompatibilityMessage"), GetCompatibilityMessage(bRigCompatible, bContoursCompatible));
				const FText MessageFormat = LOCTEXT("IdentityIncompatibleData", "MetaHuman Identity {IdentityName} is incompatible with this plugin version without the system making modifications."
					"\n\n{CompatibilityMessage}\n\nDo you wish to proceed ?");
				const FText MessageText = FText::Format(MessageFormat, Arguments);

				const FText TitleText = LOCTEXT("IdentityDataIncompatibilityMessageTitle", "Incompatible MetaHuman Identity");
				const EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::YesNo, MessageText, TitleText);

				if (DialogResult == EAppReturnType::Yes)
				{
					if (!bRigCompatible)
					{
						Face->ResetRigComponent();
						Identity->MarkPackageDirty();
					}
				}
				else
				{
					// Asset open canceled, try next identity (if any)
					continue;
				}
			}
			// For now we only update the version for promoted frames when there's a minor version change
			if (UpdatePromotedFrameVersion)
			{
				ResolveContourDataCompatibility(Face);
				Identity->MarkPackageDirty();
			}
		}

		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			UMetaHumanIdentityAssetEditor* IdentityAssetEditor = NewObject<UMetaHumanIdentityAssetEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
			IdentityAssetEditor->SetObjectToEdit(Identity);
			IdentityAssetEditor->Initialize();

			FString FunctionalityMessage;

			if (!FMetaHumanMinSpec::IsSupported())
			{
				FunctionalityMessage += FText::Format(LOCTEXT("MinSpecIdentityMessage", "Minimum specification for using an Identity is not met. Stability and performance maybe effected.\n\nMinimum specification is: {0}."), FMetaHumanMinSpec::GetMinSpec()).ToString();
			}

			if (!FMetaHumanSupportedRHI::IsSupported())
			{
				if (!FunctionalityMessage.IsEmpty())
				{
					FunctionalityMessage += TEXT("\n\n");
				}

				FunctionalityMessage += FText::Format(LOCTEXT("UnsupportedRHIIdentityMessage", "Tracking a promoted frames in an Identity will not be possible with the current RHI. To enable tracking promoted frames make sure the RHI is set to {0}."), FMetaHumanSupportedRHI::GetSupportedRHINames()).ToString();
			}

			if (!FunctionalityMessage.IsEmpty())
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FunctionalityMessage), LOCTEXT("MinSpecIdentityTitle", "Minimum specification"));
			}
		}

		UpdateImageSequencePathChecker(*Identity, ImageSequencePathChecker);
	}

	if (ImageSequencePathChecker.HasError())
	{
		ImageSequencePathChecker.DisplayDialog();
	}

	return EAssetCommandResult::Handled;
}

void UAssetDefinition_MetaHumanIdentity::ResolveContourDataCompatibility(const UMetaHumanIdentityFace* InFacePart) const
{
	const FString ContourDataString = FMetaHumanContourDataVersion::GetContourDataVersionString();
	if (const UMetaHumanIdentityPose* NeutralPose = InFacePart->FindPoseByType(EIdentityPoseType::Neutral))
	{
		for (const TObjectPtr<UMetaHumanIdentityPromotedFrame>& Frame : NeutralPose->PromotedFrames)
		{
			Frame->ContourData->ContourDataConfigVersion = ContourDataString;
		}
	}
	if (const UMetaHumanIdentityPose* TeethPose = InFacePart->FindPoseByType(EIdentityPoseType::Teeth))
	{
		for (const TObjectPtr<UMetaHumanIdentityPromotedFrame>& Frame : TeethPose->PromotedFrames)
		{
			Frame->ContourData->ContourDataConfigVersion = ContourDataString;
		}
	}
}

bool UAssetDefinition_MetaHumanIdentity::ContourDataIsCompatible(const class UMetaHumanIdentityFace* InFacePart, bool& bOutUpdateRequired) const
{
	bool Compatible = true;
	TArray<FString> PromotedFrameVersions;
	FMetaHumanContourDataVersion::ECompatibilityResult Compatibility = FMetaHumanContourDataVersion::ECompatibilityResult::NoUpgrade;

	for (EIdentityPoseType PoseType : TEnumRange<EIdentityPoseType>())
	{
		if (const UMetaHumanIdentityPose* Pose = InFacePart->FindPoseByType(PoseType))
		{
			if (!Pose->PromotedFrames.IsEmpty())
			{
				// Contour data is incompatible if any promoted frame has not been initialized
				Compatible &= Algo::AllOf(Pose->PromotedFrames, [](const TObjectPtr<UMetaHumanIdentityPromotedFrame> InPromotedFrame) {
					return InPromotedFrame->ContourData->FrameTrackingContourData.ContainsData();
				});

				if (Compatible)
				{
					for (const TObjectPtr<UMetaHumanIdentityPromotedFrame>& Frame : Pose->PromotedFrames)
					{
						PromotedFrameVersions.Add(Frame->ContourData->ContourDataConfigVersion);
					}
				}
				else
				{
					bOutUpdateRequired = false;
					return false;
				}
			}
		}
	}

	Compatible = FMetaHumanContourDataVersion::CheckVersionCompatibility(PromotedFrameVersions, Compatibility);
	bOutUpdateRequired = Compatibility == FMetaHumanContourDataVersion::ECompatibilityResult::AutoUpgrade;

	return Compatible;
}

FText UAssetDefinition_MetaHumanIdentity::GetCompatibilityMessage(bool bRigCompatible, bool bContoursCompatible) const
{
	FString CompatibilityMessage;

	FString RigIncompatibleMessage = "If you proceed the MetaHuman Identity will be reset to it's state before the \"MetaHuman Identity Solve\""
		"step was completed. The solve and subsequent steps will need to be completed again.\n"
		"This process will also cause a new Skeletal Mesh asset to be created and assigned to the Identity.";

	FString ContoursIncompatibleMessage = "Promoted frames contain invalid contour data. \nPlease make sure that the frames "
		"are re-promoted and re-tracked before running the \"MetaHuman Identity Solve\" step";

	if (!bRigCompatible)
	{
		CompatibilityMessage += RigIncompatibleMessage;
	}
	if (!bContoursCompatible)
	{
		CompatibilityMessage = CompatibilityMessage.IsEmpty() ? ContoursIncompatibleMessage : CompatibilityMessage + "\n\n" + ContoursIncompatibleMessage;
	}

	return FText::FromString(CompatibilityMessage);
}

#undef LOCTEXT_NAMESPACE
