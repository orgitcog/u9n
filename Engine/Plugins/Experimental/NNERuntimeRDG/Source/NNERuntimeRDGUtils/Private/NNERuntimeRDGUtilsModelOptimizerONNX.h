// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NNERuntimeRDGUtilsModelOptimizerBase.h"

namespace UE::NNERuntimeRDGUtils::Private
{

class FModelOptimizerONNXToONNX : public FModelOptimizerBase
{
public:
	FModelOptimizerONNXToONNX();

	virtual FString GetName() const override
	{
		return TEXT("NNEModelOptimizerFromONNXToONNX");
	}
};

} // namespace UE::NNERuntimeRDGUtils::Private
