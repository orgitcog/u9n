// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGUtilsModelOptimizer.h"

#include "NNERuntimeRDGUtilsModelOptimizerNNE.h"

namespace UE::NNERuntimeRDGUtils::Internal
{

TUniquePtr<IModelOptimizer> CreateModelOptimizer()
{
	return MakeUnique<Private::FModelOptimizerONNXToNNERT>();
}

} // namespace UE::NNERuntimeRDGUtils::Internal
