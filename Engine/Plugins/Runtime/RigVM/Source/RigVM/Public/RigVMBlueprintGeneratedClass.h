// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprintGeneratedClass.generated.h"

#define UE_API RIGVM_API

USTRUCT()
struct FRigVMGraphFunctionHeaderArray
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FRigVMGraphFunctionHeader> Headers;
};

UCLASS(MinimalAPI)
class URigVMBlueprintGeneratedClass : public UBlueprintGeneratedClass, public IRigVMGraphFunctionHost
{
	GENERATED_UCLASS_BODY()

public:

	// UClass interface
	UE_API virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const override;
	UE_API virtual void PostInitInstance(UObject* InObj, FObjectInstancingGraph* InstanceGraph) override;

	// UObject interface
	UE_API virtual void Serialize(FArchive& Ar) override;
	UE_API virtual void PostLoad() override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	UE_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	UE_API virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;

	// IRigVMGraphFunctionHost interface
	virtual const FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() const override { return &GraphFunctionStore; }
	virtual FRigVMGraphFunctionStore* GetRigVMGraphFunctionStore() override { return &GraphFunctionStore; }

	UPROPERTY()
	FRigVMGraphFunctionStore GraphFunctionStore;
};

#undef UE_API
