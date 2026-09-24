// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassZoneGraphNavigationFragments.h"
#include "MassZoneGraphNavigationTrait.generated.h"

#define UE_API MASSZONEGRAPHNAVIGATION_API


UCLASS(MinimalAPI, meta = (DisplayName = "ZoneGraph Navigation"))
class UMassZoneGraphNavigationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	UE_API virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(Category="Movement", EditAnywhere)
	FMassZoneGraphNavigationParameters NavigationParameters;
};

#undef UE_API
