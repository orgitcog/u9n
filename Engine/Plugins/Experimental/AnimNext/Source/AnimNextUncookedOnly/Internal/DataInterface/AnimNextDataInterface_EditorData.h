// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextExecuteContext.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "AnimNextDataInterface_EditorData.generated.h"

class UAnimNextDataInterfaceFactory;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

/** Editor data for AnimNext data interfaces */
UCLASS()
class ANIMNEXTUNCOOKEDONLY_API UAnimNextDataInterface_EditorData : public UAnimNextRigVMAssetEditorData
{
	GENERATED_BODY()

protected:
	// UAnimNextRigVMAssetEditorData interface
	virtual UScriptStruct* GetExecuteContextStruct() const override { return FAnimNextExecuteContext::StaticStruct(); }
	virtual TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> GetEntryClasses() const override;
	virtual void CustomizeNewAssetEntry(UAnimNextRigVMAssetEntry* InNewEntry) const override;

	friend class UAnimNextDataInterfaceFactory;
	friend UE::AnimNext::UncookedOnly::FUtils;

	// The injection site (AnimNext Graph) that will be used by default
	UPROPERTY(EditAnywhere, Category=Injection)
	FName DefaultInjectionSite;
};
