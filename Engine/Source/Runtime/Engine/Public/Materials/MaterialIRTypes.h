// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "Shader/ShaderTypes.h"
#include "MaterialValueType.h"
#include "MaterialTypes.h"

#if WITH_EDITOR

namespace MIR
{

enum ETypeKind
{
	TypeKind_Poison,
	TypeKind_Void,
	TypeKind_Primitive,
	TypeKind_Object,
};

enum EObjectKind 
{
	ObjectKind_Texture2D,
};

const TCHAR* TypeKindToString(ETypeKind Kind);

struct FType
{
	/// Identifies what derived type this is.
	ETypeKind Kind;

	/// Returns the type matching specified UE::Shader::FType.
	static const FType* FromShaderType(const UE::Shader::FType& InShaderType);
	
	/// Returns the type matching specified EMaterialValueType.
	static const FType* FromMaterialValueType(EMaterialValueType Type);

	/// Returns the type matching specified EMaterialParameterType.
	static const FType* FromMaterialParameterType(EMaterialParameterType Type);

	/// Returns the `void` type.
	static const FType* GetVoid();

	/// Returns the `void` type.
	static const FType* GetPoison();

	/// Returns whether this is the poison type.
	bool IsPoison() const;

	/// Returns whether this is a boolean primitive type (of any dimension).
	bool IsBoolean() const;
	
	/// Returns whether this is an integer primitive type (of any dimension).
	bool IsInteger() const;

	/// Returns whether this type is a `bool` scalar.
	bool IsBoolScalar() const;

	/// Returns whether this type is of a specific object kind.
	bool IsObjectOfKind(EObjectKind ObjectKind) const;

	/// Returns whether this type is a texture.
	bool IsTexture() const;

	/// Returns this type upcast this type to PrimitiveType if it is one, otherwise nullptr.
	const FPrimitiveType* AsPrimitive() const;
	
	/// Returns this type upcast this type to PrimitiveType if it is one, otherwise nullptr.
	const FPrimitiveType* AsArithmetic() const;

	/// Returns this type upcast this type to PrimitiveType if it's a scalar, otherwise nullptr.
	const FPrimitiveType* AsScalar() const;

	/// Returns this type upcast this type to PrimitiveType if it's a vector, otherwise nullptr.
	const FPrimitiveType* AsVector() const;

	/// Returns this type upcast this type to PrimitiveType if it's a matrix, otherwise nullptr.
	const FPrimitiveType* AsMatrix() const;

	/// Returns the this type name spelling (e.g. float4x4).
	FStringView GetSpelling() const;
	
	/// Returns this type cast as a FObjectPtr if it is one, otherwise nullptr.
	const FObjectType* AsObject() const;

	/// Converts this type to a UE::Shader::EValueType.
	UE::Shader::EValueType ToValueType() const;
};

/// Primitive types of a single scalar.
/// Note: These are listed in precision order. Converting one to the other is then simply performed taking the max EScalarKind.
enum EScalarKind
{
	ScalarKind_Bool,
	ScalarKind_Int,
	ScalarKind_Float,
};

/// Returns whether the specified scalar kind supports arithmetic operators (plus, minus, etc).
bool ScalarKindIsArithmetic(EScalarKind Kind);

/// Returns whether the specified scalar kind is a floating point type (float, double, etc).
bool ScalarKindIsAnyFloat(EScalarKind Kind);

/// Returns the string representation of specified scalar kind.
const TCHAR* ScalarKindToString(EScalarKind Kind);

/// Represents the type of scalars, vectors and matrices. It indicates what kind of scalar
/// data type it has and type dimensions (rows and columns).
struct FPrimitiveType : FType
{
	/// String representation of this type (e.g. 'float3', "bool4x2")
	FStringView Spelling;

	/// Scalar data type kind.
	EScalarKind ScalarKind;

	/// Number of rows.
	int NumRows;
	
	/// Number of columns (rows > 1 and columns == 1, this is a vector).
	int NumColumns;

	// Returns the boolean scalar type.
	static const FPrimitiveType* GetBool();
	
	// Returns the integer scalar type.
	static const FPrimitiveType* GetInt();
	
	// Returns the floating point scalar type.
	static const FPrimitiveType* GetFloat();
	
	// Returns the floating point 2D vector type.
	static const FPrimitiveType* GetFloat2();
	
	// Returns the floating point 3D vector type.
	static const FPrimitiveType* GetFloat3();
	
	// Returns the floating point 4D vector type.
	static const FPrimitiveType* GetFloat4();

	// Returns the scalar type with given kind.
	static const FPrimitiveType* GetScalar(EScalarKind InScalarKind);

	// Returns the column vector type with given kind and number of rows.
	static const FPrimitiveType* GetVector(EScalarKind InScalarKind, int NumRows);
	
	// Returns the primitive type with given kind and number of columns and rows.
	static const FPrimitiveType* Get(EScalarKind InScalarKind, int NumRows, int NumColumns);

	// Returns the number of components in this primitive type.
	int  GetNumComponents() const { return NumRows * NumColumns; }

	// Whether this primitive type is scalar.
	bool IsScalar() const { return GetNumComponents() == 1; }
	
	// Whether this primitive type is a column vector.
	bool IsVector() const { return NumRows > 1 && NumColumns == 1; }
	
	// Whether this primitive type is a matrix.
	bool IsMatrix() const { return NumRows > 1 && NumColumns > 1; }
	
	// Whether this primitive type is arithmetic (it supports arithemtic operations like addition).
	bool IsArithmetic() const { return ScalarKindIsArithmetic(ScalarKind); }

	// Returns this primitive type with a different scalar kind.
	const FPrimitiveType* WithScalarKind(EScalarKind InScalarKind) const;
	
	// Returns the scalar type with this type scalar kind
	const FPrimitiveType* ToScalar() const;
	
	// Returns the vector type with this type scalar kind
	const FPrimitiveType* ToVector(int NumRows) const;
};

struct FObjectType : FType
{
	EObjectKind ObjectKind;

	static const FType* GetTexture2D();
};

} // namespace MIR

#endif // #if WITH_EDITOR
