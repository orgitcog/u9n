// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/RigVMDetailsViewWrapperObject.h"
#include "Rigs/RigHierarchyDefines.h"
#include "ControlRigWrapperObject.generated.h"

UCLASS()
class CONTROLRIGEDITOR_API UControlRigWrapperObject : public URigVMDetailsViewWrapperObject
{
public:
	GENERATED_BODY()

	virtual UClass* GetClassForStruct(UScriptStruct* InStruct, bool bCreateIfNeeded) const override;

	virtual void SetContent(const uint8* InStructMemory, const UStruct* InStruct) override;
	virtual void GetContent(uint8* OutStructMemory, const UStruct* InStruct) const override;;

	FRigHierarchyKey HierarchyKey;
};
