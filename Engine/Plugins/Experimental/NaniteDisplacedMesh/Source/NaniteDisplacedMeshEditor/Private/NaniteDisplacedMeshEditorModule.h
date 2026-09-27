// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "UObject/StrongObjectPtr.h"

class UNaniteDisplacedMesh;
class FAssetTypeActions_NaniteDisplacedMesh;
class UPackage;
class FString;

struct FNaniteDisplacedMeshLinkParameters;
struct FNaniteDisplacedMeshParams;
struct FValidatedNaniteDisplacedMeshParams;

enum class ELinkDisplacedMeshAssetSetting : uint8;

class FNaniteDisplacedMeshEditorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FNaniteDisplacedMeshEditorModule& GetModule();

	UPackage* GetNaniteDisplacementMeshTransientPackage() const;

	UE_DEPRECATED(5.6, "Use FOnOverrideNaniteDisplacedMeshLink Instead")
	DECLARE_DELEGATE_RetVal_ThreeParams(UNaniteDisplacedMesh*, FOnLinkDisplacedMesh, const FNaniteDisplacedMeshParams& /*InParameters*/, const FString& /*DisplacedMeshFolder*/, const ELinkDisplacedMeshAssetSetting& /*LinkDisplacedMeshAssetSetting*/);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use OverrideNaniteDisplacedMeshLink Instead")
	FOnLinkDisplacedMesh OnLinkDisplacedMeshOverride;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Invoked when a nanite displaced mesh is linked
	DECLARE_DELEGATE_RetVal_ThreeParams(UNaniteDisplacedMesh*, FOOnOverideDisplacedMeshLink, UNaniteDisplacedMesh* /*ExistingDisplacedMesh*/, FValidatedNaniteDisplacedMeshParams&& /*InParameters*/, const FNaniteDisplacedMeshLinkParameters& /*InLinkParameters*/);
	FOOnOverideDisplacedMeshLink OverrideNaniteDisplacedMeshLink;

private:
	FAssetTypeActions_NaniteDisplacedMesh* NaniteDisplacedMeshAssetActions;
	TStrongObjectPtr<UPackage> NaniteDisplacedMeshTransientPackage;
};
