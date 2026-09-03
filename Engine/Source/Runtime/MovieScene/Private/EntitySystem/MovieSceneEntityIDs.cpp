// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntityManager.h"

namespace UE
{
namespace MovieScene
{

bool FEntityHandle::IsValid(const FEntityManager& InEntityManager) const
{
	return InEntityManager.IsHandleValid(*this);
}

bool FComponentTypeIDFilter::Passes(const FComponentMask& Type) const
{
	FComponentTypeID ConditionComponent = GetComponentType();
	return !ConditionComponent || Type.Contains(ConditionComponent) == PassFlag;
}

} // namespace MovieScene
} // namespace UE
