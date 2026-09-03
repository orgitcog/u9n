// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataLinkGraphCompileStatus.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"

class FUICommandList;
class UDataLinkGraphAssetEditor;
class UToolMenu;
struct FSlateIcon;
struct FToolMenuSection;

class FDataLinkGraphCompilerTool : public TSharedFromThis<FDataLinkGraphCompilerTool>
{
public:
	static void ExtendMenu(UToolMenu* InMenu);

	explicit FDataLinkGraphCompilerTool(UDataLinkGraphAssetEditor* InAssetEditor);

	void BindCommands(const TSharedRef<FUICommandList>& InCommandList);

	void Compile();

private:
	static void ExtendDynamicCompilerSection(FToolMenuSection& InSection);

	FSlateIcon GetCompileIcon() const;

	TObjectPtr<UDataLinkGraphAssetEditor> AssetEditor;

	EDataLinkGraphCompileStatus LastCompiledStatus;
};

