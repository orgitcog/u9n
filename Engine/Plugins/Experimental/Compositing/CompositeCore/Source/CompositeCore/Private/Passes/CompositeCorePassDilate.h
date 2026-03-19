// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphFwd.h"
#include "RHIFwd.h"

namespace UE
{
	namespace CompositeCore
	{
		namespace Private
		{
			struct FDilateInputs
			{
				/** Size of the dilation step. WARNING: Not currently implemented. */
				int32 DilationSize = 1;
				
				/** Opacify the pass output to solid colors. */
				bool bOpacifyOutput = true;
			};

			/**
			* Compute shader dilation pass of non-translucent color texels. This is done as preparation
			* for compositing to hide aliasing under the main render's anti-aliased edges, with an
			* optional opacification step.
			*/
			void AddDilatePass(FRDGBuilder& GraphBuilder, FRDGTextureRef Input, FRDGTextureRef Output, ERHIFeatureLevel::Type FeatureLevel, const FDilateInputs& PassInputs = {});
		}
	}
}
