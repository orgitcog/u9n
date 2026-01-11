// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassVisualizationTrait.h"
#include "MassStationaryVisualizationTrait.generated.h"

#define UE_API MASSREPRESENTATION_API


UCLASS(MinimalAPI)
class UMassStationaryVisualizationTrait : public UMassVisualizationTrait
{
	GENERATED_BODY()
public:
	UE_API UMassStationaryVisualizationTrait(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UE_API virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

protected:
#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};

#undef UE_API
