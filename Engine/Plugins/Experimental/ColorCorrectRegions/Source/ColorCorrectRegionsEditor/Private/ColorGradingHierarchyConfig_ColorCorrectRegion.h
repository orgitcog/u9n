// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ColorGradingMixerObjectFilterRegistry.h"

/** Color Grading hierarchy config for the AColorCorrectionRegion actor class */
struct FColorGradingHierarchyConfig_ColorCorrectRegion : IColorGradingMixerObjectHierarchyConfig
{
public:
	static TSharedRef<IColorGradingMixerObjectHierarchyConfig> MakeInstance();

	//~ IColorGradingMixerObjectHierarchyConfig interface
	virtual TArray<AActor*> FindAssociatedActors(UObject* ParentObject) const override;
	virtual bool IsActorAssociated(UObject* ParentObject, AActor* AssociatedActor) const override;
	virtual bool HasCustomDropHandling() const override;
	virtual FSceneOutlinerDragValidationInfo ValidateDrop(UObject* DropTarget, const FSceneOutlinerDragDropPayload& Payload) const override;
	virtual bool OnDrop(UObject* DropTarget, const FSceneOutlinerDragDropPayload& Payload, const FSceneOutlinerDragValidationInfo& ValidationInfo) const override;
	virtual TSet<FName> GetPropertiesThatRequireListRefresh() const override;
	//~ End IColorGradingMixerObjectHierarchyConfig interface
};