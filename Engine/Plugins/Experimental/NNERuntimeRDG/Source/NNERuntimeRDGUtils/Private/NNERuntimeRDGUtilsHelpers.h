// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace UE::NNERuntimeRDGUtils::Private
{

TOptional<uint32> GetOpVersionFromOpsetVersion(const FString& OpType, int OpsetVersion);

} // namespace UE::NNERuntimeRDGUtils::Private