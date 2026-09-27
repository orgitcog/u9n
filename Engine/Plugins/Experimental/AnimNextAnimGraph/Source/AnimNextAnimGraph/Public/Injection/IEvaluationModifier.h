// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::AnimNext
{
	struct FEvaluateTraversalContext;
};

namespace UE::AnimNext
{
	// Override point allowing injection sites to take over evaluation and inject their own tasks
	struct IEvaluationModifier
	{
		virtual ~IEvaluationModifier() = default;

		// Called before an injection site trait's children are evaluated
		virtual void PreEvaluate(FEvaluateTraversalContext& Context) const = 0;

		// Called after an injection site trait's children are evaluated
		virtual void PostEvaluate(FEvaluateTraversalContext& Context) const = 0;
	};
}
