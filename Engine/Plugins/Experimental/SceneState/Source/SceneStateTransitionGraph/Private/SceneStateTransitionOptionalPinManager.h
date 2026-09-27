// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node.h"

namespace UE::SceneState::Graph
{

struct FTransitionOptionalPinManager : FOptionalPinManager
{
	//~ Begin FOptionalPinManager
	virtual void PostInitNewPin(UEdGraphPin* InPin
		, FOptionalPinFromProperty& InRecord
		, int32 InArrayIndex
		, FProperty* InProperty
		, uint8* InPropertyAddress
		, uint8* InDefaultPropertyAddress) const override;
	//~ End FOptionalPinManager
};

} // UE::SceneState::Graph
