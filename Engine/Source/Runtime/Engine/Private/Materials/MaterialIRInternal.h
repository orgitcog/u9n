// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "MaterialShared.h"

#if WITH_EDITOR

template <typename T>
TArrayView<T> MakeTemporaryArray(FMemMark&, int Count)
{
	auto Ptr = (T*)FMemStack::Get().Alloc(sizeof(T) * Count, alignof(T));
	return { Ptr, Count };
}

namespace MIR::Internal {

//
bool IsMaterialPropertyEnabled(EMaterialProperty InProperty);

//
bool NextMaterialAttributeInput(UMaterial* BaseMaterial, int32& PropertyIndex);

//
FValue* CreateMaterialAttributeDefaultValue(FEmitter& Emitter, const UMaterial* Material, EMaterialProperty Property);
 
//
EMaterialTextureParameterType TextureMaterialValueTypeToParameterType(EMaterialValueType Type);

// Returns the value flowing into given expression input (previously set through `BindValueToExpressionInput`).
FValue* FetchValueFromExpressionInput(FMaterialIRModuleBuilderImpl* Builder, const FExpressionInput* Input);

// Flows a value into given expression input.
void BindValueToExpressionInput(FMaterialIRModuleBuilderImpl* Builder, const FExpressionInput* Input, FValue* Value);

// Flows a value int given expression output.
void BindValueToExpressionOutput(FMaterialIRModuleBuilderImpl* Builder, const FExpressionOutput* Output, FValue* Value);

//
uint32 HashBytes(const char* Ptr, uint32 Size);

/* Other helper functions */

template <typename TKey, typename TValue>
bool Find(const TMap<TKey, TValue>& Map, const TKey& Key, TValue& OutValue)
{
	if (auto ValuePtr = Map.Find(Key)) {
		OutValue = *ValuePtr;
		return true;
	}
	return false;
}

} // namespace MIR::Internal

#endif // #if WITH_EDITOR

