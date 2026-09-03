// Copyright Epic Games, Inc. All Rights Reserved.

#include "Verification/VerifyMetaHumanCharacter.h"

#include "AnimGraphNode_RigLogic.h"
#include "AnimNode_RigLogic.h"
#include "MetaHumanAssetReport.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"

#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "K2Node_CallFunction.h"
#include "MetaHumanComponentUE.h"
#include "Animation/AnimBlueprint.h"
#include "Components/LODSyncComponent.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/RuntimeErrors.h"

#define LOCTEXT_NAMESPACE "VerifyMetaHumanCharacter"

namespace UE::MetaHuman::Private
{
const TSet<FName>& GetFdsCurves()
{
	static const TSet<FName> FdsCurves = {
		FName(TEXT("CTRL_expressions_browDownL")),
		FName(TEXT("CTRL_expressions_browDownR")),
		FName(TEXT("CTRL_expressions_browLateralL")),
		FName(TEXT("CTRL_expressions_browLateralR")),
		FName(TEXT("CTRL_expressions_browRaiseInL")),
		FName(TEXT("CTRL_expressions_browRaiseInR")),
		FName(TEXT("CTRL_expressions_browRaiseOuterL")),
		FName(TEXT("CTRL_expressions_browRaiseOuterR")),
		FName(TEXT("CTRL_expressions_earUpL")),
		FName(TEXT("CTRL_expressions_earUpR")),
		FName(TEXT("CTRL_expressions_eyeBlinkL")),
		FName(TEXT("CTRL_expressions_eyeBlinkR")),
		FName(TEXT("CTRL_expressions_eyeCheekRaiseL")),
		FName(TEXT("CTRL_expressions_eyeCheekRaiseR")),
		FName(TEXT("CTRL_expressions_eyeFaceScrunchL")),
		FName(TEXT("CTRL_expressions_eyeFaceScrunchR")),
		FName(TEXT("CTRL_expressions_eyeLidPressL")),
		FName(TEXT("CTRL_expressions_eyeLidPressR")),
		FName(TEXT("CTRL_expressions_eyeLookDownL")),
		FName(TEXT("CTRL_expressions_eyeLookDownR")),
		FName(TEXT("CTRL_expressions_eyeLookLeftL")),
		FName(TEXT("CTRL_expressions_eyeLookLeftR")),
		FName(TEXT("CTRL_expressions_eyeLookRightL")),
		FName(TEXT("CTRL_expressions_eyeLookRightR")),
		FName(TEXT("CTRL_expressions_eyeLookUpL")),
		FName(TEXT("CTRL_expressions_eyeLookUpR")),
		FName(TEXT("CTRL_expressions_eyeLowerLidDownL")),
		FName(TEXT("CTRL_expressions_eyeLowerLidDownR")),
		FName(TEXT("CTRL_expressions_eyeLowerLidUpL")),
		FName(TEXT("CTRL_expressions_eyeLowerLidUpR")),
		FName(TEXT("CTRL_expressions_eyeParallelLookDirection")),
		FName(TEXT("CTRL_expressions_eyePupilNarrowL")),
		FName(TEXT("CTRL_expressions_eyePupilNarrowR")),
		FName(TEXT("CTRL_expressions_eyePupilWideL")),
		FName(TEXT("CTRL_expressions_eyePupilWideR")),
		FName(TEXT("CTRL_expressions_eyeRelaxL")),
		FName(TEXT("CTRL_expressions_eyeRelaxR")),
		FName(TEXT("CTRL_expressions_eyeSquintInnerL")),
		FName(TEXT("CTRL_expressions_eyeSquintInnerR")),
		FName(TEXT("CTRL_expressions_eyeUpperLidUpL")),
		FName(TEXT("CTRL_expressions_eyeUpperLidUpR")),
		FName(TEXT("CTRL_expressions_eyeWidenL")),
		FName(TEXT("CTRL_expressions_eyeWidenR")),
		FName(TEXT("CTRL_expressions_eyelashesDownINL")),
		FName(TEXT("CTRL_expressions_eyelashesDownINR")),
		FName(TEXT("CTRL_expressions_eyelashesDownOUTL")),
		FName(TEXT("CTRL_expressions_eyelashesDownOUTR")),
		FName(TEXT("CTRL_expressions_eyelashesUpINL")),
		FName(TEXT("CTRL_expressions_eyelashesUpINR")),
		FName(TEXT("CTRL_expressions_eyelashesUpOUTL")),
		FName(TEXT("CTRL_expressions_eyelashesUpOUTR")),
		FName(TEXT("CTRL_expressions_jawBack")),
		FName(TEXT("CTRL_expressions_jawChinCompressL")),
		FName(TEXT("CTRL_expressions_jawChinCompressR")),
		FName(TEXT("CTRL_expressions_jawChinRaiseDL")),
		FName(TEXT("CTRL_expressions_jawChinRaiseDR")),
		FName(TEXT("CTRL_expressions_jawChinRaiseUL")),
		FName(TEXT("CTRL_expressions_jawChinRaiseUR")),
		FName(TEXT("CTRL_expressions_jawClenchL")),
		FName(TEXT("CTRL_expressions_jawClenchR")),
		FName(TEXT("CTRL_expressions_jawFwd")),
		FName(TEXT("CTRL_expressions_jawLeft")),
		FName(TEXT("CTRL_expressions_jawOpen")),
		FName(TEXT("CTRL_expressions_jawOpenExtreme")),
		FName(TEXT("CTRL_expressions_jawRight")),
		FName(TEXT("CTRL_expressions_mouthCheekBlowL")),
		FName(TEXT("CTRL_expressions_mouthCheekBlowR")),
		FName(TEXT("CTRL_expressions_mouthCheekSuckL")),
		FName(TEXT("CTRL_expressions_mouthCheekSuckR")),
		FName(TEXT("CTRL_expressions_mouthCornerDepressL")),
		FName(TEXT("CTRL_expressions_mouthCornerDepressR")),
		FName(TEXT("CTRL_expressions_mouthCornerDownL")),
		FName(TEXT("CTRL_expressions_mouthCornerDownR")),
		FName(TEXT("CTRL_expressions_mouthCornerNarrowL")),
		FName(TEXT("CTRL_expressions_mouthCornerNarrowR")),
		FName(TEXT("CTRL_expressions_mouthCornerPullL")),
		FName(TEXT("CTRL_expressions_mouthCornerPullR")),
		FName(TEXT("CTRL_expressions_mouthCornerRounderDL")),
		FName(TEXT("CTRL_expressions_mouthCornerRounderDR")),
		FName(TEXT("CTRL_expressions_mouthCornerRounderUL")),
		FName(TEXT("CTRL_expressions_mouthCornerRounderUR")),
		FName(TEXT("CTRL_expressions_mouthCornerSharpenDL")),
		FName(TEXT("CTRL_expressions_mouthCornerSharpenDR")),
		FName(TEXT("CTRL_expressions_mouthCornerSharpenUL")),
		FName(TEXT("CTRL_expressions_mouthCornerSharpenUR")),
		FName(TEXT("CTRL_expressions_mouthCornerUpL")),
		FName(TEXT("CTRL_expressions_mouthCornerUpR")),
		FName(TEXT("CTRL_expressions_mouthCornerWideL")),
		FName(TEXT("CTRL_expressions_mouthCornerWideR")),
		FName(TEXT("CTRL_expressions_mouthDimpleL")),
		FName(TEXT("CTRL_expressions_mouthDimpleR")),
		FName(TEXT("CTRL_expressions_mouthDown")),
		FName(TEXT("CTRL_expressions_mouthFunnelDL")),
		FName(TEXT("CTRL_expressions_mouthFunnelDR")),
		FName(TEXT("CTRL_expressions_mouthFunnelUL")),
		FName(TEXT("CTRL_expressions_mouthFunnelUR")),
		FName(TEXT("CTRL_expressions_mouthLeft")),
		FName(TEXT("CTRL_expressions_mouthLipsBlowL")),
		FName(TEXT("CTRL_expressions_mouthLipsBlowR")),
		FName(TEXT("CTRL_expressions_mouthLipsPressL")),
		FName(TEXT("CTRL_expressions_mouthLipsPressR")),
		FName(TEXT("CTRL_expressions_mouthLipsPullDL")),
		FName(TEXT("CTRL_expressions_mouthLipsPullDR")),
		FName(TEXT("CTRL_expressions_mouthLipsPullUL")),
		FName(TEXT("CTRL_expressions_mouthLipsPullUR")),
		FName(TEXT("CTRL_expressions_mouthLipsPurseDL")),
		FName(TEXT("CTRL_expressions_mouthLipsPurseDR")),
		FName(TEXT("CTRL_expressions_mouthLipsPurseUL")),
		FName(TEXT("CTRL_expressions_mouthLipsPurseUR")),
		FName(TEXT("CTRL_expressions_mouthLipsPushDL")),
		FName(TEXT("CTRL_expressions_mouthLipsPushDR")),
		FName(TEXT("CTRL_expressions_mouthLipsPushUL")),
		FName(TEXT("CTRL_expressions_mouthLipsPushUR")),
		FName(TEXT("CTRL_expressions_mouthLipsStickyLPh1")),
		FName(TEXT("CTRL_expressions_mouthLipsStickyLPh2")),
		FName(TEXT("CTRL_expressions_mouthLipsStickyLPh3")),
		FName(TEXT("CTRL_expressions_mouthLipsStickyRPh1")),
		FName(TEXT("CTRL_expressions_mouthLipsStickyRPh2")),
		FName(TEXT("CTRL_expressions_mouthLipsStickyRPh3")),
		FName(TEXT("CTRL_expressions_mouthLipsThickDL")),
		FName(TEXT("CTRL_expressions_mouthLipsThickDR")),
		FName(TEXT("CTRL_expressions_mouthLipsThickInwardDL")),
		FName(TEXT("CTRL_expressions_mouthLipsThickInwardDR")),
		FName(TEXT("CTRL_expressions_mouthLipsThickInwardUL")),
		FName(TEXT("CTRL_expressions_mouthLipsThickInwardUR")),
		FName(TEXT("CTRL_expressions_mouthLipsThickUL")),
		FName(TEXT("CTRL_expressions_mouthLipsThickUR")),
		FName(TEXT("CTRL_expressions_mouthLipsThinDL")),
		FName(TEXT("CTRL_expressions_mouthLipsThinDR")),
		FName(TEXT("CTRL_expressions_mouthLipsThinInwardDL")),
		FName(TEXT("CTRL_expressions_mouthLipsThinInwardDR")),
		FName(TEXT("CTRL_expressions_mouthLipsThinInwardUL")),
		FName(TEXT("CTRL_expressions_mouthLipsThinInwardUR")),
		FName(TEXT("CTRL_expressions_mouthLipsThinUL")),
		FName(TEXT("CTRL_expressions_mouthLipsThinUR")),
		FName(TEXT("CTRL_expressions_mouthLipsTightenDL")),
		FName(TEXT("CTRL_expressions_mouthLipsTightenDR")),
		FName(TEXT("CTRL_expressions_mouthLipsTightenUL")),
		FName(TEXT("CTRL_expressions_mouthLipsTightenUR")),
		FName(TEXT("CTRL_expressions_mouthLipsTogetherDL")),
		FName(TEXT("CTRL_expressions_mouthLipsTogetherDR")),
		FName(TEXT("CTRL_expressions_mouthLipsTogetherUL")),
		FName(TEXT("CTRL_expressions_mouthLipsTogetherUR")),
		FName(TEXT("CTRL_expressions_mouthLipsTowardsDL")),
		FName(TEXT("CTRL_expressions_mouthLipsTowardsDR")),
		FName(TEXT("CTRL_expressions_mouthLipsTowardsUL")),
		FName(TEXT("CTRL_expressions_mouthLipsTowardsUR")),
		FName(TEXT("CTRL_expressions_mouthLowerLipBiteL")),
		FName(TEXT("CTRL_expressions_mouthLowerLipBiteR")),
		FName(TEXT("CTRL_expressions_mouthLowerLipDepressL")),
		FName(TEXT("CTRL_expressions_mouthLowerLipDepressR")),
		FName(TEXT("CTRL_expressions_mouthLowerLipRollInL")),
		FName(TEXT("CTRL_expressions_mouthLowerLipRollInR")),
		FName(TEXT("CTRL_expressions_mouthLowerLipRollOutL")),
		FName(TEXT("CTRL_expressions_mouthLowerLipRollOutR")),
		FName(TEXT("CTRL_expressions_mouthLowerLipShiftLeft")),
		FName(TEXT("CTRL_expressions_mouthLowerLipShiftRight")),
		FName(TEXT("CTRL_expressions_mouthLowerLipTowardsTeethL")),
		FName(TEXT("CTRL_expressions_mouthLowerLipTowardsTeethR")),
		FName(TEXT("CTRL_expressions_mouthPressDL")),
		FName(TEXT("CTRL_expressions_mouthPressDR")),
		FName(TEXT("CTRL_expressions_mouthPressUL")),
		FName(TEXT("CTRL_expressions_mouthPressUR")),
		FName(TEXT("CTRL_expressions_mouthRight")),
		FName(TEXT("CTRL_expressions_mouthSharpCornerPullL")),
		FName(TEXT("CTRL_expressions_mouthSharpCornerPullR")),
		FName(TEXT("CTRL_expressions_mouthStickyDC")),
		FName(TEXT("CTRL_expressions_mouthStickyDINL")),
		FName(TEXT("CTRL_expressions_mouthStickyDINR")),
		FName(TEXT("CTRL_expressions_mouthStickyDOUTL")),
		FName(TEXT("CTRL_expressions_mouthStickyDOUTR")),
		FName(TEXT("CTRL_expressions_mouthStickyUC")),
		FName(TEXT("CTRL_expressions_mouthStickyUINL")),
		FName(TEXT("CTRL_expressions_mouthStickyUINR")),
		FName(TEXT("CTRL_expressions_mouthStickyUOUTL")),
		FName(TEXT("CTRL_expressions_mouthStickyUOUTR")),
		FName(TEXT("CTRL_expressions_mouthStretchL")),
		FName(TEXT("CTRL_expressions_mouthStretchLipsCloseL")),
		FName(TEXT("CTRL_expressions_mouthStretchLipsCloseR")),
		FName(TEXT("CTRL_expressions_mouthStretchR")),
		FName(TEXT("CTRL_expressions_mouthUp")),
		FName(TEXT("CTRL_expressions_mouthUpperLipBiteL")),
		FName(TEXT("CTRL_expressions_mouthUpperLipBiteR")),
		FName(TEXT("CTRL_expressions_mouthUpperLipRaiseL")),
		FName(TEXT("CTRL_expressions_mouthUpperLipRaiseR")),
		FName(TEXT("CTRL_expressions_mouthUpperLipRollInL")),
		FName(TEXT("CTRL_expressions_mouthUpperLipRollInR")),
		FName(TEXT("CTRL_expressions_mouthUpperLipRollOutL")),
		FName(TEXT("CTRL_expressions_mouthUpperLipRollOutR")),
		FName(TEXT("CTRL_expressions_mouthUpperLipShiftLeft")),
		FName(TEXT("CTRL_expressions_mouthUpperLipShiftRight")),
		FName(TEXT("CTRL_expressions_mouthUpperLipTowardsTeethL")),
		FName(TEXT("CTRL_expressions_mouthUpperLipTowardsTeethR")),
		FName(TEXT("CTRL_expressions_neckDigastricDown")),
		FName(TEXT("CTRL_expressions_neckDigastricUp")),
		FName(TEXT("CTRL_expressions_neckMastoidContractL")),
		FName(TEXT("CTRL_expressions_neckMastoidContractR")),
		FName(TEXT("CTRL_expressions_neckStretchL")),
		FName(TEXT("CTRL_expressions_neckStretchR")),
		FName(TEXT("CTRL_expressions_neckSwallowPh1")),
		FName(TEXT("CTRL_expressions_neckSwallowPh2")),
		FName(TEXT("CTRL_expressions_neckSwallowPh3")),
		FName(TEXT("CTRL_expressions_neckSwallowPh4")),
		FName(TEXT("CTRL_expressions_neckThroatDown")),
		FName(TEXT("CTRL_expressions_neckThroatExhale")),
		FName(TEXT("CTRL_expressions_neckThroatInhale")),
		FName(TEXT("CTRL_expressions_neckThroatUp")),
		FName(TEXT("CTRL_expressions_noseNasolabialDeepenL")),
		FName(TEXT("CTRL_expressions_noseNasolabialDeepenR")),
		FName(TEXT("CTRL_expressions_noseNostrilCompressL")),
		FName(TEXT("CTRL_expressions_noseNostrilCompressR")),
		FName(TEXT("CTRL_expressions_noseNostrilDepressL")),
		FName(TEXT("CTRL_expressions_noseNostrilDepressR")),
		FName(TEXT("CTRL_expressions_noseNostrilDilateL")),
		FName(TEXT("CTRL_expressions_noseNostrilDilateR")),
		FName(TEXT("CTRL_expressions_noseWrinkleL")),
		FName(TEXT("CTRL_expressions_noseWrinkleR")),
		FName(TEXT("CTRL_expressions_noseWrinkleUpperL")),
		FName(TEXT("CTRL_expressions_noseWrinkleUpperR")),
		FName(TEXT("CTRL_expressions_teethBackD")),
		FName(TEXT("CTRL_expressions_teethBackU")),
		FName(TEXT("CTRL_expressions_teethDownD")),
		FName(TEXT("CTRL_expressions_teethDownU")),
		FName(TEXT("CTRL_expressions_teethFwdD")),
		FName(TEXT("CTRL_expressions_teethFwdU")),
		FName(TEXT("CTRL_expressions_teethLeftD")),
		FName(TEXT("CTRL_expressions_teethLeftU")),
		FName(TEXT("CTRL_expressions_teethRightD")),
		FName(TEXT("CTRL_expressions_teethRightU")),
		FName(TEXT("CTRL_expressions_teethUpD")),
		FName(TEXT("CTRL_expressions_teethUpU")),
		FName(TEXT("CTRL_expressions_tongueBendDown")),
		FName(TEXT("CTRL_expressions_tongueBendUp")),
		FName(TEXT("CTRL_expressions_tongueDown")),
		FName(TEXT("CTRL_expressions_tongueIn")),
		FName(TEXT("CTRL_expressions_tongueLeft")),
		FName(TEXT("CTRL_expressions_tongueNarrow")),
		FName(TEXT("CTRL_expressions_tongueOut")),
		FName(TEXT("CTRL_expressions_tonguePress")),
		FName(TEXT("CTRL_expressions_tongueRight")),
		FName(TEXT("CTRL_expressions_tongueRoll")),
		FName(TEXT("CTRL_expressions_tongueThick")),
		FName(TEXT("CTRL_expressions_tongueThin")),
		FName(TEXT("CTRL_expressions_tongueTipDown")),
		FName(TEXT("CTRL_expressions_tongueTipLeft")),
		FName(TEXT("CTRL_expressions_tongueTipRight")),
		FName(TEXT("CTRL_expressions_tongueTipUp")),
		FName(TEXT("CTRL_expressions_tongueTwistLeft")),
		FName(TEXT("CTRL_expressions_tongueTwistRight")),
		FName(TEXT("CTRL_expressions_tongueUp")),
		FName(TEXT("CTRL_expressions_tongueWide"))
	};

	return FdsCurves;
}
}


void UVerifyMetaHumanCharacter::Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const
{
	using namespace UE::MetaHuman::Private;

	if (!ensureAsRuntimeWarning(ToVerify) || !ensureAsRuntimeWarning(Report))
	{
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), FText::FromString(ToVerify->GetName()));

	// 1001	MH Actor Blueprint	No MH Actor BP detected
	const UBlueprint* MainBP = Cast<UBlueprint>(ToVerify);
	if (!MainBP)
	{
		Report->AddError({FText::Format(LOCTEXT("MissingBlueprint", "The Asset {AssetName} is not a MetaHuman Blueprint"), Args), ToVerify});
		return;
	}

	USkeletalMeshComponent* BodyComponent = Cast<USkeletalMeshComponent>(MainBP->FindTemplateByName(TEXT("Body")));
	if (USCS_Node* Node = MainBP->SimpleConstructionScript->FindSCSNode("Body"))
	{
		BodyComponent = Cast<USkeletalMeshComponent>(Node->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(MainBP->GeneratedClass)));
	}
	if (BodyComponent != nullptr)
	{
		if (BodyComponent->GetPhysicsAsset() == nullptr)
		{
			// 1007	Body Physics Asset	Physics Asset	No Body physics asset detected
			Report->AddWarning({FText::Format(LOCTEXT("MissingBodyPhysics", "The Asset {AssetName} has no physics asset assigned for the Body component"), Args), ToVerify});
		}

		if (USkeletalMesh* BodySkelMesh = BodyComponent->GetSkeletalMeshAsset())
		{
			Args.Add(TEXT("BodySkelMeshName"), FText::FromString(BodySkelMesh->GetName()));
			if (USkeleton* BodySkeleton = BodySkelMesh->GetSkeleton())
			{
				Args.Add(TEXT("SkeletonName"), FText::FromString(BodySkeleton->GetName()));
				if (BodySkeleton->GetName() != "metahuman_base_skel")
				{
					// 1009	Body Skeleton Compatible	Asset exists	Can't find skeletal asset "metahuman_base_skel"
					Report->AddError({FText::Format(LOCTEXT("BadBodySkeletonName", "The Skeleton {SkeletonName} should be called \"metahuman_base_skel\""), Args), ToVerify});
				}

				// 1010	Body Skeleton Compatible	Bone Names	Bone names don't match original "metahuman_base_skel" skeleton
				if (!UMetaHumanAssetManager::IsMetaHumanBodyCompatibleSkeleton(BodySkeleton))
				{
					Report->AddWarning({FText::Format(LOCTEXT("BadBodySkeleton", "The Skeleton {SkeletonName} is not compatible with the MetaHuman Body skeleton"), Args), ToVerify});
				}
			}
			else
			{
				// 1009	Body Skeleton Compatible	Asset exists	Can't find skeletal asset "metahuman_base_skel"
				Report->AddError({FText::Format(LOCTEXT("MissingBodySkeleton", "The SkelMesh {BodySkelMeshName} does not have a skeleton assigned"), Args), ToVerify});
			}

			if (!BodySkelMesh->GetPostProcessAnimBlueprint())
			{
				// 1017	Body post process animBP	Asset exists	No body post process animBP
				Report->AddError({FText::Format(LOCTEXT("MissingBodyAnimBP", "The SkelMesh {BodySkelMeshName} does not have a post-process AnimBP and will not animate correctly"), Args), ToVerify});
			}

			// 1015	Body Control Rig	Asset exists	No body control rig detected Common/Common/MetaHuman_ControlRig.MetaHuman_ControlRig
			if (BodySkelMesh->GetDefaultAnimatingRig().IsNull() || !BodySkelMesh->GetDefaultAnimatingRig().GetLongPackageName().EndsWith(TEXT("MetaHuman_ControlRig")))
			{
				Report->AddWarning({FText::Format(LOCTEXT("MissingBodyControlRig", "The SkelMesh {BodySkelMeshName} does not have a control rig."), Args), BodySkelMesh});
			}
		}
		else
		{
			Report->AddError({FText::Format(LOCTEXT("MissingBodySkelMesh", "The Asset {AssetName} has no skeletal mesh assigned to the Body component"), Args), ToVerify});
		}
	}
	else
	{
		// 1003	MH Actor Blueprint	Skeletal Components Names	Skeletal Mesh Components don't contain the base name of "Body"
		Report->AddError({FText::Format(LOCTEXT("MissingBodyComponent", "The Asset {AssetName} has no skeletal mesh component named \"Body\""), Args), ToVerify});
	}

	USkeletalMeshComponent* FaceComponent = Cast<USkeletalMeshComponent>(MainBP->FindTemplateByName(TEXT("Face")));
	if (USCS_Node* Node = MainBP->SimpleConstructionScript->FindSCSNode("Face"))
	{
		FaceComponent = Cast<USkeletalMeshComponent>(Node->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(MainBP->GeneratedClass)));
	}
	if (FaceComponent != nullptr)
	{
		if (FaceComponent->GetPhysicsAsset() == nullptr)
		{
			// 1008	Face Physics Asset	Physics Asset	No Head physics asset detected
			Report->AddWarning({FText::Format(LOCTEXT("MissingFacePhysics", "The Asset {AssetName} has no physics asset assigned for the Face component"), Args), ToVerify});
		}

		if (USkeletalMesh* FaceSkelMesh = FaceComponent->GetSkeletalMeshAsset())
		{
			Args.Add(TEXT("FaceSkelMeshName"), FText::FromString(FaceSkelMesh->GetName()));
			if (USkeleton* FaceSkeleton = FaceSkelMesh->GetSkeleton())
			{
				Args.Add(TEXT("SkeletonName"), FText::FromString(FaceSkeleton->GetName()));
				if (FaceSkeleton->GetName() != "Face_Archetype_Skeleton")
				{
					// 1011	Face Skeleton Compatible	Asset exists	Can't find skeletal asset "Face_Archetype_Skeleton"
					Report->AddError({FText::Format(LOCTEXT("BadFaceSkeletonName", "The Skeleton {SkeletonName} should be called \"Face_Archetype_Skeleton\""), Args), FaceSkeleton});
				}

				// 1012	Face Skeleton Compatible	Bone Names	Bone names don't match original "Face_Archetype_Skeleton" skeleton
				if (!UMetaHumanAssetManager::IsMetaHumanFaceCompatibleSkeleton(FaceSkeleton))
				{
					Report->AddWarning({FText::Format(LOCTEXT("BadFaceSkeleton", "The Skeleton {SkeletonName} is not compatible with the MetaHuman Face skeleton"), Args), FaceSkeleton});
				}

				// 1029 Face skeleton conforms to MetaHuman Facial Description Standard
				TArray<FName> CurveNames;
				FaceSkeleton->GetCurveMetaDataNames(CurveNames);
				TSet<FName> MissingCurves = GetFdsCurves().Difference(TSet<FName>(CurveNames));
				if (!MissingCurves.IsEmpty())
				{
					TArray<FString> MissingCurveStrings;
					Algo::Transform(MissingCurves, MissingCurveStrings, [](const FName& Name) { return Name.ToString(); });
					Args.Add("MissingCurveNames", FText::FromString(FString::Join(MissingCurveStrings, TEXT(", "))));
					Report->AddError({FText::Format(LOCTEXT("BadFaceSkeletonCurves", "The animation curves on Skeleton {SkeletonName} are not compatible with the MetaHuman facial description standard. The character will not animate with MetaHuman Animator. Missing curves are: {MissingCurveNames}"), Args), FaceSkeleton});
				}
			}
			else
			{
				// 1011	Face Skeleton Compatible	Asset exists	Can't find skeletal asset "Face_Archetype_Skeleton"
				Report->AddError({FText::Format(LOCTEXT("MissingFaceSkeleton", "The SkelMesh {FaceSkelMeshName} does not have a skeleton assigned"), Args), FaceSkelMesh});
			}

			if (TSubclassOf<UAnimInstance> GeneratedAnimInstance = FaceSkelMesh->GetPostProcessAnimBlueprint())
			{
				if (UAnimBlueprint* FaceAnimBP = Cast<UAnimBlueprint>(GeneratedAnimInstance->ClassGeneratedBy))
				{
					bool bFoundRigLogic = false;
					while (!bFoundRigLogic && FaceAnimBP)
					{
						for (const UEdGraph* FunctionGraph : FaceAnimBP->FunctionGraphs)
						{
							if (FunctionGraph->GetName() == "AnimGraph")
							{
								for (const UEdGraphNode* Node : FunctionGraph->Nodes)
								{
									if (Cast<UAnimGraphNode_RigLogic>(Node))
									{
										bFoundRigLogic = true;
										break;
									}
								}
								break;
							}
						}
						FaceAnimBP = UAnimBlueprint::GetParentAnimBlueprint(FaceAnimBP);
					}
					if (!bFoundRigLogic)
					{
						// 1000	Rig Logic Node present	No Rig logig Node detected Common/Face/Face_PostProcess_AnimBP.Face_PostProcess_AnimBP
						Report->AddWarning({FText::Format(LOCTEXT("MissingRigLogicNode", "The SkelMesh {FaceSkelMeshName} does not use RigLogic in its post-process AnimBP and may not animate correctly"), Args), FaceSkelMesh});
					}
				}
			}
			else
			{
				// 1018	Face animBP	Asset exists	No face animBP, face wont work
				Report->AddError({FText::Format(LOCTEXT("MissingFaceAnimBP", "The SkelMesh {FaceSkelMeshName} does not have a post-process AnimBP and will not animate correctly"), Args), FaceSkelMesh});
			}

			// 1013	Control Rig Face Board	Asset exists	No Face board detected, animation will have to be baked to face bones Common/Face/Face_ControlBoard_CtrlRig.Face_ControlBoard_CtrlRig
			if (FaceSkelMesh->GetDefaultAnimatingRig().IsNull() || !FaceSkelMesh->GetDefaultAnimatingRig().GetLongPackageName().EndsWith(TEXT("Face_ControlBoard_CtrlRig")))
			{
				Report->AddWarning({FText::Format(LOCTEXT("MissingFaceBoard", "The SkelMesh {FaceSkelMeshName} does not have a Face board, animation will have to be baked to face bones."), Args), FaceSkelMesh});
			}
		}
		else
		{
			Report->AddError({FText::Format(LOCTEXT("MissingFaceSkelMesh", "The Asset {AssetName} has no skeletal mesh assigned to the Face component"), Args), ToVerify});
		}
	}
	else
	{
		// 1004	MH Actor Blueprint	Skeletal Components Names	Skeletal Mesh Components don't contain the base name of "Face"
		Report->AddError({FText::Format(LOCTEXT("MissingFaceComponent", "The Asset {AssetName} has no skeletal mesh component named \"Face\""), Args), ToVerify});
	}

	bool bCallsSetLeaderPose = false;
	bool bFoundLiveLinkSetup = false;
	const FName LeaderPoseFunctionName = GET_FUNCTION_NAME_CHECKED(USkeletalMeshComponent, SetLeaderPoseComponent);
	for (const TObjectPtr<UEdGraph>& Graph : MainBP->FunctionGraphs)
	{
		if (Graph.GetName() == TEXT("LiveLinkSetup"))
		{
			bFoundLiveLinkSetup = true;
		}
		for (const TObjectPtr<UEdGraphNode>& Node : Graph->Nodes)
		{
			if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (CallNode->GetFunctionName() == LeaderPoseFunctionName)
				{
					bCallsSetLeaderPose = true;
					break;
				}
			}
		}
		if (bCallsSetLeaderPose && bFoundLiveLinkSetup)
		{
			break;
		}
	}

	// 1005	MH Actor Blueprint	Construction Script	No leader pose construction script detected, body and head might not be bound together during animation
	if (!bCallsSetLeaderPose)
	{
		Report->AddWarning({FText::Format(LOCTEXT("NoLeaderPoseConstruction", "The Blueprint {AssetName} does not have a construction script that calls SetLeaderPoseComponent. The Face and Body components may not move together when animated."), Args), ToVerify});
	}

	// 1021	Live Link setup
	if (!bFoundLiveLinkSetup)
	{
		Report->AddWarning({FText::Format(LOCTEXT("NoLiveLinkSetup", "No Live Link Setup functionality detected in the Blueprint {AssetName}."), Args), ToVerify});
	}

	FString RootFolder = FPaths::GetPath(FPaths::GetPath(ToVerify->GetPathName()));

	// 1014	Head IK Control Rig - Common\Face\HeadMovementIK_Proc_CtrlRig.uasset
	TArray<FAssetData> HeadIkControlRigAssets;
	FString HeadIkControlRigPath = RootFolder / TEXT("Common") / TEXT("Face") / TEXT("HeadMovementIK_Proc_CtrlRig");
	IAssetRegistry::GetChecked().GetAssetsByPackageName(FName(HeadIkControlRigPath), HeadIkControlRigAssets);
	if (HeadIkControlRigAssets.IsEmpty())
	{
		HeadIkControlRigPath = RootFolder / TEXT("Common") / TEXT("Face") / TEXT("CR_MetaHuman_HeadMovement_IK_Proc");
		IAssetRegistry::GetChecked().GetAssetsByPackageName(FName(HeadIkControlRigPath), HeadIkControlRigAssets);
	}
	if (HeadIkControlRigAssets.IsEmpty())
	{
		Args.Add(TEXT("HeadIkControlRigPath"), FText::FromString(HeadIkControlRigPath));
		Report->AddWarning({FText::Format(LOCTEXT("NoProceduralHeadRig", "No procedural face control rig found. Expected to find \"{HeadIkControlRigPath}\"."), Args), ToVerify});
	}

	// 1019	LOD Sync component
	ULODSyncComponent* LodSyncComponent = Cast<ULODSyncComponent>(MainBP->FindTemplateByName(TEXT("LODSync")));
	if (USCS_Node* Node = MainBP->SimpleConstructionScript->FindSCSNode("LODSync"))
	{
		LodSyncComponent = Cast<ULODSyncComponent>(Node->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(MainBP->GeneratedClass)));
	}
	if (LodSyncComponent == nullptr)
	{
		Report->AddWarning({FText::Format(LOCTEXT("MissingLodSyncComponent", "The Asset {AssetName} has no Lod Sync Component named \"LodSync\""), Args), ToVerify});
	}

	// 1020	MH Component
	UMetaHumanComponentUE* MetaHumanComponent = Cast<UMetaHumanComponentUE>(MainBP->FindTemplateByName(TEXT("MetaHuman")));
	if (USCS_Node* Node = MainBP->SimpleConstructionScript->FindSCSNode("MetaHuman"))
	{
		MetaHumanComponent = Cast<UMetaHumanComponentUE>(Node->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(MainBP->GeneratedClass)));
	}
	if (MetaHumanComponent == nullptr)
	{
		Report->AddWarning({FText::Format(LOCTEXT("MetaHumanComponent", "The Asset {AssetName} has no MetaHuman Component named \"MetaHuman\""), Args), ToVerify});
	}
}

#undef LOCTEXT_NAMESPACE
