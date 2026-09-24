// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_RIGVMLEGACYEDITOR
#include "Editor/RigVMLegacyEditor.h"
#endif
#include "Editor/RigVMNewEditor.h"

class UControlRigBlueprint;
class FBlueprintActionDatabaseRegistrar;

class IControlRigEditor
{
public:
	virtual URigVMHost* GetRigVMHost() const = 0;
};

#if WITH_RIGVMLEGACYEDITOR
class IControlRigLegacyEditor : public FRigVMLegacyEditor, public IControlRigEditor
{
};
#endif

class IControlRigNewEditor : public FRigVMNewEditor, public IControlRigEditor
{
};
