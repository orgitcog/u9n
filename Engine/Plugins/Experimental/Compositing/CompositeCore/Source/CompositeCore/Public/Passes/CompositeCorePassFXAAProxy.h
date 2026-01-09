// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CompositeCorePassProxy.h"

#define UE_API COMPOSITECORE_API

namespace UE
{
	namespace CompositeCore
	{
		class FFXAAPassProxy : public FCompositeCorePassProxy
		{
		public:
			using FCompositeCorePassProxy::FCompositeCorePassProxy;

			UE_API FPassOutput Add(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPassInputArray& Inputs, const FPassContext& PassContext) const override;

			/** Optional r.FXAA.Quality setting override.  */
			TOptional<int32> QualityOverride = {};
		};
	}
}

#undef UE_API
