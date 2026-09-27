// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdMode.h"

#include "AnimNextEditorContext.generated.h"

namespace UE::Workspace
{
	class IWorkspaceEditor;
}

UCLASS(Transient)
class UAnimNextEditorContext : public UObject
{
	GENERATED_BODY()

public:
	// The workspace editor host
	TWeakPtr<UE::Workspace::IWorkspaceEditor> WeakWorkspaceEditor;
};
