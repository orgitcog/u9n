// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

struct FMeshDescription;

class UProceduralMeshComponent;

FMeshDescription
PROCEDURALMESHCOMPONENT_API
BuildMeshDescription( UProceduralMeshComponent* ProcMeshComp );

void
PROCEDURALMESHCOMPONENT_API
MeshDescriptionToProcMesh( const FMeshDescription& MeshDescription, UProceduralMeshComponent* ProcMeshComp );
