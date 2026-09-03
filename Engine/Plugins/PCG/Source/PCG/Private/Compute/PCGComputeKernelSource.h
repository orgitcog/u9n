// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ComputeKernelSource.h"

#include "PCGComputeKernelSource.generated.h"

UCLASS(MinimalAPI)
class UPCGComputeKernelSource : public UComputeKernelSource
{
	GENERATED_BODY()

public:
	//~ Begin UComputeKernelSource Interface.
	FString GetSource() const override
	{
#if WITH_EDITOR
		return Source;
#else
		ensure(false);
		return "";
#endif
	}
	//~ End UComputeKernelSource Interface.

#if WITH_EDITOR
	void SetSource(FString const& InSource) { Source = InSource; }

protected:
	FString Source;
#endif
};
