// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ProjectPackagingSettings.h"
#include "PlatformsMenuSettings.generated.h"

#define UE_API DEVELOPERTOOLSETTINGS_API



UCLASS(MinimalAPI, config=Game)
class UPlatformsMenuSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()
	
	/** The directory to which the packaged project will be copied. */
	UPROPERTY(config, EditAnywhere, Category=Project)
	FDirectoryPath StagingDirectory;

	/** Name of the target to use for LaunchOn (only Game/Client targets) */
	UPROPERTY(config)
	FString LaunchOnTarget;

	/** Gets the current launch on target, checking that it's valid, and the default build target if it is not */
	UE_API const FTargetInfo* GetLaunchOnTargetInfo() const;
	
	/**
	 * Get and set the per-platform build config and targetplatform settings for the Turnkey/Launch on menu
	 */
	UE_API EProjectPackagingBuildConfigurations GetBuildConfigurationForPlatform(FName PlatformName) const;
	UE_API void SetBuildConfigurationForPlatform(FName PlatformName, EProjectPackagingBuildConfigurations Configuration);

	UE_API FName GetTargetFlavorForPlatform(FName PlatformName) const;
	UE_API void SetTargetFlavorForPlatform(FName PlatformName, FName TargetFlavorName);

	UE_API FString GetArchitectureForPlatform(FName PlatformName) const;
	UE_API void SetArchitectureForPlatform(FName PlatformName, FString ArchitectureName);

	UE_API FString GetBuildTargetForPlatform(FName PlatformName) const;
	UE_API void SetBuildTargetForPlatform(FName PlatformName, FString BuildTargetName);

	UE_API const FTargetInfo* GetBuildTargetInfoForPlatform(FName PlatformName, bool& bOutIsProjectTarget) const;

private:
	/** Per platform build configuration */
	UPROPERTY(config)
	TMap<FName, EProjectPackagingBuildConfigurations> PerPlatformBuildConfig;

	/** Per platform flavor cooking target */
	UPROPERTY(config)
	TMap<FName, FName> PerPlatformTargetFlavorName;

	/** Per platform architecture */
	UPROPERTY(config, EditAnywhere, Category=Project)
	TMap<FName, FString> PerPlatformArchitecture;

	/** Per platform build target */
	UPROPERTY(config, EditAnywhere, Category=Project)
	TMap<FName, FString> PerPlatformBuildTarget;


	
};

#undef UE_API
