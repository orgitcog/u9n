// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class SEditorViewport;
class FEditorViewportClient;
class FMenuBuilder;
class FUICommandInfo;
class FUICommandList;
struct FToolMenuEntry;
enum class EVirtualTextureVisualizationMode : uint8;

class COMMONMENUEXTENSIONS_API FVirtualTextureVisualizationMenuCommands : public TCommands<FVirtualTextureVisualizationMenuCommands>
{
public:
	struct FVisualizationRecord
	{
		FName Name;
		TSharedPtr<FUICommandInfo> Command;
		EVirtualTextureVisualizationMode ModeID;
	};

	typedef TMultiMap<FName, FVisualizationRecord> TVisualizationModeCommandMap;
	typedef TVisualizationModeCommandMap::TConstIterator TCommandConstIterator;

	FVirtualTextureVisualizationMenuCommands();

	TCommandConstIterator CreateCommandConstIterator() const;

	static void BuildVisualisationSubMenu(FMenuBuilder& Menu);

	virtual void RegisterCommands() override;

	void BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const;

	inline bool IsPopulated() const
	{
		return CommandMap.Num() > 0;
	}

private:
	void BuildCommandMap();
	bool AddCommandTypeToMenu(FMenuBuilder& Menu, const EVirtualTextureVisualizationMode ModeID) const;

	static void ChangeVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);
	static bool IsVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName);
	
private:
	TVisualizationModeCommandMap CommandMap;
};
