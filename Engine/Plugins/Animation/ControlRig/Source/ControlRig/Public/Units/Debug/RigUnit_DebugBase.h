// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigVMCore/RigVMDebugDrawSettings.h"
#include "RigUnit_DebugBase.generated.h"

USTRUCT(meta=(Abstract, Category="Debug", NodeColor = "0.83077 0.846873 0.049707"))
struct FRigUnit_DebugBase : public FRigUnit
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input, DetailsOnly, DisplayName = "Draw Settings"))
	FRigVMDebugDrawSettings DebugDrawSettings;
};

USTRUCT(meta=(Abstract, Category="Debug", NodeColor = "0.83077 0.846873 0.049707"))
struct FRigUnit_DebugBaseMutable : public FRigUnitMutable
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input, DetailsOnly, DisplayName = "Draw Settings"))
	FRigVMDebugDrawSettings DebugDrawSettings;
};
