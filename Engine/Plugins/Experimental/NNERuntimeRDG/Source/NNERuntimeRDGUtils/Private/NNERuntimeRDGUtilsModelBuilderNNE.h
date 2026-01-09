// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNERuntimeRDGUtilsModelBuilder.h"
#include "Templates/UniquePtr.h"

namespace UE::NNERuntimeRDGUtils::Private
{

TUniquePtr<IModelBuilder> CreateNNEModelBuilder();

} // UE::NNERuntimeRDGUtils::Private

