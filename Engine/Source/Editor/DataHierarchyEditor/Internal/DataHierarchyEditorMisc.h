// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "UObject/NameTypes.h"


namespace UE::DataHierarchyEditor
{
	DATAHIERARCHYEDITOR_API FName GetUniqueName(FName CandidateName, const TSet<FName>& ExistingNames);
}
