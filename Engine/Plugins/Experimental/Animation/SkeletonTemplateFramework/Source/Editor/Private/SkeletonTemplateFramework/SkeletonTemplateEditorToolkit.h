// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class USkeletonTemplate;
class SDockTab;
namespace UE::Anim::STF
{
	class SAttributesTreeView;
	class SAttributeSetsTreeView;
	class SAttributeMappingsTreeView;
}

class ISkeletonTemplateEditorToolkit
{
public:
	virtual ~ISkeletonTemplateEditorToolkit() = default;
	
	virtual void SetDetailsObject(TObjectPtr<UObject> InObject) = 0;
};

class FSkeletonTemplateEditorToolkit : public FAssetEditorToolkit, public ISkeletonTemplateEditorToolkit
{
public:
	void InitEditor(const TArray<UObject*>& InObjects);

	// IToolkit
	void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	FName GetToolkitFName() const override;
	FText GetBaseToolkitName() const override;
	FString GetWorldCentricTabPrefix() const override;
	FLinearColor GetWorldCentricTabColorScale() const override;

	// ISkeletonTemplateEditorToolkit
	void SetDetailsObject(TObjectPtr<UObject> InObject) override;
	
private:
	TSharedRef<SDockTab> SpawnTabAttributes(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTabAttributeSets(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTabAttributeMappings(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTabDetails(const FSpawnTabArgs& Args);

	TObjectPtr<USkeletonTemplate> SkeletonTemplate;

	TSharedPtr<UE::Anim::STF::SAttributesTreeView> AttributesTreeView;

	TSharedPtr<UE::Anim::STF::SAttributeSetsTreeView> AttributeSetsTreeView;

	TSharedPtr<UE::Anim::STF::SAttributeMappingsTreeView> AttributeMappingsTreeView;

	TSharedPtr<IDetailsView> DetailsView;
};