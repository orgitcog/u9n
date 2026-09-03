// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityStateValidator.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanCurveDataController.h"
#include "MetaHumanFaceTrackerInterface.h"

#include "Containers/SortedMap.h"
#include "CaptureData.h"
#include "Misc/SecureHash.h"
#include "Features/IModularFeatures.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "MetaHumanIdentityStateValidator"

FMetaHumanIdentityStateValidator::FMetaHumanIdentityStateValidator()
{
	bPrepareForPerformanceEnabled = IModularFeatures::Get().IsModularFeatureAvailable(IPredictiveSolverInterface::GetModularFeatureName());
	
	SolveText = {1, LOCTEXT("SolveInvalidationTooltip","  - MetaHuman Identity Solve\n")};
	MeshToMetahumanText = {2, LOCTEXT("ARInvalidationTooltip","  - Mesh to MetaHuman\n")};
	FitTeethText = {3, LOCTEXT("TeethInvalidationTooltip","  - Fit Teeth\n")};
	if (bPrepareForPerformanceEnabled)
	{
		PrepareForPerformanceText = {4, LOCTEXT("PredictiveSolverInvalidationTooltip","  - Prepare for Performance\n")};
	}
}

void FMetaHumanIdentityStateValidator::UpdateIdentityProgress()
{
	UpdateCurrentProgressState();
	UpdateIdentityInvalidationState();
}

void FMetaHumanIdentityStateValidator::MeshConformedStateUpdate()
{
	UpdateCurrentProgressState();
	CalculateIdentityHashes();
	BindToContourDataChangeDelegates();

	if (CurrentProgress == EIdentityProgressState::AR)
	{
		Identity->InvalidationState = EIdentityInvalidationState::Valid;
	}
	else
	{
		Identity->InvalidationState = EIdentityInvalidationState::AR;
	}
}

void FMetaHumanIdentityStateValidator::MeshAutoriggedUpdate()
{
	UpdateCurrentProgressState();

	// As part of running Mesh To Metahuman, we're invalidating the predictive solver
	// Which means that current state here will always be requiring the user to prepare for performance	
	if (Identity->InvalidationState == EIdentityInvalidationState::AR)
	{
		Identity->InvalidationState = EIdentityInvalidationState::PrepareForPerformance;
	}
}

void FMetaHumanIdentityStateValidator::MeshPreparedForPerformanceUpdate() const
{
	if (Identity->InvalidationState == EIdentityInvalidationState::PrepareForPerformance)
	{
		Identity->InvalidationState = EIdentityInvalidationState::Valid;
	}
}

void FMetaHumanIdentityStateValidator::TeethFittedUpdate()
{
	CalculateIdentityHashes();
	BindToContourDataChangeDelegates();
	
	if (Identity->InvalidationState == EIdentityInvalidationState::FitTeeth)
	{
		Identity->InvalidationState = EIdentityInvalidationState::Valid;
	}
}

void FMetaHumanIdentityStateValidator::UpdateIdentityInvalidationState()
{
	if (IdentityHashes.SolveStateHash != GetSolverStateHash())
	{
		Identity->InvalidationState = EIdentityInvalidationState::Solve;
	}
	else if (IdentityHashes.TeethStateHash != GetTeethStateHash())
	{
		if (const UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (Face->FindPoseByType(EIdentityPoseType::Teeth))
			{
				Identity->InvalidationState = EIdentityInvalidationState::FitTeeth;
			}
		}
	}
}

void FMetaHumanIdentityStateValidator::UpdateCurrentProgressState()
{
	EIdentityProgressState State = EIdentityProgressState::Solve;

	if (const UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (Face->CanSubmitToAutorigging())
		{
			State = EIdentityProgressState::AR;
			if (Face->bIsAutoRigged)
			{
				State = EIdentityProgressState::PrepareForPerformance;
				if (Face->HasPredictiveSolvers())
				{
					State = EIdentityProgressState::Complete;
				}
			}
		}
	}

	CurrentProgress = State;
}

FText FMetaHumanIdentityStateValidator::GetInvalidationStateToolTip()
{
	TSortedMap<int32, FText> MessageContainer;
	bool bRefitTeeth = true;
	FText InvalidatedText = LOCTEXT("IdentityInvalidatedTooltip", "This identity is either unfinished or has been edited. The following steps need to be (re)executed:\n\n");

	UpdateIdentityProgress();

	if (CurrentProgress == EIdentityProgressState::Solve || Identity->InvalidationState == EIdentityInvalidationState::Solve)
	{
		MessageContainer.Add(SolveText.Key, SolveText.Value);
		MessageContainer.Add(MeshToMetahumanText.Key, MeshToMetahumanText.Value);
		MessageContainer.Add(PrepareForPerformanceText.Key, PrepareForPerformanceText.Value);
	}
	else if (CurrentProgress == EIdentityProgressState::AR || Identity->InvalidationState == EIdentityInvalidationState::AR)
	{
		MessageContainer.Add(MeshToMetahumanText.Key, MeshToMetahumanText.Value);
		MessageContainer.Add(PrepareForPerformanceText.Key, PrepareForPerformanceText.Value);
	}
	else if (CurrentProgress == EIdentityProgressState::PrepareForPerformance || Identity->InvalidationState == EIdentityInvalidationState::PrepareForPerformance)
	{
		MessageContainer.Add(PrepareForPerformanceText.Key, PrepareForPerformanceText.Value);
	}

	if (const UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (const UMetaHumanIdentityPose* TeethPose = Face->FindPoseByType(EIdentityPoseType::Teeth))
		{
			// Only display warning message if teeth fitting is not done with Capture Data of type Footage
			if (TeethPose->IsCaptureDataValid() && TeethPose->GetCaptureData()->IsA<UFootageCaptureData>())
			{
				if (!TeethPose->GetValidContourDataFramesFrontFirst().IsEmpty())
				{
					/**
					 * TODO: This check is not strictly true as you could have added the front frame and tracked teeth, 
					 * but then didn't click on either fit teeth or Mesh To Metahuman. We will have to revisit
					 * this when a way of checking the teeth fitting state is added. For now this combined with
					 * Invalidation State is sufficinet for state invalidation widget to behave as expected
					 * */
					bRefitTeeth = false;
				}
			}
		}
		else if (const UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
		{
			// No need to display fit teeth warning for mesh input if teeth pose is not explicitly added
			if (NeutralPose->IsCaptureDataValid() && NeutralPose->GetCaptureData()->IsA<UMeshCaptureData>())
			{
				bRefitTeeth = false;
			}
		}
	}

	if (bRefitTeeth || Identity->InvalidationState == EIdentityInvalidationState::FitTeeth)
	{
		MessageContainer.Add(FitTeethText.Key, FitTeethText.Value);
	}
	
	for (const TPair<int32, FText>& Message : MessageContainer)
	{
		InvalidatedText = FText::Format(LOCTEXT("InvalidatedIdentitySolveTooltip", "{0}  {1}"), InvalidatedText, Message.Value);
	}

	return MessageContainer.IsEmpty() ? FText() : InvalidatedText;
}

void FMetaHumanIdentityStateValidator::CalculateIdentityHashes()
{
	IdentityHashes.SolveStateHash = GetSolverStateHash();
	IdentityHashes.TeethStateHash = GetTeethStateHash();
}

FSHAHash FMetaHumanIdentityStateValidator::GetSolverStateHash() const
{
	FString SolverStateString = "";
	if (const UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (const UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
		{
			const FString EyesSetString = NeutralPose->bFitEyes ? TEXT("Yes") : TEXT("No");
			SolverStateString += TEXT("Data Driven Eyes Set: " + EyesSetString);
			SolverStateString += TEXT("Promoted frame number: " + FString::FromInt(NeutralPose->PromotedFrames.Num()));
			for (const TObjectPtr<UMetaHumanIdentityPromotedFrame>& Frame : NeutralPose->PromotedFrames)
			{
				SolverStateString += Frame->FrameName.ToString();
			}
		}
	}

	return GetHashForString(SolverStateString);
}

FSHAHash FMetaHumanIdentityStateValidator::GetTeethStateHash() const
{
	FString TeethStateString = "";
	if (const UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (const UMetaHumanIdentityPose* TeethPose = Face->FindPoseByType(EIdentityPoseType::Teeth))
		{
			TeethStateString += TEXT("Manual Teeth Offset: " + FString::SanitizeFloat(TeethPose->ManualTeethDepthOffset));			
		}
	}
	
	return GetHashForString(TeethStateString);
}

FSHAHash FMetaHumanIdentityStateValidator::GetHashForString(const FString& InStringToHash) const
{
	FSHA1 Sha1;
	Sha1.UpdateWithString(*InStringToHash, InStringToHash.Len());
	return Sha1.Finalize();
}

void FMetaHumanIdentityStateValidator::BindToContourDataChangeDelegates()
{
	if (const UMetaHumanIdentityFace* Face = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (const UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
		{
			for (const TObjectPtr<UMetaHumanIdentityPromotedFrame>& Frame : NeutralPose->PromotedFrames)
			{
				Frame->CurveDataController->TriggerContourUpdate().RemoveAll(this);
				Frame->CurveDataController->TriggerContourUpdate().AddSP(this, &FMetaHumanIdentityStateValidator::InvalidateIdentityWhenContoursChange);
			}
		}
		if (const UMetaHumanIdentityPose* TeethPose = Face->FindPoseByType(EIdentityPoseType::Teeth))
		{
			for (const TObjectPtr<UMetaHumanIdentityPromotedFrame>& Frame : TeethPose->PromotedFrames)
			{
				Frame->CurveDataController->TriggerContourUpdate().RemoveAll(this);
				Frame->CurveDataController->TriggerContourUpdate().AddSP(this, &FMetaHumanIdentityStateValidator::InvalidateTeethWhenContoursChange);
			}
		}
	}
}

void FMetaHumanIdentityStateValidator::InvalidateIdentityWhenContoursChange()
{
#if WITH_EDITOR
	FScopedTransaction StateModifiedTransaction(LOCTEXT("NeutralPoseInvalidation", "Invalidate Identity State"));
	Identity->Modify();
#endif
	
	Identity->InvalidationState = EIdentityInvalidationState::Solve;
}

void FMetaHumanIdentityStateValidator::InvalidateTeethWhenContoursChange()
{
	// If teeth data is present, Process of running Mesh To Metahuman will also run teeth fitting, so no need to change invalidation state
	if (Identity->InvalidationState != EIdentityInvalidationState::Solve && Identity->InvalidationState != EIdentityInvalidationState::AR)
	{
#if WITH_EDITOR
		FScopedTransaction StateModifiedTransaction(LOCTEXT("TeethPoseInvalidation", "Invalidate Identity State"));
		Identity->Modify();
#endif
		Identity->InvalidationState = EIdentityInvalidationState::FitTeeth;
	}
}

void FMetaHumanIdentityStateValidator::PostAssetLoadHashInitialization(TWeakObjectPtr<UMetaHumanIdentity> InIdentity)
{
	Identity = InIdentity;

	BindToContourDataChangeDelegates();
	CalculateIdentityHashes();
}

#undef LOCTEXT_NAMESPACE
