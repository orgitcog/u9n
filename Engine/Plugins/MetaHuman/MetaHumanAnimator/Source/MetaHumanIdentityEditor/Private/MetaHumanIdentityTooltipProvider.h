// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Internationalization/Text.h"

enum class ETrackPromotedFrameTooltipState
{
	Default, AddFacePart, AddPose, AddPromotedFrame, SelectPose, SelectFrame, SetTracker, MissingAuthoringObjects, UnsupportedRHI
};

struct FMetaHumanIdentityTooltipProvider
{
	static FText GetTrackActiveFrameButtonTooltip(const TWeakObjectPtr<class UMetaHumanIdentity> InIdentity,
		const TWeakObjectPtr<class UMetaHumanIdentityPose> SelectedIdentityPose,
		const class UMetaHumanIdentityPromotedFrame* InSelectedFrame);

	static FText GetIdentitySolveButtonTooltip(const TWeakObjectPtr<class UMetaHumanIdentity> InIdentity);
	static FText GetMeshToMetaHumanButtonTooltip(const TWeakObjectPtr<class UMetaHumanIdentity> InIdentity);
	static FText GetFitTeethButtonTooltip(const TWeakObjectPtr<class UMetaHumanIdentity> InIdentity, bool bInCanFitTeeth);
};
