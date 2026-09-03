// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "AssetRegistry/AssetDependencyGatherer.h"
#include "AssetRegistry/AssetData.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Engine/World.h"

struct FARFilter;

class FExternalObjectAndActorDependencyGatherer : public IAssetDependencyGatherer
{
public:	
	FExternalObjectAndActorDependencyGatherer() = default;
	virtual ~FExternalObjectAndActorDependencyGatherer() = default;

	static ENGINE_API FARFilter GetQueryFilter(FName PackageName, TArray<FString>* OutQueryDirectories = nullptr);

	ENGINE_API virtual void GatherDependencies(const FAssetData& AssetData, const FAssetRegistryState& AssetRegistryState,
		TFunctionRef<FARCompiledFilter(const FARFilter&)> CompileFilterFunc,
		TArray<IAssetDependencyGatherer::FGathereredDependency>& OutDependencies,
		TArray<FString>& OutDependencyDirectories) const override;
};

#endif
