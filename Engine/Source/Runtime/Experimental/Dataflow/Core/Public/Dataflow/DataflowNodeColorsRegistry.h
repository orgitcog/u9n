// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowSettings.h"
#include "ChaosLog.h"

struct FDataflowNode;
struct FDataflowConnection;
	
class FLazySingleton;

namespace UE::Dataflow
{
	//
	// Registry for custom Node colors
	//
	class FNodeColorsRegistry
	{
	public:
		static DATAFLOWCORE_API FNodeColorsRegistry& Get();
		static DATAFLOWCORE_API void TearDown();

		DATAFLOWCORE_API void RegisterNodeColors(const FName& Category, const FNodeColors& NodeColors);
		DATAFLOWCORE_API FLinearColor GetNodeTitleColor(const FName& Category) const;
		DATAFLOWCORE_API FLinearColor GetNodeBodyTintColor(const FName& Category) const;
		DATAFLOWCORE_API void NodeColorsChangedInSettings(const FNodeColorsMap& NodeColorsMap);

	private:
		DATAFLOWCORE_API FNodeColorsRegistry();
		DATAFLOWCORE_API ~FNodeColorsRegistry();

		FNodeColorsMap ColorsMap;					// [Category] -> Colors
		FDelegateHandle DataflowSettingsChangedDelegateHandle;

		friend FLazySingleton;
	};

	//
	// Registry for custom Pin colors
	//
	class FPinSettingsRegistry
	{
	public:
		static DATAFLOWCORE_API FPinSettingsRegistry& Get();
		static DATAFLOWCORE_API void TearDown();

		DATAFLOWCORE_API void RegisterPinSettings(const FName& PinType, const FPinSettings& InSettings);
		DATAFLOWCORE_API FLinearColor GetPinColor(const FName& PinType) const;
		DATAFLOWCORE_API float GetPinWireThickness(const FName& PinType) const;
		DATAFLOWCORE_API void PinSettingsChangedInSettings(const FPinSettingsMap& PinSettingsrMap);
		DATAFLOWCORE_API bool IsPinTypeRegistered(const FName& PinType) const;

	private:
		DATAFLOWCORE_API FPinSettingsRegistry();
		DATAFLOWCORE_API ~FPinSettingsRegistry();

		FPinSettingsMap SettingsMap;					// [PinType] -> {Color, WireThickness}
		FDelegateHandle DataflowSettingsChangedDelegateHandle;

		friend FLazySingleton;
	};
}

