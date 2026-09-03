// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ZoneGraphTypes.h"
#include "MassSettings.h"
#include "MassSmartObjectSettings.generated.h"

#define UE_API MASSSMARTOBJECTS_API

#if WITH_EDITOR
/** Called when annotation tag settings changed. */
DECLARE_MULTICAST_DELEGATE(FOnAnnotationSettingsChanged);
#endif

/**
 * Settings for the MassSmartObject module.
 */
UCLASS(MinimalAPI, config = Plugins, defaultconfig, DisplayName = "Mass SmartObject")
class UMassSmartObjectSettings : public UMassModuleSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	mutable FOnAnnotationSettingsChanged OnAnnotationSettingsChanged;
#endif

	/** Tag used to indicate that smart objects are associated to a lane for queries using lanes. */
	UPROPERTY(EditDefaultsOnly, Category = ZoneGraph, config)
	FZoneGraphTag SmartObjectTag;

	/** Extents used to find precomputed entry points to reach a smart object from a zone graph lane. */
	UPROPERTY(EditDefaultsOnly, Category = ZoneGraph, config)
	float SearchExtents = 500.f;

protected:

#if WITH_EDITOR
	UE_API virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
};

#undef UE_API
