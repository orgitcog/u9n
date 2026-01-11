// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "SmartObjectRequestTypes.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "SmartObjectSubsystem.h"
#endif
#include "EnvQueryGenerator_SmartObjects.generated.h"


/** Fetches Smart Object slots within QueryBoxExtent from locations given by QueryOriginContext, that match SmartObjectRequestFilter */
UCLASS(MinimalAPI, meta = (DisplayName = "Smart Objects"))
class UEnvQueryGenerator_SmartObjects : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	SMARTOBJECTSMODULE_API UEnvQueryGenerator_SmartObjects();

protected:
	SMARTOBJECTSMODULE_API virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	SMARTOBJECTSMODULE_API virtual FText GetDescriptionTitle() const override;
	SMARTOBJECTSMODULE_API virtual FText GetDescriptionDetails() const override;

	/** The context indicating the locations to be used as query origins */
	UPROPERTY(EditAnywhere, Category=Generator)
	TSubclassOf<UEnvQueryContext> QueryOriginContext;

	/** If set will be used to filter gathered results */
	UPROPERTY(EditAnywhere, Category=Generator)
	FSmartObjectRequestFilter SmartObjectRequestFilter;

	/** Combined with generator's origin(s) (as indicated by QueryOriginContext) determines query's volume */
	UPROPERTY(EditAnywhere, Category = Generator)
	FVector QueryBoxExtent;

	/** Determines whether only currently claimable slots are allowed */
	UPROPERTY(EditAnywhere, Category = Generator)
	bool bOnlyClaimable = true;
};
