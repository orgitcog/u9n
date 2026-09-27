// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

namespace UE::NNERuntimeRDG::Private { class FTensor; }

namespace UE::NNERuntimeRDG::Private::CPUHelper::Cast
{
	void NNERUNTIMERDG_API Apply(const FTensor& Tensor, FTensor& OutputTensor);

} // UE::NNERuntimeRDG::Private::CPUHelper::Cast