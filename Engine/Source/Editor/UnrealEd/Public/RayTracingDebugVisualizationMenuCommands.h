// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/Commands.h"

class FEditorViewportClient;
class UToolMenu;
struct FToolMenuSection;

class UNREALED_API FRayTracingDebugVisualizationMenuCommands : public TCommands<FRayTracingDebugVisualizationMenuCommands>
{
public:
	enum class FVisualizationType : uint8
	{
		Overview,
		Standard,
		Performance,
		Timing,
		Other,
	};

	struct FVisualizationRecord
	{
		FName Name;
		TSharedPtr<FUICommandInfo> Command;
		FVisualizationType Type;

		FVisualizationRecord()
			: Name()
			, Command()
			, Type(FVisualizationType::Overview)
		{
		}
	};

	typedef TMultiMap<FName, FVisualizationRecord> TVisualizationModeCommandMap;
	typedef TVisualizationModeCommandMap::TConstIterator TCommandConstIterator;

	FRayTracingDebugVisualizationMenuCommands();

	TCommandConstIterator CreateCommandConstIterator() const;

	UE_DEPRECATED(5.6, "Please use the version taking UToolMenu* as argument")
	static void BuildVisualisationSubMenu(FMenuBuilder& Menu);

	static void BuildVisualisationSubMenu(UToolMenu* InMenu);

	virtual void RegisterCommands() override;

	void BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const;

	inline bool IsPopulated() const
	{
		return CommandMap.Num() > 0;
	}

private:
	void BuildCommandMap();

	/** Remove this function when removing deprecated BuildVisualisationSubMenu(FMenuBuilder& Menu) */
	bool AddCommandTypeToMenu(FMenuBuilder& Menu, const FVisualizationType Type) const;

	bool AddCommandTypeToSection(FToolMenuSection& InSection, const FVisualizationType Type) const;

	static void ChangeRayTracingDebugVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);
	static bool IsRayTracingDebugVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);

	TVisualizationModeCommandMap CommandMap;
};
