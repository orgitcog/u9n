// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"
#include "SceneQueries/SceneSnappingManager.h"

#include "EditorSnappingManager.generated.h"

class UInteractiveToolsContext;
struct FSceneSnapQueryResult;
struct FSceneSnapQueryRequest;
struct FSceneHitQueryResult;
struct FSceneHitQueryRequest;
class USceneSnappingManager;
class UInteractiveGizmoManager;
class IToolsContextQueriesAPI;

class UEditorSceneSnappingManager;

namespace UE::Editor::Gizmos
{
	EDITORINTERACTIVETOOLSFRAMEWORK_API bool RegisterSceneSnappingManager(UInteractiveToolsContext* InToolsContext);

	EDITORINTERACTIVETOOLSFRAMEWORK_API bool DeregisterSceneSnappingManager(const UInteractiveToolsContext* InToolsContext);

	EDITORINTERACTIVETOOLSFRAMEWORK_API UEditorSceneSnappingManager* FindSceneSnappingManager(const UInteractiveToolManager* InToolManager);
}

UCLASS(MinimalAPI)
class UEditorSceneSnappingManager : public USceneSnappingManager
{
	GENERATED_BODY()

public:
	void Initialize(const TObjectPtr<UInteractiveToolsContext>& InToolsContext);
	void Shutdown();

	virtual bool ExecuteSceneHitQuery(const FSceneHitQueryRequest& InRequest, FSceneHitQueryResult& OutResult) const override;
	virtual bool ExecuteSceneSnapQuery(const FSceneSnapQueryRequest& InRequest, TArray<FSceneSnapQueryResult>& OutResults) const override;

protected:
	const IToolsContextQueriesAPI* QueriesAPI = nullptr;
};
