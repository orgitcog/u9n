// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRInternal.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialIREmitter.h"
#include "Materials/Material.h"
#include "MaterialShared.h"

#if WITH_EDITOR

namespace MIR::Internal {

bool IsMaterialPropertyEnabled(EMaterialProperty InProperty)
{
	switch (InProperty)
	{
		case MP_Normal:
		case MP_Tangent:
		case MP_EmissiveColor:
		case MP_Opacity:
		case MP_OpacityMask:
		case MP_BaseColor:
		case MP_Metallic:
		case MP_Specular:
		case MP_Roughness:
		case MP_Anisotropy:
		case MP_AmbientOcclusion:
		case MP_Refraction:
		case MP_PixelDepthOffset:
		case MP_SubsurfaceColor:
		case MP_ShadingModel:
		case MP_SurfaceThickness:
		case MP_FrontMaterial:
		case MP_Displacement:
		case MP_WorldPositionOffset:
		case MP_CustomData0:
		case MP_CustomData1:
			return true;
		default:
			return false;
	}
}

bool NextMaterialAttributeInput(UMaterial* BaseMaterial, int32& PropertyIndex)
{
	for (; PropertyIndex < MP_MAX; ++PropertyIndex)
	{
		EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
		if (MIR::Internal::IsMaterialPropertyEnabled(Property) 
			&& Property != MP_SubsurfaceColor
			&& Property != MP_FrontMaterial)
		{
			return true;
		}
	}

	return false;
}

MIR::FValue* CreateMaterialAttributeDefaultValue(FEmitter& Emitter, const UMaterial* Material, EMaterialProperty Property)
{
	EMaterialValueType Type = FMaterialAttributeDefinitionMap::GetValueType(Property);
	FVector4f DefaultValue = FMaterialAttributeDefinitionMap::GetDefaultValue(Property);

	switch (Type)
	{
		case MCT_ShadingModel: return Emitter.ConstantInt(Material->GetShadingModels().GetFirstShadingModel());

		case MCT_Float1: return Emitter.ConstantFloat(  DefaultValue.X );
		case MCT_Float2: return Emitter.ConstantFloat2({ DefaultValue.X, DefaultValue.Y });
		case MCT_Float3: return Emitter.ConstantFloat3({ DefaultValue.X, DefaultValue.Y, DefaultValue.Z });
		case MCT_Float:
		case MCT_Float4: return Emitter.ConstantFloat4(DefaultValue);

		case MCT_UInt1: return Emitter.ConstantInt((TInteger)DefaultValue.X);
		case MCT_UInt2: return Emitter.ConstantInt2({ (TInteger)DefaultValue.X, (TInteger)DefaultValue.Y });
		case MCT_UInt3: return Emitter.ConstantInt3({ (TInteger)DefaultValue.X, (TInteger)DefaultValue.Y, (TInteger)DefaultValue.Z });
		case MCT_UInt:
		case MCT_UInt4: return Emitter.ConstantInt4({ (TInteger)DefaultValue.X, (TInteger)DefaultValue.Y, (TInteger)DefaultValue.Z, (TInteger)DefaultValue.W });

		default: UE_MIR_UNREACHABLE();
	}
}

EMaterialTextureParameterType TextureMaterialValueTypeToParameterType(EMaterialValueType Type)
{
	switch (Type)
	{
		case MCT_Texture2D: return EMaterialTextureParameterType::Standard2D;
		case MCT_Texture2DArray: return EMaterialTextureParameterType::Array2D;
		case MCT_TextureCube: return EMaterialTextureParameterType::Cube;
		case MCT_TextureCubeArray: return EMaterialTextureParameterType::ArrayCube;
		case MCT_VolumeTexture: return EMaterialTextureParameterType::Volume;
		default: UE_MIR_UNREACHABLE();
	}
}

uint32 HashBytes(const char* Ptr, uint32 Size)
{
	uint32 Hash = 0;

	const uint32* UPtr = (const uint32*)Ptr;
	uint32 i = 0; 
	for (uint32 SizeIn32Bits = Size / 4; i < SizeIn32Bits; ++i)
	{
		Hash = HashCombineFast(Hash, UPtr[i]);
	}

	uint32 Modulo = Size % 4;
	if (Modulo == 1)
	{
		Hash = HashCombineFast(Hash, UPtr[i] & 0xff000000);
	}
	else if (Modulo == 2)
	{
		Hash = HashCombineFast(Hash, UPtr[i] & 0xffff0000);
	}
	else if (Modulo == 3)
	{
		Hash = HashCombineFast(Hash, UPtr[i] & 0xffffff00);
	}

	return Hash;
}

} // namespace MIR::Internal

#endif
