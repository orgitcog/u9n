// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

namespace MIR
{

const TCHAR* TypeKindToString(ETypeKind Kind)
{
	switch (Kind)
	{
		case TypeKind_Void: return TEXT("void");
		case TypeKind_Primitive: return TEXT("primitive");
		default: UE_MIR_UNREACHABLE();
	}
}

const FType* FType::FromShaderType(const UE::Shader::FType& InShaderType)
{
	check(!InShaderType.IsStruct());
	check(!InShaderType.IsObject());

	switch (InShaderType.ValueType)
	{
		case UE::Shader::EValueType::Void:
			return FType::GetVoid();

		case UE::Shader::EValueType::Float1:
		case UE::Shader::EValueType::Float2:
		case UE::Shader::EValueType::Float3:
		case UE::Shader::EValueType::Float4:
			return FPrimitiveType::GetVector(ScalarKind_Float, (int)InShaderType.ValueType - (int)UE::Shader::EValueType::Float1 + 1);

		case UE::Shader::EValueType::Int1:
		case UE::Shader::EValueType::Int2:
		case UE::Shader::EValueType::Int3:
		case UE::Shader::EValueType::Int4:
			return FPrimitiveType::GetVector(ScalarKind_Int, (int)InShaderType.ValueType - (int)UE::Shader::EValueType::Int1 + 1);

		case UE::Shader::EValueType::Bool1:
		case UE::Shader::EValueType::Bool2:
		case UE::Shader::EValueType::Bool3:
		case UE::Shader::EValueType::Bool4:
			return FPrimitiveType::GetVector(ScalarKind_Bool, (int)InShaderType.ValueType - (int)UE::Shader::EValueType::Int1 + 1);

		default:
			UE_MIR_UNREACHABLE();
	}
}

const FType* FType::FromMaterialValueType(EMaterialValueType Type)
{
	switch (Type)
	{
		case MCT_VoidStatement:
			return FType::GetVoid();

		case MCT_Float1:
			return FPrimitiveType::GetVector(ScalarKind_Float, 1);
		case MCT_Float2:
			return FPrimitiveType::GetVector(ScalarKind_Float, 2);
		case MCT_Float3:
			return FPrimitiveType::GetVector(ScalarKind_Float, 3);
		case MCT_Float4:
			return FPrimitiveType::GetVector(ScalarKind_Float, 4);

		case MCT_Float:
			return FPrimitiveType::GetVector(ScalarKind_Float, 4);

		case MCT_UInt1:
			return FPrimitiveType::GetVector(ScalarKind_Int, 1);
		case MCT_UInt2:
			return FPrimitiveType::GetVector(ScalarKind_Int, 2);
		case MCT_UInt3:
			return FPrimitiveType::GetVector(ScalarKind_Int, 3);
		case MCT_UInt4:
			return FPrimitiveType::GetVector(ScalarKind_Int, 4);

		case MCT_Bool:
			return FPrimitiveType::GetVector(ScalarKind_Bool, 1);

		default:
			UE_MIR_UNREACHABLE();
	}
}

const FType* FType::FromMaterialParameterType(EMaterialParameterType Type)
{
	switch (Type)
	{
		case EMaterialParameterType::Scalar: return FPrimitiveType::GetFloat();
		case EMaterialParameterType::Vector: return FPrimitiveType::GetFloat4();
		case EMaterialParameterType::DoubleVector: UE_MIR_TODO();
		case EMaterialParameterType::Texture: return FObjectType::GetTexture2D();
		case EMaterialParameterType::TextureCollection: UE_MIR_TODO();
		case EMaterialParameterType::Font: UE_MIR_TODO();
		case EMaterialParameterType::RuntimeVirtualTexture: UE_MIR_TODO();
		case EMaterialParameterType::SparseVolumeTexture: UE_MIR_TODO();
		case EMaterialParameterType::StaticSwitch: return FPrimitiveType::GetBool();
		default: UE_MIR_UNREACHABLE();
	}
}

const FType* FType::GetVoid()
{
	static FType Type{ TypeKind_Void };
	return &Type;
}

FStringView FType::GetSpelling() const
{
	if (IsPoison())
	{
		return TEXT("Poison");
	}
	else if (auto PrimitiveType = AsPrimitive())
	{
		return PrimitiveType->Spelling;
	}
	else if (auto ObjectType = AsObject())
	{
		switch (ObjectType->ObjectKind)
		{
			case ObjectKind_Texture2D: return TEXT("Texture2D");
		}
	}
	UE_MIR_UNREACHABLE();
}

UE::Shader::EValueType FType::ToValueType() const
{
	using namespace UE::Shader;

	if (const FPrimitiveType* PrimitiveType = AsPrimitive())
	{
		if (PrimitiveType->IsMatrix())
		{
			if (PrimitiveType->NumRows == 4 && PrimitiveType->NumColumns == 4)
			{
				if (PrimitiveType->ScalarKind == ScalarKind_Float)
				{
					return EValueType::Float4x4;
				}
				else
				{
					return EValueType::Numeric4x4;
				}
			}

			return EValueType::Any;
		}

		check(PrimitiveType->NumColumns == 1 && PrimitiveType->NumRows <= 4);

		switch (PrimitiveType->ScalarKind)
		{
			case ScalarKind_Bool: 	return (EValueType)((int)EValueType::Bool1 + PrimitiveType->NumRows - 1);
			case ScalarKind_Int: 	return (EValueType)((int)EValueType::Int1 + PrimitiveType->NumRows - 1);
			case ScalarKind_Float: 	return (EValueType)((int)EValueType::Float1 + PrimitiveType->NumRows - 1);
			default: UE_MIR_UNREACHABLE();
		}
	}
	else if (const FObjectType* ObjectType = AsObject())
	{
		return EValueType::Object;
	}
	
	UE_MIR_UNREACHABLE();
}

const FType* FType::GetPoison()
{
	static FType Type{ TypeKind_Poison };
	return &Type;
}

bool FType::IsPoison() const
{
	return Kind == TypeKind_Poison;
}

bool FType::IsBoolean() const
{
	const FPrimitiveType* Ptr = AsPrimitive();
	return Ptr && Ptr->ScalarKind == ScalarKind_Bool;
}

bool FType::IsInteger() const
{
	const FPrimitiveType* Ptr = AsPrimitive();
	return Ptr && Ptr->ScalarKind == ScalarKind_Int;
}

bool FType::IsBoolScalar() const
{
	return this == FPrimitiveType::GetBool();
}

bool FType::IsObjectOfKind(EObjectKind ObjectKind) const
{
	if (const FObjectType* ObjectPtr = AsObject())
	{
		return ObjectPtr->ObjectKind == ObjectKind;
	}
	return false;
}

bool FType::IsTexture() const
{
	if (const FObjectType* ObjectPtr = AsObject())
	{
		return true;
	}
	return false; 
}

const FPrimitiveType* FType::AsPrimitive() const
{
	return Kind == TypeKind_Primitive ? static_cast<const FPrimitiveType*>(this) : nullptr; 
}

const FPrimitiveType* FType::AsArithmetic() const
{
	return Kind == TypeKind_Primitive && static_cast<const FPrimitiveType*>(this)->IsArithmetic()
		? static_cast<const FPrimitiveType*>(this)
		: nullptr; 
}

const FObjectType* FType::AsObject() const
{
	return Kind == TypeKind_Object ? static_cast<const FObjectType*>(this) : nullptr; 
}

const FPrimitiveType* FType::AsScalar() const
{
	const FPrimitiveType* Type = AsPrimitive();
	return Type->IsScalar() ? Type : nullptr;
}

const FPrimitiveType* FType::AsVector() const
{
	const FPrimitiveType* Type = AsPrimitive();
	return Type->IsVector() ? Type : nullptr;
}

const FPrimitiveType* FType::AsMatrix() const
{
	const FPrimitiveType* Type = AsPrimitive();
	return Type->IsMatrix() ? Type : nullptr;
}

bool ScalarKindIsArithmetic(EScalarKind Kind)
{
	return Kind != ScalarKind_Bool;
}

bool ScalarKindIsAnyFloat(EScalarKind Kind)
{
	return Kind == ScalarKind_Float;
}

const TCHAR* ScalarKindToString(EScalarKind Kind)
{
	switch (Kind)
	{
		case ScalarKind_Bool: return TEXT("bool");
		case ScalarKind_Int: return TEXT("int");
		case ScalarKind_Float: return TEXT("MaterialFloat");
		default: UE_MIR_UNREACHABLE();
	}
}

const FPrimitiveType* FPrimitiveType::GetBool()
{
	return GetScalar(ScalarKind_Bool);
}

const FPrimitiveType* FPrimitiveType::GetInt()
{
	return GetScalar(ScalarKind_Int);
}

const FPrimitiveType* FPrimitiveType::GetFloat()
{
	return GetScalar(ScalarKind_Float);
}

const FPrimitiveType* FPrimitiveType::GetFloat2()
{
	return GetVector(ScalarKind_Float, 2);
}

const FPrimitiveType* FPrimitiveType::GetFloat3()
{
	return GetVector(ScalarKind_Float, 3);
}

const FPrimitiveType* FPrimitiveType::GetFloat4()
{
	return GetVector(ScalarKind_Float, 4);
}

const FPrimitiveType* FPrimitiveType::GetScalar(EScalarKind InScalarKind)
{
	return Get(InScalarKind, 1, 1);
}

const FPrimitiveType* FPrimitiveType::GetVector(EScalarKind InScalarKind, int NumRows)
{
	check(NumRows >= 1 && NumRows <= 4);
	return Get(InScalarKind, NumRows, 1);
}

const FPrimitiveType* FPrimitiveType::Get(EScalarKind InScalarKind, int NumRows, int NumColumns)
{
	check(InScalarKind >= 0 && InScalarKind <= ScalarKind_Float);
	
	static const FStringView Invalid = TEXT("invalid");

	static const FPrimitiveType Types[] {
		{ { TypeKind_Primitive }, { TEXT("bool") }, 		ScalarKind_Bool, 1, 1 },
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Bool, 1, 2 }, 
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Bool, 1, 3 },
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Bool, 1, 4 },
		{ { TypeKind_Primitive }, { TEXT("bool2") },   	ScalarKind_Bool, 2, 1 },
		{ { TypeKind_Primitive }, { TEXT("bool2x2") }, 	ScalarKind_Bool, 2, 2 },
		{ { TypeKind_Primitive }, { TEXT("bool2x3") }, 	ScalarKind_Bool, 2, 3 },
		{ { TypeKind_Primitive }, { TEXT("bool2x4") }, 	ScalarKind_Bool, 2, 4 },
		{ { TypeKind_Primitive }, { TEXT("bool3") },   	ScalarKind_Bool, 3, 1 },
		{ { TypeKind_Primitive }, { TEXT("bool3x2") }, 	ScalarKind_Bool, 3, 2 },
		{ { TypeKind_Primitive }, { TEXT("bool3x3") }, 	ScalarKind_Bool, 3, 3 },
		{ { TypeKind_Primitive }, { TEXT("bool3x4") }, 	ScalarKind_Bool, 3, 4 },
		{ { TypeKind_Primitive }, { TEXT("bool4") },   	ScalarKind_Bool, 4, 1 },
		{ { TypeKind_Primitive }, { TEXT("bool4x2") }, 	ScalarKind_Bool, 4, 2 },
		{ { TypeKind_Primitive }, { TEXT("bool4x3") }, 	ScalarKind_Bool, 4, 3 },
		{ { TypeKind_Primitive }, { TEXT("bool4x4") }, 	ScalarKind_Bool, 4, 4 },
		{ { TypeKind_Primitive }, { TEXT("int") }, 		ScalarKind_Int, 1, 1 },
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Int, 1, 2 },
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Int, 1, 3 },
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Int, 1, 4 },
		{ { TypeKind_Primitive }, { TEXT("int2") },   	ScalarKind_Int, 2, 1 },
		{ { TypeKind_Primitive }, { TEXT("int2x2") }, 	ScalarKind_Int, 2, 2 },
		{ { TypeKind_Primitive }, { TEXT("int2x3") }, 	ScalarKind_Int, 2, 3 },
		{ { TypeKind_Primitive }, { TEXT("int2x4") }, 	ScalarKind_Int, 2, 4 },
		{ { TypeKind_Primitive }, { TEXT("int3") },   	ScalarKind_Int, 3, 1 },
		{ { TypeKind_Primitive }, { TEXT("int3x2") }, 	ScalarKind_Int, 3, 2 },
		{ { TypeKind_Primitive }, { TEXT("int3x3") }, 	ScalarKind_Int, 3, 3 },
		{ { TypeKind_Primitive }, { TEXT("int3x4") }, 	ScalarKind_Int, 3, 4 },
		{ { TypeKind_Primitive }, { TEXT("int4") },   	ScalarKind_Int, 4, 1 },
		{ { TypeKind_Primitive }, { TEXT("int4x2") }, 	ScalarKind_Int, 4, 2 },
		{ { TypeKind_Primitive }, { TEXT("int4x3") }, 	ScalarKind_Int, 4, 3 },
		{ { TypeKind_Primitive }, { TEXT("int4x4") }, 	ScalarKind_Int, 4, 4 },
		{ { TypeKind_Primitive }, { TEXT("float") }, 		ScalarKind_Float, 1, 1 },
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Float, 1, 2 },
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Float, 1, 3 },
		{ { TypeKind_Primitive }, Invalid, 				ScalarKind_Float, 1, 4 },
		{ { TypeKind_Primitive }, { TEXT("float2") },   	ScalarKind_Float, 2, 1 },
		{ { TypeKind_Primitive }, { TEXT("float2x2") }, 	ScalarKind_Float, 2, 2 },
		{ { TypeKind_Primitive }, { TEXT("float2x3") }, 	ScalarKind_Float, 2, 3 },
		{ { TypeKind_Primitive }, { TEXT("float2x4") }, 	ScalarKind_Float, 2, 4 },
		{ { TypeKind_Primitive }, { TEXT("float3") },   	ScalarKind_Float, 3, 1 },
		{ { TypeKind_Primitive }, { TEXT("float3x2") }, 	ScalarKind_Float, 3, 2 },
		{ { TypeKind_Primitive }, { TEXT("float3x3") }, 	ScalarKind_Float, 3, 3 },
		{ { TypeKind_Primitive }, { TEXT("float3x4") }, 	ScalarKind_Float, 3, 4 },
		{ { TypeKind_Primitive }, { TEXT("float4") },   	ScalarKind_Float, 4, 1 },
		{ { TypeKind_Primitive }, { TEXT("float4x2") }, 	ScalarKind_Float, 4, 2 },
		{ { TypeKind_Primitive }, { TEXT("float4x3") }, 	ScalarKind_Float, 4, 3 },
		{ { TypeKind_Primitive }, { TEXT("float4x4") }, 	ScalarKind_Float, 4, 4 },
	};

	int Index = InScalarKind * 4 * 4 + (NumRows - 1) * 4 + (NumColumns - 1);
	check(Index < UE_ARRAY_COUNT(Types));
	return &Types[Index];
}

const FPrimitiveType* FPrimitiveType::ToScalar() const
{
	return FPrimitiveType::GetScalar(ScalarKind);
}

const FPrimitiveType* FPrimitiveType::WithScalarKind(EScalarKind InScalarKind) const
{
	return FPrimitiveType::Get(InScalarKind, NumRows, NumColumns);
}

const FPrimitiveType* FPrimitiveType::ToVector(int InNumRows) const
{
	return FPrimitiveType::GetVector(ScalarKind, InNumRows);
}

const FType* FObjectType::GetTexture2D()
{
	static const FObjectType Instance { { TypeKind_Object }, ObjectKind_Texture2D };
	return &Instance;
}

} // namespace MIR

#endif // #if WITH_EDITOR
