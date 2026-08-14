// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class AActor;
class FEditorViewportClient;
class FLevelEditorViewportClient;
struct FWorldSelectionElementArgs;

class IEditorViewportClientProxy : public TSharedFromThis<IEditorViewportClientProxy>
{
public:
	virtual ~IEditorViewportClientProxy() = default;
	static IEditorViewportClientProxy* CreateViewportClientProxy(FEditorViewportClient* InViewportClient);
	static bool IsMouseOnWidgetAxis(IEditorViewportClientProxy* InEditorViewportClientProxy);


	virtual bool IsActorVisible(const AActor* InActor) const = 0;

	virtual const TArray<FName> GetHiddenLayers() const = 0;

	virtual FEditorViewportClient* GetEditorViewportClient() = 0;
};

class FEditorViewportClientProxy : public IEditorViewportClientProxy
{
public:
	FEditorViewportClientProxy(FEditorViewportClient* InEditorViewportClient);
	
	virtual bool IsActorVisible(const AActor* InActor) const override;
	virtual const TArray<FName> GetHiddenLayers() const override;
	virtual FEditorViewportClient* GetEditorViewportClient() override;

private:
	TSharedPtr<FEditorViewportClient> EditorViewportClient;
};

class FLevelEditorViewportClientProxy : public IEditorViewportClientProxy
{
public:
	FLevelEditorViewportClientProxy(FLevelEditorViewportClient *InLevelEditorViewportClient);

	virtual bool IsActorVisible(const AActor* InActor) const override;
	virtual const TArray<FName> GetHiddenLayers() const override;
	virtual FEditorViewportClient* GetEditorViewportClient() override;

private:
	TSharedPtr<FLevelEditorViewportClient> LevelEditorViewportClient;
};

