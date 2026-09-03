// Copyright Epic Games, Inc. All Rights Reserved.

#include "Snapping/EditorSnappingManager.h"

#include "ContextObjectStore.h"
#include "InteractiveToolsContext.h"
#include "SceneQueries/SceneSnappingManager.h"
#include "ToolContextInterfaces.h"
#include "VectorUtil.h"

namespace UE::Editor::Gizmos
{
	namespace EditorSnappingManagerLocals
	{
		// This assumes the provided AxisList only specifies a single axis
		EAxis::Type GetAxisFromAxisList(EAxisList::Type InAxisList)
		{
			if (InAxisList == EAxisList::X)
            {
                return EAxis::X;
            }

			if (InAxisList == EAxisList::Y)
			{
				return EAxis::Y;
			}

			if (InAxisList == EAxisList::Z)
			{
				return EAxis::Z;
			}

			ensureMsgf(true, TEXT("Invalid AxisList, it must specify either X, Y or Z"));

			return EAxis::None;
		}
	}

	bool RegisterSceneSnappingManager(UInteractiveToolsContext* InToolsContext)
	{
		if (!ensure(InToolsContext))
		{
			return false;
		}

		// Check for existing registration, and return true if found
		const UEditorSceneSnappingManager* FoundRegisteredSnappingManager = InToolsContext->ContextObjectStore->FindContext<UEditorSceneSnappingManager>();
		if (FoundRegisteredSnappingManager)
		{
			return true;
		}

		UEditorSceneSnappingManager* SelectionManager = NewObject<UEditorSceneSnappingManager>(InToolsContext->ToolManager);
		if (!ensure(SelectionManager))
		{
			return false;
		}

		SelectionManager->Initialize(InToolsContext);
		InToolsContext->ContextObjectStore->AddContextObject(SelectionManager);

		return true;
	}

	bool DeregisterSceneSnappingManager(const UInteractiveToolsContext* InToolsContext)
	{
		if (!ensure(InToolsContext))
		{
			return false;
		}

		if (UEditorSceneSnappingManager* FoundRegisteredSnappingManager = InToolsContext->ContextObjectStore->FindContext<UEditorSceneSnappingManager>())
		{
			FoundRegisteredSnappingManager->Shutdown();
			InToolsContext->ContextObjectStore->RemoveContextObject(FoundRegisteredSnappingManager);
		}

		return true;
	}

	UEditorSceneSnappingManager* FindSceneSnappingManager(const UInteractiveToolManager* InToolManager)
	{
		if (!ensure(InToolManager))
		{
			return nullptr;
		}

		if (UEditorSceneSnappingManager* FoundRegisteredSnappingManager = InToolManager->GetContextObjectStore()->FindContext<UEditorSceneSnappingManager>())
		{
			return FoundRegisteredSnappingManager;
		}

		return nullptr;
	}
}

void UEditorSceneSnappingManager::Initialize(const TObjectPtr<UInteractiveToolsContext>& InToolsContext)
{
	UInteractiveToolManager* ToolManager = InToolsContext ? InToolsContext->ToolManager : nullptr;
	QueriesAPI = ToolManager ? ToolManager->GetContextQueriesAPI() : nullptr;
}

void UEditorSceneSnappingManager::Shutdown()
{
	QueriesAPI = nullptr;
}

bool UEditorSceneSnappingManager::ExecuteSceneHitQuery(const FSceneHitQueryRequest& InRequest, FSceneHitQueryResult& OutResult) const
{
	// @todo: implement when needed/supported
	return false;
}

bool UEditorSceneSnappingManager::ExecuteSceneSnapQuery(const FSceneSnapQueryRequest& InRequest, TArray<FSceneSnapQueryResult>& OutResults) const
{
	return false;
}
