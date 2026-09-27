// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "NNERuntimeRDGTensor.h"

namespace UE::NNERuntimeRDG::Private::CPUHelper::Concat
{
	void NNERUNTIMERDG_API Apply(TConstArrayView<FTensorRef> InputTensors, FTensor& OutputTensor, int32 Axis);

} // UE::NNERuntimeRDG::Private::CPUHelper::Concat