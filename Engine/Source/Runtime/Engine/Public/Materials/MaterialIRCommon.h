// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SceneTypes.h"
#include "RHIShaderPlatform.h"
#include "Containers/Array.h"
#include "Math/MathFwd.h"

#if WITH_EDITOR

/* Forward declarations */

class FMaterial;
class FMaterialIRModule;
class ITargetPlatform;
class UMaterial;
class UMaterialExpression;
class UMaterialExpressionMaterialFunctionCall;
class UTexture;
enum EMaterialProperty : int;
struct FExpressionInput;
struct FExpressionOutput;
struct FMaterialInputDescription;
struct FMaterialInsights;
struct FMaterialIRModuleBuilder;
struct FMaterialIRModuleBuilderImpl;
struct FShaderCompilerEnvironment;
struct FStaticParameterSet;

namespace UE::Shader
{
	struct FValue;
}

namespace ERHIFeatureLevel { enum Type : int; }

namespace MIR
{

/* Types*/
struct FType;
struct FPrimitiveType;
struct FObjectType;

/* IR */
struct FValue;
struct FInstruction;
struct FSetMaterialOutput;
enum class EExternalInput;

/* Others */
class FEmitter;
struct FBlock;

namespace Internal
{
	uint32 HashBytes(const char* Ptr, uint32 Size);
}

}

#define UE_MIR_UNREACHABLE() { check(!"Unreachable"); UE_ASSUME(false); }
#define UE_MIR_TODO() UE_MIR_UNREACHABLE()

#endif // #if WITH_EDITOR
