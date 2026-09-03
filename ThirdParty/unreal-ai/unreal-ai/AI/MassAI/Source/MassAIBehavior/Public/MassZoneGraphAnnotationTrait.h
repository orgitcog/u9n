// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassZoneGraphAnnotationTrait.generated.h"

#define UE_API MASSAIBEHAVIOR_API

UCLASS(MinimalAPI, meta = (DisplayName = "ZoneGraph Annotation"))
class UMassZoneGraphAnnotationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	UE_API virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

#undef UE_API
