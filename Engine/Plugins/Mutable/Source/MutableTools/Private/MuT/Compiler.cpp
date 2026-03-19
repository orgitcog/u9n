// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/Compiler.h"

#include "MuT/CompilerPrivate.h"
#include "MuT/AST.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/ASTOpInstanceAdd.h"
#include "MuT/CodeGenerator.h"
#include "MuT/CodeOptimiser.h"
#include "MuT/ErrorLog.h"
#include "MuT/Node.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Serialisation.h"
#include "MuR/MutableRuntimeModule.h"

#include "Trace/Detail/Channel.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformCrt.h"
#include "Hash/CityHash.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/AssertionMacros.h"
#include "Async/ParallelFor.h"

#include <inttypes.h>	// Required for 64-bit printf macros

namespace mu
{

    const char* CompilerOptions::GetTextureLayoutStrategyName( CompilerOptions::TextureLayoutStrategy s )
    {
        static const char* s_textureLayoutStrategyName[ 2 ] =
        {
            "Unscaled Pack",
            "No Packing",
        };

        return s_textureLayoutStrategyName[(int)s];
    }


	// clang-format off
	static const FOpToolsDesc SOpToolsDescs[] =
	{
		// cached	supported base image formats
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NONE

		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_CONSTANT
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_CONSTANT
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CONSTANT
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_CONSTANT
		{ true,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_CONSTANT
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CONSTANT
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_CONSTANT
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// PR_CONSTANT
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ST_CONSTANT
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ED_CONSTANT
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// MA_CONSTANT

		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_PARAMETER
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_PARAMETER
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_PARAMETER
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_PARAMETER
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// PR_PARAMETER
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PARAMETER
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_PARAMETER
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ST_PARAMETER
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// MA_PARAMETER

		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_REFERENCE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_REFERENCE

		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_CONDITIONAL
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CONDITIONAL
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_CONDITIONAL
		{ true,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 }		},	// IM_CONDITIONAL
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CONDITIONAL
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_CONDITIONAL
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_CONDITIONAL
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ED_CONDITIONAL

		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// NU_SWITCH
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_SWITCH
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SWITCH
		{ true,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 }		},	// IM_SWITCH
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_SWITCH
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_SWITCH
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_SWITCH
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ED_SWITCH

		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_EQUAL_SC_CONST
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_AND
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_OR
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// BO_NOT

		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_ARITHMETIC
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// SC_CURVE

		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SAMPLEIMAGE
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_SWIZZLE
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_FROMSCALARS
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// CO_ARITHMETIC

		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LAYER
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LAYERCOLOUR
		{ true,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PIXELFORMAT
		{ true,		{ 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MIPMAP
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZE
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZELIKE
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RESIZEREL
		{ true,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_BLANKLAYOUT
		{ true,		{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 }		},	// IM_COMPOSE
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_INTERPOLATE
		{ true,		{ 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_SATURATE
		{ true,		{ 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_LUMINANCE
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_SWIZZLE
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_COLOURMAP
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_BINARISE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PLAINCOLOUR
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_CROP
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_PATCH
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_RASTERMESH
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MAKEGROWMAP
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_DISPLACE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_MULTILAYER
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_INVERT
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_NORMALCOMPOSITE
		{ true,		{ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IM_TRANSFORM

		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYLAYOUT
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_PREPARELAYOUT
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_DIFFERENCE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MORPH
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MERGE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MASKCLIPMESH
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MASKCLIPUVMASK
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MASKDIFF
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_REMOVEMASK
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_FORMAT
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_EXTRACTLAYOUTBLOCK
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_TRANSFORM
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPMORPHPLANE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPWITHMESH
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_SETSKELETON
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_PROJECT
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYPOSE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_BINDSHAPE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_APPLYSHAPE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_CLIPDEFORM
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_MORPHRESHAPE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_OPTIMIZESKINNING
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_ADDTAGS
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// ME_TRANSFORMWITHMESH

		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDMESH
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDIMAGE
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDVECTOR
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSCALAR
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSTRING
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDSURFACE
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDCOMPONENT
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDLOD
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDEXTENSIONDATA
		{ false,	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// IN_ADDOVERLAYMATERIAL

		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_PACK
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_MERGE
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_REMOVEBLOCKS
		{ true,		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }		},	// LA_FROMMESH
	};

	// clang-format on

	static_assert(UE_ARRAY_COUNT(SOpToolsDescs) == (int32)EOpType::COUNT, "OperationDescMismatch");

	const FOpToolsDesc& GetOpToolsDesc(EOpType type)
	{
		return SOpToolsDescs[(int32)type];
	}


	//---------------------------------------------------------------------------------------------
	FProxyFileContext::FProxyFileContext()
	{
		uint32 Seed = FPlatformTime::Cycles();
		FRandomStream RandomStream = FRandomStream((int32)Seed);
		CurrentFileIndex = RandomStream.GetUnsignedInt();
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    CompilerOptions::CompilerOptions()
    {
        m_pD = new Private();
    }


    //---------------------------------------------------------------------------------------------
    CompilerOptions::~CompilerOptions()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    CompilerOptions::Private* CompilerOptions::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetLogEnabled( bool bEnabled)
    {
        m_pD->bLog = bEnabled;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetOptimisationEnabled( bool bEnabled)
    {
        m_pD->OptimisationOptions.bEnabled = bEnabled;
        if (bEnabled)
        {
            m_pD->OptimisationOptions.bConstReduction = true;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetConstReductionEnabled( bool bConstReductionEnabled )
    {
        m_pD->OptimisationOptions.bConstReduction = bConstReductionEnabled;
    }


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetUseDiskCache(bool bEnabled)
	{
		m_pD->OptimisationOptions.DiskCacheContext = bEnabled ? &m_pD->DiskCacheContext : nullptr;
	}


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetOptimisationMaxIteration( int32 MaxIterations )
    {
        m_pD->OptimisationOptions.MaxOptimisationLoopCount = MaxIterations;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetIgnoreStates( bool bIgnore )
    {
        m_pD->bIgnoreStates = bIgnore;
    }


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetImageCompressionQuality(int32 Quality)
	{
		m_pD->ImageCompressionQuality = Quality;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetImageTiling(int32 Tiling)
	{
		m_pD->ImageTiling = Tiling;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetDataPackingStrategy(int32 MinTextureResidentMipCount, uint64 EmbeddedDataBytesLimit, uint64 PackagedDataBytesLimit)
	{
		m_pD->EmbeddedDataBytesLimit = EmbeddedDataBytesLimit;
		m_pD->PackagedDataBytesLimit = PackagedDataBytesLimit;
		m_pD->MinTextureResidentMipCount = MinTextureResidentMipCount;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetEnableProgressiveImages(bool bEnabled)
	{
		m_pD->OptimisationOptions.bEnableProgressiveImages = bEnabled;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetImagePixelFormatOverride(const FImageOperator::FImagePixelFormatFunc& InFunc)
	{
		m_pD->ImageFormatFunc = InFunc;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetReferencedResourceCallback(const FReferencedImageResourceFunc& Image, const FReferencedMeshResourceFunc& Mesh)
	{
		m_pD->OptimisationOptions.ReferencedImageResourceProvider = Image;
		m_pD->OptimisationOptions.ReferencedMeshResourceProvider = Mesh;
	}

    void CompilerOptions::SetDisableImageGeneration(bool bDisabled)
    {
		m_pD->OptimisationOptions.bDisableImageGeneration = bDisabled;
    }

    void CompilerOptions::SetDisableMeshGeneration(bool bDisabled)
    {
		m_pD->OptimisationOptions.bDisableMeshGeneration = bDisabled;
    }

	void CompilerOptions::LogStats() const
	{
		UE_LOG(LogMutableCore, Log, TEXT("   Cache Files Written : %" PRIu64), m_pD->DiskCacheContext.FilesWritten.load());
		UE_LOG(LogMutableCore, Log, TEXT("   Cache Files Read    : %" PRIu64), m_pD->DiskCacheContext.FilesRead.load());
		UE_LOG(LogMutableCore, Log, TEXT("   Cache MB Written    : %" PRIu64), m_pD->DiskCacheContext.BytesWritten.load() >> 20);
		UE_LOG(LogMutableCore, Log, TEXT("   Cache MB Read       : %" PRIu64), m_pD->DiskCacheContext.BytesRead.load()>>20);
	}
	
	
	//---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    Compiler::Compiler( Ptr<CompilerOptions> options, TFunction<void()>& InWaitCallback)
    {
        m_pD = new Private();
		m_pD->Options = options;
		m_pD->WaitCallback = InWaitCallback;
		if (!m_pD->Options)
        {
            m_pD->Options = new CompilerOptions;
        }
    }


    //---------------------------------------------------------------------------------------------
    Compiler::~Compiler()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }

    namespace Private
    {
        void RemoveDisabledAddInstanceOpsValues(const Ptr<CompilerOptions>& Options, const TArray<FStateCompilationData>& States)
        {
            MUTABLE_CPUPROFILER_SCOPE(RemoveDisabledAddInstanceOpsValues);
            
            TArray<Ptr<ASTOp>> Roots;
            for(const FStateCompilationData& State : States)
            {
                Roots.Add(State.root);
            }

            TArray<Ptr<ASTOpInstanceAdd>> InstanceAddOpsToRemove;

            const bool bRemoveMeshes = Options->GetPrivate()->OptimisationOptions.bDisableMeshGeneration;
            const bool bRemoveImages = Options->GetPrivate()->OptimisationOptions.bDisableImageGeneration;

            // Gather all InstanceAddOps its value needs to be removed.
            if (bRemoveMeshes || bRemoveImages)
            {
                ASTOp::Traverse_TopDown_Unique_Imprecise(Roots, [&](Ptr<ASTOp> Node)
                //ASTOp::Traverse_TopRandom_Unique_NonReentrant(Roots, [&](Ptr<ASTOp> Node)
                {
			        
                    bool bProcessChildren = GetOpDataType(Node->GetOpType()) == EDataType::Instance;
                    if (!bProcessChildren)
                    {
                        return false;
                    }

                    switch (Node->GetOpType())
                    {

                    case EOpType::IN_ADDMESH:
                    {
                        Ptr<ASTOpInstanceAdd> TypedNode = static_cast<ASTOpInstanceAdd*>(Node.get());
                        if (bRemoveMeshes && TypedNode)
                        {
                            InstanceAddOpsToRemove.Add(TypedNode);
                        }
                        break;
                    }
                    case EOpType::IN_ADDIMAGE:
                    {
                        Ptr<ASTOpInstanceAdd> TypedNode = static_cast<ASTOpInstanceAdd*>(Node.get());
                        if (bRemoveImages && TypedNode)
                        {
                            InstanceAddOpsToRemove.Add(TypedNode);
                        }
                        break;
                    }

                    default:
                        break;
                    }

                    return true;
                });
            }

            // Remove the ops dangling from the InstanceAddOp but keep the InstanceAddOp for simplicity and parity with a full AST.
            // The case of missing value is contemplated in all usages.
            for (Ptr<ASTOpInstanceAdd>& InstanceAddOp : InstanceAddOpsToRemove)
            {
                InstanceAddOp->value = nullptr;
            }
        }
    }

    //---------------------------------------------------------------------------------------------
	TSharedPtr<FModel> Compiler::Compile( const Ptr<Node>& pNode )
    {
        MUTABLE_CPUPROFILER_SCOPE(Compile);


        TArray< FStateCompilationData > States;
        TSharedPtr<FErrorLog> GenErrorLog;
		TArray<FParameterDesc> Parameters;
        {
            CodeGenerator Generator( m_pD->Options->GetPrivate(), m_pD->WaitCallback );

            Generator.GenerateRoot( pNode );

            check( !Generator.States.IsEmpty() );

            for ( const TPair<FObjectState, Ptr<ASTOp>>& s: Generator.States )
            {
                FStateCompilationData Data;
                Data.nodeState = s.Key;
                Data.root = s.Value;
                Data.state.Name = s.Key.Name;
                States.Add( Data );
            }

            GenErrorLog = Generator.ErrorLog;

			{
				UE::TUniqueLock Lock(Generator.FirstPass.ParameterNodes.Mutex);

				// Gather the parameter list from the non-optimized data, so that we have them all even if they are optimized out
				{
					const int32 ParameterCount = 
						Generator.FirstPass.ParameterNodes.GenericParametersCache.Num()
						+
						Generator.FirstPass.ParameterNodes.MeshParametersCache.Num();

					Parameters.Reserve(ParameterCount);
					for (const TPair<Ptr<const Node>, Ptr<ASTOpParameter>>& Entry : Generator.FirstPass.ParameterNodes.GenericParametersCache)
					{
						Parameters.Add(Entry.Value->Parameter);
					}
					for (const TPair<Ptr<const NodeMeshParameter>, TArray<TPair<Ptr<ASTOpParameter>,FMeshGenerationResult>>>& Entry : Generator.FirstPass.ParameterNodes.MeshParametersCache)
					{
						for (const TPair<Ptr<ASTOpParameter>, FMeshGenerationResult>& ArrayEntry : Entry.Value)
						{
							if (ArrayEntry.Key)
							{
								Parameters.AddUnique(ArrayEntry.Key->Parameter);
							}
						}
					}
				}

				// Sort the parameters as deterministically as possible.
				Parameters.StableSort([](const FParameterDesc& A, const FParameterDesc& B)->bool
					{
						if (A.Name < B.Name)
						{
							return true;
						}
						if (A.Name > B.Name)
						{
							return false;
						}
						return A.UID < B.UID;
					});

        		// Check for repeated parameter names and report them to the user
				if (Parameters.Num() > 1)
				{
					TArray<FString> RepeatedParameterNames;
				
					for (int32 Index = 1; Index < Parameters.Num(); ++Index)
					{
						const FString CurrentParameterName = Parameters[Index].Name;
						const FString PreviousParameterName = Parameters[Index-1].Name;
					
						if (CurrentParameterName.Compare(PreviousParameterName) == 0 && !RepeatedParameterNames.Contains(CurrentParameterName))
						{
							// Find the nodes defining these parameters
							TArray<const void*, TInlineAllocator<4>> MessageContexts;
							{
								for (const TPair<Ptr<const Node>, Ptr<ASTOpParameter>>& Entry : Generator.FirstPass.ParameterNodes.GenericParametersCache)
								{
									if (Entry.Value->Parameter.Name.Compare(CurrentParameterName))
									{
										MessageContexts.AddUnique(Entry.Key->GetMessageContext());
									}
								}
								for (const TPair<Ptr<const NodeMeshParameter>, TArray<TPair<Ptr<ASTOpParameter>, FMeshGenerationResult>>>& Entry : Generator.FirstPass.ParameterNodes.MeshParametersCache)
								{
									for (const TPair<Ptr<ASTOpParameter>, FMeshGenerationResult>& ArrayEntry : Entry.Value)
									{
										if (ArrayEntry.Key && ArrayEntry.Key->Parameter.Name.Compare(CurrentParameterName))
										{
											MessageContexts.AddUnique(Entry.Key->GetMessageContext());
										}
									}
								}
							}

							// Report repeated parameter name
							if (MessageContexts.Num() >= 2)
							{
								const FString WarningText = FString::Printf(TEXT("Repeated parameter found : \"%s\". Please use a different name."), *CurrentParameterName);
								GenErrorLog->Add(WarningText, ELMT_WARNING, MessageContexts[0], MessageContexts[1]);
								RepeatedParameterNames.Add(CurrentParameterName);
							}
							else
							{
								ensure(false);
							}
						}
					}
				}
			}
		}

        mu::Private::RemoveDisabledAddInstanceOpsValues(m_pD->Options, States);

        // Optimize the generated code
        {
            CodeOptimiser Optimiser( m_pD->Options, States );
            Optimiser.Optimise();
        }

        // Link the Program and generate state data.
		TSharedPtr<FModel> Result = MakeShared<FModel>();
        FProgram& Program = Result->GetPrivate()->Program;

		check(Program.Parameters.IsEmpty());
		Program.Parameters = Parameters;

		// Preallocate ample memory
		Program.ByteCode.Reserve(16 * 1024 * 1024);
		Program.OpAddress.Reserve(1024 * 1024);

		// Keep the link options outside the scope because it is also used to cache constant data that has already been 
		// added and could be reused across states.
		FImageOperator ImOp = FImageOperator::GetDefault(m_pD->Options->GetPrivate()->ImageFormatFunc);
		FLinkerOptions LinkerOptions(ImOp);

		for(FStateCompilationData& State : States )
        {
			LinkerOptions.MinTextureResidentMipCount = m_pD->Options->GetPrivate()->MinTextureResidentMipCount;

            if (State.root)
            {
				State.state.Root = ASTOp::FullLink(State.root, Program, &LinkerOptions);
            }
            else
            {
                State.state.Root = 0;
            }
        }

		Program.ByteCode.Shrink();
		Program.OpAddress.Shrink();

        // Set the runtime parameter indices.
        for(FStateCompilationData& State : States )
        {
            for ( int32 RuntimeParameterIndex=0; RuntimeParameterIndex <State.nodeState.RuntimeParams.Num(); ++RuntimeParameterIndex)
            {
                int32 ParameterIndex = -1;
                for ( int32 i=0; ParameterIndex<0 && i<Program.Parameters.Num(); ++i )
                {
                    if ( Program.Parameters[i].Name
                         ==
                         State.nodeState.RuntimeParams[RuntimeParameterIndex] )
                    {
                        ParameterIndex = (int)i;
                    }
                }

                if (ParameterIndex>=0)
                {
                    State.state.m_runtimeParameters.Add( ParameterIndex );
                }
                else
                {
					FString WarningString = FString::Printf(TEXT(
						"The state [%s] refers to a parameter [%s]  that has not been found in the model. This error can be "
						"safely dismissed in case of partial compilation."), 
						*State.nodeState.Name,
						*State.nodeState.RuntimeParams[RuntimeParameterIndex]);
                    m_pD->ErrorLog->Add(WarningString, ELMT_WARNING, pNode->GetMessageContext() );
                }
            }

            // Generate the mask of update cache ops
            for (const Ptr<ASTOp>& Instruction : State.m_updateCache )
            {
                State.state.m_updateCache.Emplace(Instruction->linkedAddress);
            }

            // Sort the update cache addresses for performance and determinism
            State.state.m_updateCache.Sort();

            // Generate the mask of dynamic resources
            for (const TPair<Ptr<ASTOp>, TArray<FString>>& Instruction : State.m_dynamicResources )
            {
                uint64 RelevantMask = 0;
                for (const FString& InstructionParameter : Instruction.Value )
                {
                    // Find the index in the model parameter list
                    int ParameterIndex = -1;
                    for ( int32 i=0; ParameterIndex<0 && i<Program.Parameters.Num(); ++i )
                    {
                        if ( Program.Parameters[i].Name == InstructionParameter )
                        {
                            ParameterIndex = (int)i;
                        }
                    }
                    check(ParameterIndex>=0);

                    // Find the position in the state data vector.
                    const int32 IndexInRuntimeList = State.state.m_runtimeParameters.Find( ParameterIndex );

                    if (IndexInRuntimeList !=INDEX_NONE )
                    {
                        RelevantMask |= uint64(1) << IndexInRuntimeList;
                    }
                }

                // TODO: this shouldn't happen but it seems to happen. Investigate.
                // Maybe something with the difference of precision between the relevant parameters
                // in operation subtrees.
                //check(relevantMask!=0);
                if (RelevantMask!=0)
                {
                    State.state.m_dynamicResources.Emplace( Instruction.Key->linkedAddress, RelevantMask );
                }
            }            

            // Sort for performance and determinism
            State.state.m_dynamicResources.Sort();

            Program.States.Add(State.state);
        }

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("Program size"), int64(Program.OpAddress.Num()));

        // Merge the log in the right order
        GenErrorLog->Merge( m_pD->ErrorLog.Get() );
        m_pD->ErrorLog = GenErrorLog;

		// Pack data
		m_pD->GenerateRoms(Result.Get(), LinkerOptions.AdditionalData );

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("Program size"), int64(Program.OpAddress.Num()));

		// We are not touching the program anymore. Ensure we are not wasting memory.
		Program.Roms.Shrink();
		Program.ConstantImageLODsPermanent.Shrink();
		Program.ConstantImageLODIndices.Shrink();
		Program.ConstantImages.Shrink();
		Program.ConstantMeshesPermanent.Shrink();
		Program.ConstantStrings.Shrink();
		Program.ConstantSkeletons.Shrink();
		Program.ConstantPhysicsBodies.Shrink();
		Program.Parameters.Shrink();

        return Result;
    }


    //---------------------------------------------------------------------------------------------
	TSharedPtr<FErrorLog> Compiler::GetLog() const
    {
        return m_pD->ErrorLog;
    }


	//---------------------------------------------------------------------------------------------
	void Compiler::Private::GenerateRoms(FModel* Model, const FLinkerOptions::FAdditionalData& AdditionalData )
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(GenerateRoms);

		uint64 EmbeddedDataBytesLimit = Options->GetPrivate()->EmbeddedDataBytesLimit;

		mu::FProgram& Program = Model->GetPrivate()->Program;

		// These are used for logging only.
		int32 NumRoms = 0;
		uint64 NumRomsBytes = 0;
		int32 NumEmbedded = 0;
		int32 NumEmbeddedBytes = 0;
		int32 NumHighRes = 0;
		uint64 NumHighResBytes = 0;

		// Maximum number of roms
		int32 MaxRomCount = Program.ConstantImageLODsPermanent.Num() + Program.ConstantMeshesPermanent.Num();
		Program.Roms.Reserve(MaxRomCount);

		TSet<uint32> UsedIds;
		UsedIds.Reserve(MaxRomCount);

		int32 MaxResources = FMath::Max(Program.ConstantImageLODsPermanent.Num(), Program.ConstantMeshesPermanent.Num());
		TArray<FRomDataRuntime> RomDatas;
		RomDatas.SetNumZeroed(MaxResources);
		TArray<FRomDataCompile> RomDatasCompile;
		RomDatasCompile.SetNumZeroed(MaxResources);

		// Images
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateRoms_ImageIds);

			TArray<TSharedPtr<const FImage>> AllMips = MoveTemp(Program.ConstantImageLODsPermanent);
			Program.ConstantImageLODsPermanent = {};
			Program.ConstantImageLODsPermanent.Reserve(AllMips.Num());


			ParallelFor(AllMips.Num(),
				[EmbeddedDataBytesLimit, &Program, &AllMips, &RomDatas](uint32 MipIndex)
				{
					TSharedPtr<const FImage>& Resource = AllMips[MipIndex];

					// Serialize to find out final size of this rom
					FOutputSizeStream SizeStream;
					FOutputArchive MemoryArch(&SizeStream);
					FImage::Serialise(Resource.Get(), MemoryArch);


					FRomDataRuntime& RomData = RomDatas[MipIndex];
					RomData.ResourceType = uint32(ERomDataType::Image);
					uint32 Size = SizeStream.GetBufferSize();
					check(Size < uint32(1 << 30));
					RomData.Size = Size;
				});

			// Generate the high-res flags for images
			{
				// Initially all are high-res because if at least one reference to a mip is not we will set it to not-high-res
				TArray<bool> IsLODHighRes;
				IsLODHighRes.Init(true, AllMips.Num());

				for (int32 ImageIndex = 0; ImageIndex < Program.ConstantImages.Num(); ++ImageIndex)
				{
					const FImageLODRange& LODRange = Program.ConstantImages[ImageIndex];

	                int32 OptionalMaxLODSize = AdditionalData.SourceImagePerConstant[ImageIndex].OptionalMaxLODSize;
                    int32 OptionalLODBias = AdditionalData.SourceImagePerConstant[ImageIndex].OptionalLODBias; 
                    int32 NumNonOptionalLODs = AdditionalData.SourceImagePerConstant[ImageIndex].NumNonOptionalLODs; 

                    int32 NumOptionalMips = 0; 
                    if (OptionalMaxLODSize > 0)
                    {
                        int32 NumTotalLODs = FMath::CeilLogTwo(FMath::Max(LODRange.ImageSizeX, LODRange.ImageSizeY)) + 1;
                        int32 FirstOptionalLOD = FMath::Min<int32>(FMath::CeilLogTwo(OptionalMaxLODSize) + 1, NumTotalLODs);

                        NumOptionalMips = FMath::Min<int32>(
                                NumTotalLODs - (FirstOptionalLOD - OptionalLODBias), FMath::Max<int32>(0, NumTotalLODs - NumNonOptionalLODs));
                    }

					for (int32 LODRangeIndex = NumOptionalMips; LODRangeIndex < LODRange.LODCount; ++LODRangeIndex)
					{
						FConstantResourceIndex LODIndex = Program.ConstantImageLODIndices[LODRange.FirstIndex + LODRangeIndex];
						check(!LODIndex.Streamable); // Not classified yet
						IsLODHighRes[LODIndex.Index] = false;
					}

					// Moreover, at least one mip of each image has to be non-highres
					if (LODRange.LODCount > 0)
					{
						FConstantResourceIndex LastLODIndex = Program.ConstantImageLODIndices[LODRange.FirstIndex + LODRange.LODCount - 1];
						check(!LastLODIndex.Streamable); // Not classified yet
						IsLODHighRes[LastLODIndex.Index] = false;
					}
				}

				for (int32 MipIndex = 0; MipIndex < AllMips.Num(); ++MipIndex)
				{
					// If this mip represents a high-quality mip, flag the rom as such
					if (IsLODHighRes[MipIndex])
					{
						FRomDataRuntime& RomData = RomDatas[MipIndex];
						RomData.IsHighRes = true;

						++NumHighRes;
						NumHighResBytes += RomData.Size;
					}
				}
			}

			{
				MUTABLE_CPUPROFILER_SCOPE(GenerateRoms_ImageSourceIds);

				for (int32 ImageIndex = 0; ImageIndex < Program.ConstantImages.Num(); ++ImageIndex)
				{
					const FImageLODRange& LODRange = Program.ConstantImages[ImageIndex];

					uint32 SourceId = AdditionalData.SourceImagePerConstant[ImageIndex].SourceId;

					for (int32 LODRangeIndex = 0; LODRangeIndex < LODRange.LODCount; ++LODRangeIndex)
					{
						FConstantResourceIndex ResourceIndex = Program.ConstantImageLODIndices[LODRange.FirstIndex + LODRangeIndex];
						check(!ResourceIndex.Streamable); // Not classified yet
						RomDatasCompile[ResourceIndex.Index].SourceId = SourceId;
					}
				}
			}

			// Split the data in permanent and streamable and assign final FConstantResourceIndex
			{
				MUTABLE_CPUPROFILER_SCOPE(GenerateRoms_ImageSplit);

				TArray<FConstantResourceIndex> IndexPerMip;
				IndexPerMip.SetNumUninitialized(AllMips.Num());

				for (int32 MipIndex = 0; MipIndex < AllMips.Num(); ++MipIndex)
				{
					FRomDataRuntime& RomData = RomDatas[MipIndex];
					if (RomData.Size > EmbeddedDataBytesLimit)
					{
						NumRoms++;
						NumRomsBytes += RomData.Size;

						uint32 RomIndex = Program.Roms.Num();
						check(RomIndex < 0x7fffffff);

						IndexPerMip[MipIndex] = {RomIndex, 1};
						Program.ConstantImageLODsStreamed.Add(RomIndex, AllMips[MipIndex]);

						Program.Roms.Add(RomData);

						FRomDataCompile& RomDataCompile = RomDatasCompile[MipIndex];
						Program.RomsCompileData.Add(RomDataCompile);
					}
					else
					{
						uint32 Index = uint32(Program.ConstantImageLODsPermanent.Num());
						check(Index < 0x7fffffff);
						IndexPerMip[MipIndex] = { Index, 0 };
						Program.ConstantImageLODsPermanent.Add(AllMips[MipIndex]);

						NumEmbedded++;
						NumEmbeddedBytes += RomData.Size;
					}
				}

				for (int32 Index = 0; Index < Program.ConstantImageLODIndices.Num(); ++Index)
				{
					int32 RomDataIndex = Program.ConstantImageLODIndices[Index].Index;
					Program.ConstantImageLODIndices[Index] = IndexPerMip[RomDataIndex];
				}
			}
		}

		// Meshes
		TArray<TSharedPtr<const FMesh>> AllMeshes = MoveTemp(Program.ConstantMeshesPermanent);
		Program.ConstantMeshesPermanent = {};
		Program.ConstantMeshesPermanent.Reserve(AllMeshes.Num());

		FMemory::Memzero( RomDatas.GetData(), RomDatas.GetAllocatedSize() );
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateRoms_ImageSourceIds);

			for (int32 MeshIndex = 0; MeshIndex < Program.ConstantMeshes.Num(); ++MeshIndex)
			{
				const FMeshContentRange& MeshContentRange = Program.ConstantMeshes[MeshIndex];

				const uint32 SourceId = AdditionalData.SourceMeshPerConstant[MeshIndex].SourceId;

				const int32 NumMeshContentElements = 
						FMath::CountBits(static_cast<uint64>(MeshContentRange.GetContentFlags()));

				for (int32 MeshContentIndex = 0; MeshContentIndex < NumMeshContentElements; ++MeshContentIndex)
				{
					FConstantResourceIndex ResourceIndex = 
							Program.ConstantMeshContentIndices[MeshContentRange.GetFirstIndex() + MeshContentIndex];

					check(!ResourceIndex.Streamable); // Not classified yet
					RomDatasCompile[ResourceIndex.Index].SourceId = SourceId;
				}
			}
		}

		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateRoms_MeshIds);

			ParallelFor(AllMeshes.Num(),
				[EmbeddedDataBytesLimit, &AllMeshes , &Program, &RomDatas](uint32 ResourceIndex)
				{
					TSharedPtr<const FMesh>& ResData = AllMeshes[ResourceIndex];

					// Serialize to memory, to find out final size of this rom
					const FMesh* Resource = ResData.Get();

					FOutputSizeStream MemStream;
					FOutputArchive MemoryArch(&MemStream);
					FMesh::Serialise(ResData.Get(), MemoryArch);

					FRomDataRuntime& RomData = RomDatas[ResourceIndex];
					RomData.ResourceType = uint32(ERomDataType::Mesh);
					uint32 Size = MemStream.GetBufferSize();
					check(Size < uint32(1 << 30));
					RomData.Size = Size;
				});
		}

		// Split the data in permanent and streamable and assign final FConstantResourceIndex
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateRoms_MeshSplit);

			TArray<FConstantResourceIndex> IndexPerMeshContent;
			IndexPerMeshContent.SetNumUninitialized(AllMeshes.Num());
			
			for (int32 MeshIndex = 0; MeshIndex < AllMeshes.Num(); ++MeshIndex)
			{
				FRomDataRuntime& RomData = RomDatas[MeshIndex];
				FRomDataCompile& RomDataCompile = RomDatasCompile[MeshIndex];

				if (RomData.Size > EmbeddedDataBytesLimit)
				{
					NumRoms++;
					NumRomsBytes += RomData.Size;

					uint32 RomIndex = Program.Roms.Num();
					check(RomIndex < 0x7fffffff);

					IndexPerMeshContent[MeshIndex] = { RomIndex, 1 };
					Program.ConstantMeshesStreamed.Add(RomIndex, AllMeshes[MeshIndex]);

					Program.Roms.Add(RomData);
					Program.RomsCompileData.Add(RomDataCompile);
				}
				else
				{
					uint32 Index = uint32(Program.ConstantMeshesPermanent.Num());
					check(Index < 0x7fffffff);

					IndexPerMeshContent[MeshIndex] = { Index, 0 };
					Program.ConstantMeshesPermanent.Add(AllMeshes[MeshIndex]);

					NumEmbedded++;
					NumEmbeddedBytes += RomData.Size;
				}
			}

			for (int32 Index = 0; Index < Program.ConstantMeshContentIndices.Num(); ++Index)
			{
				int32 RomDataIndex = Program.ConstantMeshContentIndices[Index].Index;
				Program.ConstantMeshContentIndices[Index] = IndexPerMeshContent[RomDataIndex];
			}
		}

		UE_LOG(LogMutableCore, Log, TEXT("Generated roms: %d (%d KB) are embedded, %d (%d KB) are streamed of which %d (%d KB) are high-res."), 
			NumEmbedded, uint32(NumEmbeddedBytes/1024), NumRoms, uint32(NumRomsBytes/1024), NumHighRes, uint32(NumHighResBytes/1024));
	}


}
