// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CompositeCorePassProxy.h"

#include "CompositeCorePassMergeProxy.generated.h"

#define UE_API COMPOSITECORE_API

UENUM()
enum class ECompositeCoreMergeOp : uint8
{
	None,
	Over,
	Add,
	Multiply,
	Divide,
};

namespace UE
{
	namespace CompositeCore
	{
		class FMergePassProxy : public FCompositeCorePassProxy
		{
		public:
			using FCompositeCorePassProxy::FCompositeCorePassProxy;

			UE_API FPassOutput Add(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPassInputArray& Inputs, const FPassContext& PassContext) const override;

			/** Merge operation. */
			ECompositeCoreMergeOp MergeOp = ECompositeCoreMergeOp::Over;

			/** Flag to enable or disable application of engine-provided lens distortion LUTs, true by default. */
			bool bUseLensDistortion = true;
		};
	}
}

#undef UE_API
