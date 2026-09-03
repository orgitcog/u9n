// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "MetaHumanCloudServicesSettings.generated.h"

UENUM()
enum class EMetaHumanCloudServicesDdcPolicy
{
	NoCache,
	UseLocalDdc,
	UseSharedDdc,
	UseAnyDdc,	
};

UENUM()
enum class EMetaHumanCloudServiceEnvironment
{
	Production,
	GameDev
};

USTRUCT()
struct FEosConstantsPlatform
{
	GENERATED_BODY()

	/** The product id for the running application, found on the dev portal */
	UPROPERTY()
	FString ProductId;

	/** The sandbox id for the running application, found on the dev portal */
	UPROPERTY()
	FString SandboxId;

	/** The deployment id for the running application, found on the dev portal */
	UPROPERTY()
	FString DeploymentId;

	/** Client id of the service permissions entry, found on the dev portal */
	UPROPERTY()
	FString ClientCredentialsId;

	/** Client secret for accessing the set of permissions, found on the dev portal */
	UPROPERTY()
	FString ClientCredentialsSecret;
};

/**
 * NOTE: Settings are in MetaHumanCharacter/Config/BaseMetaHumanCharacter.ini
 */
UCLASS(Config = MetaHumanSDK)
class UMetaHumanCloudServicesSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config)
	FString TextureSynthesisServiceUrl;
	UPROPERTY(Config)
	FString AutorigServiceUrl;
	UPROPERTY(Config)
	float Timeout;
	UPROPERTY(Config)
	float LongPollTimeout;	
	UPROPERTY(Config)
	float AuthTimeout;
	UPROPERTY(Config)
	float AuthPollInterval;
	UPROPERTY(Config)
	int32 RetryCount;
	UPROPERTY(Config)
	EMetaHumanCloudServicesDdcPolicy DdcPolicy;
	UPROPERTY(Config)
	EMetaHumanCloudServiceEnvironment ServiceEnvironment = EMetaHumanCloudServiceEnvironment::Production;
	UPROPERTY(Config)
	FEosConstantsPlatform ProdEosConstants;
	UPROPERTY(Config)
	FEosConstantsPlatform GameDevEosConstants;
};
