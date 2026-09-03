// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMHost.h"
#include "AnimNextRigVMAsset.generated.h"

#define UE_API ANIMNEXT_API

class UAnimNextVariableEntry;
class UAnimNextRigVMAssetEditorData;
struct FAnimNextModuleInstance;

namespace UE::AnimNext
{
	struct FProxyVariablesContext;
	struct FModuleEventTickFunction;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

/** Base class for all AnimNext assets that can host RigVM logic */
UCLASS(MinimalAPI, Abstract)
class UAnimNextRigVMAsset : public URigVMHost
{
	GENERATED_BODY()

protected:
	friend class UAnimNextVariableEntry;
	friend class UAnimNextRigVMAssetEditorData;
	friend struct FAnimNextModuleInstance;
	friend struct UE::AnimNext::FModuleEventTickFunction;
	friend struct UE::AnimNext::FProxyVariablesContext;
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	UE_API UAnimNextRigVMAsset(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	UE_API virtual void BeginDestroy() override;
	UE_API virtual void PostLoad() override;
	UE_API virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_API virtual void PreDuplicate(FObjectDuplicationParameters& DupParams) override;

public:
	// Get public variable defaults property bag
	const FInstancedPropertyBag& GetPublicVariableDefaults() const { return PublicVariableDefaults; }

	// Get the RigVM object we host
	const URigVM* GetRigVM() const { return RigVM; }

private:
	// URigVMHost interface
	UE_API virtual TArray<FRigVMExternalVariable> GetExternalVariablesImpl(bool bFallbackToBlueprint) const;

protected:
	// The ExtendedExecuteContext object holds the common work data used by the RigVM internals. It is populated during the initial VM initialization.
	// Each instance of an AnimGraph requires a copy of this context and a call to initialize the VM instance with the context copy, 
	// so the cached memory handles are updated to the correct memory addresses.
	// This context is used as a reference to copy the common data for all instances created.
	UPROPERTY(Transient)
	FRigVMExtendedExecuteContext ExtendedExecuteContext;

	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

	// Variables and their defaults (including public variables, sorted first)
	UPROPERTY()
	FInstancedPropertyBag VariableDefaults;

	// Public variables (for easy duplication)
	UPROPERTY()
	FInstancedPropertyBag PublicVariableDefaults;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Editor Data", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;
#endif
};

#undef UE_API
