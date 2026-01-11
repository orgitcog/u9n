// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVMDebugDrawSettings.h"
#include "RigVMFunction_DebugBase.generated.h"

USTRUCT(meta=(Abstract, Category="Debug", NodeColor = "0.83077 0.846873 0.049707"))
struct FRigVMFunction_DebugBase : public FRigVMStruct
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input, DetailsOnly, DisplayName = "Draw Settings"))
	FRigVMDebugDrawSettings DebugDrawSettings;
};

USTRUCT(meta=(Abstract, Category="Debug", NodeColor = "0.83077 0.846873 0.049707"))
struct FRigVMFunction_DebugBaseMutable : public FRigVMStructMutable
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input, DetailsOnly, DisplayName = "Draw Settings"))
	FRigVMDebugDrawSettings DebugDrawSettings;
};
