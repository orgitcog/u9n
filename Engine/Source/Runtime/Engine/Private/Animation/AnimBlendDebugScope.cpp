// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimBlendDebugScope.h"
#include "Animation/AnimNodeBase.h"

IMPLEMENT_ANIMGRAPH_MESSAGE(UE::Anim::FAnimBlendDebugScope);

namespace UE { namespace Anim {


FAnimBlendDebugScope::FAnimBlendDebugScope(const FAnimationBaseContext& InContext, const int32& InBlendIndex, const int32& InNumBlends, const FColor& InDebugColor)
	: BlendIndex(InBlendIndex)
	, NumBlends(InNumBlends)
	, DebugColor(InDebugColor)
	, NodeId(InContext.GetCurrentNodeId())
{
}


}}	// namespace UE::Anim
