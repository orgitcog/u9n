// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class USkeletonBinding;
class SDockTab;
namespace UE::Anim::STF
{
	class SAttributeBindingsTreeView;
	class SBindingSetsTreeView;
	class SBindingMappingsTreeView;
}

class ISkeletonBindingEditorToolkit
{
public:
	virtual ~ISkeletonBindingEditorToolkit() = default;
	
	virtual void SetDetailsObject(TObjectPtr<UObject> InObject) = 0;
};

class FSkeletonBindingEditorToolkit : public FAssetEditorToolkit, public ISkeletonBindingEditorToolkit
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

	TObjectPtr<USkeletonBinding> SkeletonBinding;

	TSharedPtr<IDetailsView> DetailsView;

	TSharedPtr<UE::Anim::STF::SAttributeBindingsTreeView> AttributeBindingsTreeView;

	TSharedPtr<UE::Anim::STF::SBindingSetsTreeView> BindingSetsTreeView;

	TSharedPtr<UE::Anim::STF::SBindingMappingsTreeView> BindingMappingsTreeView;
};