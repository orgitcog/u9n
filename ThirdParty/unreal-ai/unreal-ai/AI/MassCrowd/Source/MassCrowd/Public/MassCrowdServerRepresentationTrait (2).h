// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassRepresentationTypes.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassRepresentationFragments.h"
#include "GameFramework/Actor.h"
#include "MassCrowdServerRepresentationTrait.generated.h"


UCLASS(meta=(DisplayName="Crowd Server Representation"))
class MASSCROWD_API UMassCrowdServerRepresentationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

	UMassCrowdServerRepresentationTrait();

protected:

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	/** Actor class of this agent when spawned on server */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TSubclassOf<AActor> TemplateActor;

	/** Configuration parameters for the representation processor */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	FMassRepresentationParameters Params;
};
