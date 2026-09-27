// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class DNAINTERCHANGE_API FDNAInterchangeModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FDNAInterchangeModule& GetModule();

	/** DNAInterchangeModule implementation */

	class USkeletalMesh* ImportSync(const FString& InNewRigAssetName, const FString& InNewRigPath);

private:

	void PopulateSkelMeshData(class USkeletalMesh* InSkelMesh, const FString& InPathToDNA);
	void PopulateSkeletonData(class USkeleton* InSkeleton, const FString& InPluginDir);
};
