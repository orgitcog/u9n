// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityTooltipProvider.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanSupportedRHI.h"
#include "UI/SMetaHumanIdentityPromotedFramesEditor.h"

#define LOCTEXT_NAMESPACE "MetaHumanIdentityTooltipProvider"

FText FMetaHumanIdentityTooltipProvider::GetTrackActiveFrameButtonTooltip(const TWeakObjectPtr<UMetaHumanIdentity> InIdentity,
	const TWeakObjectPtr<UMetaHumanIdentityPose> InSelectedIdentityPose,
	const UMetaHumanIdentityPromotedFrame* InSelectedFrame)
{
	FText TrackMarkersTooltipText = LOCTEXT("TrackActiveToolbarButtonTooltip", "Track Markers for currently active frame");
	ETrackPromotedFrameTooltipState CurrentTooltip = ETrackPromotedFrameTooltipState::Default;

	if (UMetaHumanIdentityFace* Face = InIdentity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (Face->GetPoses().IsEmpty())
		{
			CurrentTooltip = ETrackPromotedFrameTooltipState::AddPose;
		}
	}
	else
	{
		CurrentTooltip = ETrackPromotedFrameTooltipState::AddFacePart;
	}

	if (CurrentTooltip == ETrackPromotedFrameTooltipState::Default)
	{
		if (InSelectedIdentityPose == nullptr)
		{
			CurrentTooltip = ETrackPromotedFrameTooltipState::SelectPose;
		}
		else if (InSelectedIdentityPose->PromotedFrames.IsEmpty())
		{
			CurrentTooltip = ETrackPromotedFrameTooltipState::AddPromotedFrame;
		}
		else if (InSelectedFrame == nullptr)
		{
			CurrentTooltip = ETrackPromotedFrameTooltipState::SelectFrame;
		}
		else if (InSelectedFrame->ContourTracker == nullptr)
		{
			CurrentTooltip = ETrackPromotedFrameTooltipState::SetTracker;
		}
		else if (!InIdentity->GetMetaHumanAuthoringObjectsPresent())
		{
			CurrentTooltip = ETrackPromotedFrameTooltipState::MissingAuthoringObjects;
		}
		else if (!FMetaHumanSupportedRHI::IsSupported())
		{
			CurrentTooltip = ETrackPromotedFrameTooltipState::UnsupportedRHI;
		}
	}

	switch (ETrackPromotedFrameTooltipState(CurrentTooltip))
	{
	case ETrackPromotedFrameTooltipState::AddFacePart :
		TrackMarkersTooltipText = FText::Format(LOCTEXT("TrackActiveToolbarButtonNoFaceTooltip", "{0}\nTo enable this option, "
			"first add Face Part to the MetaHuman Identity by using\n+Add button in the MetaHuman Identity Parts Tree View, or Create Components button on the Toolbar"), TrackMarkersTooltipText);
		break;

	case ETrackPromotedFrameTooltipState::AddPose :
		TrackMarkersTooltipText = FText::Format(LOCTEXT("TrackActiveToolbarButtonNoPoseTooltip", "{0}\nTo enable this option, add a Pose to the Face Part of MetaHuman Identity\n"
			"by using the Pose sub - menu under + Add button in the MetaHuman Identity Parts Tree View."), TrackMarkersTooltipText);
		break;

	case ETrackPromotedFrameTooltipState::AddPromotedFrame :
		TrackMarkersTooltipText = FText::Format(LOCTEXT("TrackActiveToolbarButtonPoseExistsTooltip", "{0}\nTo enable this option, first promote a frame by using\n"
			"Promote Frame button on the Toolbar."), TrackMarkersTooltipText);
		break;

	case ETrackPromotedFrameTooltipState::SelectPose :
		TrackMarkersTooltipText = FText::Format(LOCTEXT("TrackActiveToolbarButtonPoseNotSelectedTooltip", "{0}\nTo enable this option, select a Pose in MetaHuman Identity Parts Tree View"), TrackMarkersTooltipText);
		break;

	case ETrackPromotedFrameTooltipState::SelectFrame :
		TrackMarkersTooltipText = FText::Format(LOCTEXT("TrackActiveToolbarButtonNoPromotedFrameSelectedTooltip", "{0}\nTo enable this option, select a frame on the Promoted Frames Timeline"), TrackMarkersTooltipText);
		break;

	case ETrackPromotedFrameTooltipState::SetTracker :
		TrackMarkersTooltipText = FText::Format(LOCTEXT("TrackActiveToolbarButtonNoDefaultTrackerTooltip", "{0}\nTo enable this option, promoted frame has to be created with a default tracker selected for a pose."), TrackMarkersTooltipText);
		break;

	case ETrackPromotedFrameTooltipState::MissingAuthoringObjects:
		TrackMarkersTooltipText = FText::Format(LOCTEXT("TrackActiveToolbarButtonMissingAuthoringObjectsTooltip", "{0}\nTo enable this option, ensure authoring objects are present."), TrackMarkersTooltipText);
		break;

	case ETrackPromotedFrameTooltipState::UnsupportedRHI:
		TrackMarkersTooltipText = FText::Format(LOCTEXT("TrackActiveToolbarButtonUnsupportedRHITooltip", "{0}\nTo enable this option, ensure RHI is set to {1}."), TrackMarkersTooltipText, FMetaHumanSupportedRHI::GetSupportedRHINames());
		break;

	default:
		break;
	}

	return TrackMarkersTooltipText;
}

FText FMetaHumanIdentityTooltipProvider::GetIdentitySolveButtonTooltip(const TWeakObjectPtr<class UMetaHumanIdentity> InIdentity)
{
	FText IdentitySolveDefaultTooltipText = LOCTEXT("IdentitySolveToolbarButtonTooltip", "Conforms the Template Mesh to Markers obtained by Track Markers\ncommand, so it can be sent to MetaHuman Service for auto-rigging.");
	if (UMetaHumanIdentityFace* Face = InIdentity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
		{
			if (NeutralPose->PromotedFrames.Num() > 0)
			{
				bool AnyFramesTracked = false;
				for (TObjectPtr<UMetaHumanIdentityPromotedFrame> PromotedFrame : NeutralPose->PromotedFrames)
				{
					AnyFramesTracked = AnyFramesTracked || PromotedFrame->GetFrameTrackingContourData()->ContainsData();
				}
				if (AnyFramesTracked)
				{
					if (NeutralPose->GetFrontalViewPromotedFrame())
					{
						return IdentitySolveDefaultTooltipText;
					}
					else
					{
						return FText::Format(LOCTEXT("IdentitySolveToolbarButtonNoFrontFrameTooltip", "{0}\n\nTo enable this option, set Front View using the right-click context menu on any Promoted Frame"), IdentitySolveDefaultTooltipText);
					}
				}
				else
				{
					return FText::Format(LOCTEXT("IdentitySolveToolbarButtonTrackOneFrameTooltip", "{0}\n\nTo enable this option, first Track at least one Promoted Frame"), IdentitySolveDefaultTooltipText);
				}
			}
			else
			{
				return FText::Format(LOCTEXT("IdentitySolveToolbarButtonNoPromotedFramesTooltip", "{0}\n\nTo enable this option, add at least one Promoted Frame\nby using Promote Frame button"), IdentitySolveDefaultTooltipText);
			}
		}
		else
		{
			return FText::Format(LOCTEXT("IdentitySolveToolbarButtonNoNeutralTooltip", "{0}\n\nTo enable this option, first add Neutral Pose to Face Part of the MetaHuman Identity\nby using a Pose sub-menu under Add (+) button on the Toolbar"), IdentitySolveDefaultTooltipText);
		}
	}
	else
	{
		return FText::Format(LOCTEXT("IdentitySolveToolbarButtonNoFaceTooltip", "{0}\n\nTo enable this option, first add Face Part to the MetaHuman Identity treeview\nby using Add(+) or Create Components button on the Toolbar"), IdentitySolveDefaultTooltipText);
	}
}

FText FMetaHumanIdentityTooltipProvider::GetMeshToMetaHumanButtonTooltip(const TWeakObjectPtr<class UMetaHumanIdentity> InIdentity)
{
	FText MeshToMetaHumanCaptionTooltipText = LOCTEXT("MeshToMetaHumanToolbarButtonTooltip", "Send the Template Mesh to MetaHuman Service for auto-rigging\n\nWhen it finishes, the Service will return a Skeletal Mesh asset with\nthe likeness of the given MetaHuman Identity into your Content Browser");
	if (UMetaHumanIdentityFace* Face = InIdentity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (Face->CanSubmitToAutorigging())
		{
			return MeshToMetaHumanCaptionTooltipText;
		}
		else
		{
			return FText::Format(LOCTEXT("MeshToMetaHumanToolbarButtonNotConformedTooltip", "{0}\n\nTo enable this option, the Template Mesh needs to be conformed\nto the given MetaHuman Identity by using MetaHuman Identity Solve command,\nand the Neutral Pose must have a valid Capture Data set."), MeshToMetaHumanCaptionTooltipText);
		}
	}
	else
	{
		return FText::Format(LOCTEXT("MeshToMetaHumanToolbarButtonNoFaceTooltip", "{0}\n\nTo enable this option, first add Face Part to the MetaHuman Identity treeview\nby using Add(+) or Create Components button on the Toolbar"), MeshToMetaHumanCaptionTooltipText);
	}
}

FText FMetaHumanIdentityTooltipProvider::GetFitTeethButtonTooltip(const TWeakObjectPtr<class UMetaHumanIdentity> InIdentity, bool bInCanFitTeeth)
{
	FText FitTeethTooltipText = LOCTEXT("FitTeethToolbarButtonTooltip", "Adjust the teeth of the Skeletal Mesh to fit the teeth in Teeth Pose");
	if (!bInCanFitTeeth)
	{
		FitTeethTooltipText = FText::Format(LOCTEXT("FitTeethToolbarButtonCannotFitTeethTooltip", "{0}\n\nThis command requires a Skeletal Mesh with an embedded MetaHuman DNA,\nwhich can be obtained from MetaHuman Service by using Mesh To MetaHuman command"), FitTeethTooltipText);
	}
	if (UMetaHumanIdentityFace* Face = InIdentity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (UMetaHumanIdentityPose* TeethPose = Face->FindPoseByType(EIdentityPoseType::Teeth))
		{
			if (TeethPose->GetCaptureData() != nullptr)
			{
				bool AnyFramesTracked = false;
				for (TObjectPtr<UMetaHumanIdentityPromotedFrame> PromotedFrame : TeethPose->PromotedFrames)
				{
					if (PromotedFrame->GetFrameTrackingContourData()->ContainsData())
					{
						AnyFramesTracked = true;
						break;
					}
				}
				if (AnyFramesTracked)
				{
					//CanFitTeeth already assured - if cannot not fit yet, the text will have a note about MeshToMetaHuman added
					return FitTeethTooltipText;
				}
				else
				{
					return FText::Format(LOCTEXT("FitTeethToolbarButtonNoTrackedFramesTooltip", "{0}\n\nTo enable this option, first Track at least one Promoted Frame for Teeth Pose"), FitTeethTooltipText);
				}
			}
			else
			{
				return FText::Format(LOCTEXT("FitTeethToolbarButtonNoCaptureDataTooltip", "{0}\n\nTo enable this option, set Capture Data in the Details panel of Teeth Pose"), FitTeethTooltipText);
			}
		}
		else
		{
			return FText::Format(LOCTEXT("FitTeethToolbarButtonNoTeethPoseTooltip", "{0}\n\nTo enable this option, add Teeth Pose to Face Part of MetaHuman Identity by using\n+Add->Add Pose->Add Teeth in the MetaHuman Identity Parts Tree View"), FitTeethTooltipText);
		}
	}
	else
	{
		return FText::Format(LOCTEXT("FitTeethToolbarButtonNoFaceTooltip", "{0}\n\nTo enable this option, first add Face Part to MetaHuman Identity by using\n+Add->Add Part->Add Face in MetaHuman Identity Parts Tree View, or\nCreate Components button on the Toolbar"), FitTeethTooltipText);
	}
}

#undef LOCTEXT_NAMESPACE
