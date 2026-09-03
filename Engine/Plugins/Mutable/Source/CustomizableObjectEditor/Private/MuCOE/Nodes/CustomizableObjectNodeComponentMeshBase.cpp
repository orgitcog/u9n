// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeComponentMeshBase.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"


TArray<FEdGraphPinReference>& ICustomizableObjectNodeComponentMeshInterface::GetLODPins()
{
	const ICustomizableObjectNodeComponentMeshInterface* ConstThis = this;
	return const_cast<TArray<FEdGraphPinReference>&>(ConstThis->GetLODPins());
}
