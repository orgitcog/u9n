// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "UObject/WeakObjectPtr.h"

class UAnimNextEdGraphNode;

namespace UE::Workspace
{
	class IWorkspaceEditor;
}

namespace UE::AnimNext::Editor
{

struct FTraitStackData
{
	FTraitStackData() = default;

	explicit FTraitStackData(const TWeakObjectPtr<UAnimNextEdGraphNode>& InEdGraphNodeWeak)
		: EdGraphNodeWeak(InEdGraphNodeWeak)
	{}

	TWeakObjectPtr<UAnimNextEdGraphNode> EdGraphNodeWeak = nullptr;
};

// Interface used to talk to the trait stack editor embedded in a workspace
class ITraitStackEditor : public IModularFeature
{
public:
	static inline FLazyName ModularFeatureName = FLazyName(TEXT("TraitStackEditor"));

	virtual ~ITraitStackEditor() = default;

	// Sets the trait data to be displayed for the specified workspace editor instance
	virtual void SetTraitData(const TSharedRef<UE::Workspace::IWorkspaceEditor>& InWorkspaceEditor, const FTraitStackData& InTraitStackData) = 0;
};

}