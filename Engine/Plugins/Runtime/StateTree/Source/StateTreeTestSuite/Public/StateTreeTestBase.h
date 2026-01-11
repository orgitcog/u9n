// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "AITestsCommon.h"

#include "StateTree.h"
#include "StateTreePropertyBindings.h"

class UStateTreeEditorData;
namespace UE::StateTree::Tests
{
	
/**
 * Base class for StateTree test
 */
struct FStateTreeTestBase : public FAITestBase
{
protected:
	UStateTree& NewStateTree() const;
	static FStateTreePropertyPathBinding MakeBinding(const FGuid& SourceID, const FStringView Source, const FGuid& TargetID, const FStringView Target);
	static FGameplayTag GetTestTag1();
	static FGameplayTag GetTestTag2();
	static FGameplayTag GetTestTag3();
	FInstancedPropertyBag& GetRootPropertyBag(UStateTreeEditorData& EditorData) const;
};

} // namespace UE::StateTree::Tests