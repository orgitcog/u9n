// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeStatePath.h"

class UStateTree;
class UObject;

#if WITH_STATETREE_DEBUG

namespace UE::StateTree::Debug
{
/**
 * For debugging purposes. Data used for runtime check.
*/
class FRuntimeValidationInstanceData
{
public:
	FRuntimeValidationInstanceData() = default;
	~FRuntimeValidationInstanceData();

	void SetContext(const UObject* Owner, const UStateTree* StateTree);

	void NodeEnterState(FGuid NodeID, FActiveFrameID FrameID);
	void NodeExitState(FGuid NodeID, FActiveFrameID FrameID);

private:
	void ValidatesTreeNodes(const UStateTree* InNewStateTree) const;

private:
	enum class EState : uint8
	{
		None = 0x00,
		BetweenEnterExitState = 0x01,
	};
	FRIEND_ENUM_CLASS_FLAGS(EState);

	struct FNodeStatePair
	{
		FGuid NodeID;
		FActiveFrameID FrameID;
		EState State = EState::None;
	};
	TArray<FNodeStatePair> NodeStates;
	TWeakObjectPtr<const UStateTree> StateTree;
	TWeakObjectPtr<const UObject> Owner;
};

ENUM_CLASS_FLAGS(FRuntimeValidationInstanceData::EState);

} // UE::StateTree::Debug

#endif // WITH_STATETREE_DEBUG