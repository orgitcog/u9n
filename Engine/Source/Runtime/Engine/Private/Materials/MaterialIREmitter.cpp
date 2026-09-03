// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIREmitter.h"
#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIRModuleBuilder.h"
#include "Materials/MaterialIRInternal.h"
#include "Materials/MaterialExternalCodeRegistry.h"
#include "Shader/ShaderTypes.h"
#include "MaterialShared.h"
#include "MaterialExpressionIO.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"

#if WITH_EDITOR

namespace MIR
{

const TCHAR* VectorComponentToString(EVectorComponent Component)
{
	static const TCHAR* Strings[] = { TEXT("x"), TEXT("y"), TEXT("z"), TEXT("w") };
	return Strings[(int32)Component];
}

FSwizzleMask::FSwizzleMask(EVectorComponent X)
: NumComponents{ 1 }
{
	Components[0] = X;
}

FSwizzleMask::FSwizzleMask(EVectorComponent X, EVectorComponent Y)
: NumComponents{ 2 }
{
	Components[0] = X;
	Components[1] = Y;
}

FSwizzleMask::FSwizzleMask(EVectorComponent X, EVectorComponent Y, EVectorComponent Z)
: NumComponents{ 3 }
{
	Components[0] = X;
	Components[1] = Y;
	Components[2] = Z;
}

FSwizzleMask::FSwizzleMask(EVectorComponent X, EVectorComponent Y, EVectorComponent Z, EVectorComponent W)
: NumComponents{ 4 }
{

	Components[0] = X;
	Components[1] = Y;
	Components[2] = Z;
	Components[3] = W;
}

FSwizzleMask FSwizzleMask::XYZ()
{
	return { EVectorComponent::X, EVectorComponent::Y, EVectorComponent::Z };
}

void FSwizzleMask::Append(EVectorComponent Component)
{
	check(NumComponents < 4);
	Components[NumComponents++] = Component;
}

struct FEmitter::FPrivate
{
	// Looks for an existing value in the module that matches `Prototype` and returns it if found.
	static FValue* FindValue(FEmitter& Emitter, const FValue* Prototype)
	{
		FValue** Value = Emitter.ValueSet.Find(Prototype);
		return Value ? *Value : nullptr;
	}

	// Allocates a block of memory using the emitter's allocator.
	static uint8* Allocate(FEmitter& Emitter, int32 Size, int32 Alignment)
	{
		uint8* Bytes = (uint8*) FMemory::Malloc(Size, Alignment); // Emitter.Module->Allocator.PushBytes(Size, Alignment);
		FMemory::Memzero(Bytes, Size);
		return Bytes;
	}

	// Pushes a new value to the list of values.
	static void PushNewValue(FEmitter& Emitter, FValue* Value)
	{
		Emitter.Module->Values.Add(Value);
		Emitter.ValueSet.Add(Value);
	}
};

// Creates a new `FDimensional` value of specified `Type` and returns it.
static FDimensional* NewDimensionalValue(FEmitter& Emitter, const FPrimitiveType* Type)
{
	check(!Type->IsScalar());

	int32 Dimensions = Type->NumRows * Type->NumColumns;
	int32 SizeInBytes = sizeof(FDimensional) + sizeof(FValue*) * Dimensions;

	uint8* Bytes = FEmitter::FPrivate::Allocate(Emitter, SizeInBytes, alignof(FDimensional));

	FDimensional* Value = new (Bytes) FDimensional{};
	Value->Kind = VK_Dimensional;
	Value->Type = Type;

	return Value;
}

// Emits specified newly created `Value`. If the exact value already exists,
// specified one is *destroyed* and existing one is returned instead.
static FValueRef EmitNew(FEmitter& Emitter, FValue* Value)
{
	if (FValue* Existing = FEmitter::FPrivate::FindValue(Emitter, Value))
	{
		delete Value;
		return Existing;
	}

	FEmitter::FPrivate::PushNewValue(Emitter, Value);
	return Value;
}

template <typename T>
static T MakePrototype(const FType* InType)
{
	static_assert(std::is_trivially_constructible_v<T> && std::is_trivially_destructible_v<T> &&  std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T>,
		"FValues are expected to be trivial types.");

	T Value;
	FMemory::Memzero(Value);
	Value.Kind = T::TypeKind;
	Value.Type = InType;
	return Value;
}

// Searches for an existing value in module that matches specified `Prototype`.
// If none found, it creates a new value as a copy of the prototype, adds it to
// the module then returns it.
template <typename TValueType>
static FValueRef EmitPrototype(FEmitter& Emitter, const TValueType& Prototype)
{
	if (FValue* Existing = FEmitter::FPrivate::FindValue(Emitter, &Prototype))
	{
		return Existing;
	}

	uint8* Bytes = FEmitter::FPrivate::Allocate(Emitter, sizeof(TValueType), alignof(TValueType));
	TValueType* Value = new (Bytes) TValueType{ Prototype };

	FEmitter::FPrivate::PushNewValue(Emitter, Value);

	return Value;
}

// Finds the expression input index. Although the implementation has O(n) complexity, it is only used for error reporting.
static int32 SlowFindExpressionInputIndex(UMaterialExpression* Expression, const FExpressionInput* InInput)
{
	for (FExpressionInputIterator It{ Expression }; It; ++It)
	{
		if (It.Input == InInput)
		{
			return It.Index;
		}
	}
	check(false && "No input found.");
	return -1;
}

// Finds the expression input name. Although the implementation has O(n) complexity, it is only used for error reporting.
static FName SlowFindInputName(UMaterialExpression* Expression, const FExpressionInput* InInput)
{
	int32 InputIndex = SlowFindExpressionInputIndex(Expression, InInput);
	return Expression->GetInputName(InputIndex);
}

/*------------------------------------- FValueRef --------------------------------------*/

static FValueRef With(FValueRef ValueRef, FValue* Value)
{
	FValueRef Copy = ValueRef;
	Copy.Value = Value;
	return Copy;
}

static inline bool IsAnyNotValid()
{
	return false;
}

// Returns whether any of the values is invalid (null or poison).
template <typename... TTail>
static bool IsAnyNotValid(FValueRef Head, TTail... Tail)
{
	return !Head.IsValid() || IsAnyNotValid(Tail...);
}

// Returns whether any of the values is invalid (null or poison).
static inline bool IsAnyNotValid(TConstArrayView<FValueRef> Values)
{
	for (FValueRef Value : Values)
	{
		if (!Value.IsValid())
		{
			return true;
		}
	}
	return false;
}

bool FValueRef::IsValid() const
{
	return Value && !Value->IsPoison();
}

FValueRef FValueRef::To(FValue* InValue) const
{
	return { InValue, Input };
}

FValueRef FValueRef::ToPoison() const
{
	return To(FPoison::Get());
}

/*----------------------------------- Error handling -----------------------------------*/

void FEmitter::Error(FValueRef Source, FStringView Message)
{
	Source.Input
		? Error(FString::Printf(TEXT("From expression input '%s': %s"), *SlowFindInputName(Expression, Source.Input).ToString(),Message.GetData()))
		: Error(Message);
}

void FEmitter::Error(FStringView Message)
{
	FMaterialIRModule::FError Error;
	Error.Expression = Expression;

	// Add the node type to the error message
	const int32 ChopCount = FCString::Strlen(TEXT("MaterialExpression"));
	const FString& ErrorClassName = Expression->GetClass()->GetName();
	
	Error.Message.Appendf(TEXT("(Node %s) %s"), *ErrorClassName + ChopCount, Message.GetData());
	
	Module->Errors.Push(Error);
	bCurrentExpressionHasErrors = true;
}

/*--------------------------------- Type handling ----------------------------------*/

const FType* FEmitter::GetCommonType(const FType* A, const FType* B)
{
	if (const FType* CommonType = TryGetCommonType(A, B))
	{
		return CommonType;
	}

	Errorf(TEXT("No common type between '%s' and '%s'."), A->GetSpelling().GetData(), B->GetSpelling().GetData());
	return nullptr;
}

/*-------------------------------- Input management --------------------------------*/

FValueRef FEmitter::TryInput(const FExpressionInput* InInput)
{
	return FValueRef{ MIR::Internal::FetchValueFromExpressionInput(BuilderImpl, InInput), InInput };
}

FValueRef FEmitter::Input(const FExpressionInput* InInput)
{
	FValueRef Value = TryInput(InInput);
	if (!Value)
	{
		Errorf(TEXT("Missing '%s' input value."), *SlowFindInputName(Expression, InInput).ToString());
		return Value.ToPoison();
	}
	return Value;
}

FValueRef FEmitter::InputDefaultBool(const FExpressionInput* Input, bool Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantBool(Default));
}

FValueRef FEmitter::InputDefaultInt(const FExpressionInput* Input, TInteger Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantInt(Default));
}

FValueRef FEmitter::InputDefaultInt2(const FExpressionInput* Input, UE::Math::TIntVector2<TInteger> Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantInt2(Default));
}

FValueRef FEmitter::InputDefaultInt3(const FExpressionInput* Input, UE::Math::TIntVector3<TInteger> Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantInt3(Default));
}

FValueRef FEmitter::InputDefaultInt4(const FExpressionInput* Input, UE::Math::TIntVector4<TInteger> Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantInt4(Default));
}

FValueRef FEmitter::InputDefaultFloat(const FExpressionInput* Input, TFloat Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantFloat(Default));
}

FValueRef FEmitter::InputDefaultFloat2(const FExpressionInput* Input, UE::Math::TVector2<TFloat> Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantFloat2(Default));
}

FValueRef FEmitter::InputDefaultFloat3(const FExpressionInput* Input, UE::Math::TVector<TFloat> Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantFloat3(Default));
}

FValueRef FEmitter::InputDefaultFloat4(const FExpressionInput* Input, UE::Math::TVector4<TFloat> Default)
{
	FValueRef Value = TryInput(Input);
	return Value ? Value : Value.To(ConstantFloat4(Default));
}

FValueRef FEmitter::CheckTypeIsKind(FValueRef Value, ETypeKind Kind)
{
	if (Value.IsValid() && Value->Type->Kind != Kind)
	{
		Errorf(Value, TEXT("Expected a '%s' value, got a '%s' instead."), TypeKindToString(Kind), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}
	return Value;
}

FValueRef FEmitter::CheckIsPrimitive(FValueRef Value)
{
	if (Value.IsValid() && !Value->Type->AsPrimitive())
	{
		Errorf(Value, TEXT("Expected a primitive value, got a '%s' instead."), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}
	return Value;
}

FValueRef FEmitter::CheckIsArithmetic(FValueRef Value)
{
	if (Value.IsValid() && !Value->Type->AsArithmetic())
	{
		Errorf(Value, TEXT("Expected an arithmetic value, got a '%s' instead."), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}
	return Value;
}

FValueRef FEmitter::CheckIsInteger(FValueRef Value)
{
	if (Value.IsValid() && !Value->Type->IsInteger())
	{
		Errorf(Value, TEXT("Expected an integer value, got a '%s' instead."), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}
	return Value;
}

FValueRef FEmitter::CheckIsScalar(FValueRef Value)
{
	if (Value.IsValid() && (!Value->Type->AsPrimitive() || !Value->Type->AsPrimitive()->IsScalar()))
	{
		Errorf(Value, TEXT("Expected a scalar value, got a '%s' instead."), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}
	return Value;
}

FValueRef FEmitter::CheckIsScalarOrVector(FValueRef Value)
{
	if (Value.IsValid() && (!Value->Type->AsPrimitive() || Value->Type->AsPrimitive()->IsMatrix()))
	{
		Errorf(Value, TEXT("Expected a scalar or vector value, got a '%s' instead."), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}
	return Value;
}

FValueRef FEmitter::CheckIsTexture(FValueRef Value)
{
	if (Value.IsValid() && !Value->Type->IsTexture())
	{
		Errorf(Value, TEXT("Expected a texture value, got a '%s' instead."), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}
	return Value;
}

bool FEmitter::ToConstantBool(FValueRef Value)
{
	if (!Value.IsValid())
	{
		return false;
	}
	const MIR::FConstant* Constant = Value->As<MIR::FConstant>();
    if (!Constant)
    {
        Errorf(Value, TEXT("Expected a constant bool value, got a non-constant value instead."));
        return false;
    }
	if (Constant->Type != FPrimitiveType::GetBool())
    {
        Errorf(Value, TEXT("Expected a constant bool value, got a '%s' instead."), Constant->Type->GetSpelling().GetData());
        return false;
    }
	return Constant->Boolean;
 }

/*-------------------------------- Output management -------------------------------*/

FEmitter& FEmitter::Output(int32 OutputIndex, FValueRef Value)
{
	Output(Expression->GetOutput(OutputIndex), Value);
	return *this;
}

FEmitter& FEmitter::Output(const FExpressionOutput* Output, FValueRef Value)
{
	MIR::Internal::BindValueToExpressionOutput(BuilderImpl, Output, Value);
	return *this;
}

/*------------------------------- Constants emission -------------------------------*/

FValueRef FEmitter::ConstantFromShaderValue(const UE::Shader::FValue& InValue)
{
	using namespace UE::Shader;

	switch (InValue.Type.ValueType)
	{
		case UE::Shader::EValueType::Float1: return ConstantFloat(InValue.AsFloatScalar());
		case UE::Shader::EValueType::Float2: return ConstantFloat2(UE::Math::TVector2<TFloat>{ InValue.Component[0].Float, InValue.Component[1].Float });
		case UE::Shader::EValueType::Float3: return ConstantFloat3(UE::Math::TVector<TFloat>{ InValue.Component[0].Float, InValue.Component[1].Float, InValue.Component[2].Float });
		case UE::Shader::EValueType::Float4: return ConstantFloat4(UE::Math::TVector4<TFloat>{ InValue.Component[0].Float, InValue.Component[1].Float, InValue.Component[2].Float, InValue.Component[3].Float });

		case UE::Shader::EValueType::Int1: return ConstantInt(InValue.AsFloatScalar());
		case UE::Shader::EValueType::Int2: return ConstantInt2({ InValue.Component[0].Int, InValue.Component[1].Int });
		case UE::Shader::EValueType::Int3: return ConstantInt3({ InValue.Component[0].Int, InValue.Component[1].Int, InValue.Component[2].Int });
		case UE::Shader::EValueType::Int4: return ConstantInt4({ InValue.Component[0].Int, InValue.Component[1].Int, InValue.Component[2].Int, InValue.Component[3].Int });
	}

	UE_MIR_UNREACHABLE();
}

FValueRef FEmitter::ConstantZero(EScalarKind Kind)
{
	switch (Kind)
	{
		case ScalarKind_Bool: return ConstantFalse();
		case ScalarKind_Int: return ConstantInt(0);
		case ScalarKind_Float: return ConstantFloat(0.0f);
		default: UE_MIR_UNREACHABLE();
	}
}

FValueRef FEmitter::ConstantOne(EScalarKind Kind)
{
	switch (Kind)
	{
		case ScalarKind_Bool: return ConstantTrue();
		case ScalarKind_Int: return ConstantInt(1);
		case ScalarKind_Float: return ConstantFloat(1.0f);
		default: UE_MIR_UNREACHABLE();
	}
}

FValueRef FEmitter::ConstantScalar(EScalarKind Kind, TFloat FromFloat)
{
	switch (Kind)
	{
		case ScalarKind_Bool: return ConstantBool(FromFloat != 0.0f);
		case ScalarKind_Int: return ConstantInt(TInteger(FromFloat));
		case ScalarKind_Float: return ConstantFloat(FromFloat);
		default: UE_MIR_UNREACHABLE();
	}
}

FValueRef FEmitter::ConstantTrue()
{
	return TrueConstant;
}

FValueRef FEmitter::ConstantFalse()
{
	return FalseConstant;
}

FValueRef FEmitter::ConstantBool(bool InX)
{
	return InX ? ConstantTrue() : ConstantFalse();
}

FValueRef FEmitter::ConstantInt(TInteger InX)
{
	FConstant Scalar = MakePrototype<FConstant>(FPrimitiveType::GetScalar(ScalarKind_Int));
	Scalar.Integer = InX;
	return EmitPrototype(*this, Scalar);
}

FValueRef FEmitter::ConstantInt2(UE::Math::TIntVector2<TInteger> InValue)
{
	FValueRef X = ConstantInt(InValue.X);
	FValueRef Y = ConstantInt(InValue.Y);
	return Vector2(X, Y);
}

FValueRef FEmitter::ConstantInt3(UE::Math::TIntVector3<TInteger> InValue)
{
	FValueRef X = ConstantInt(InValue.X);
	FValueRef Y = ConstantInt(InValue.Y);
	FValueRef Z = ConstantInt(InValue.Z);
	return Vector3(X, Y, Z);
}

FValueRef FEmitter::ConstantInt4(UE::Math::TIntVector4<TInteger> InValue)
{
	FValueRef X = ConstantInt(InValue.X);
	FValueRef Y = ConstantInt(InValue.Y);
	FValueRef Z = ConstantInt(InValue.Z);
	FValueRef W = ConstantInt(InValue.W);
	return Vector4(X, Y, Z, W);
}

FValueRef FEmitter::ConstantFloat(TFloat InX)
{
	FConstant Scalar = MakePrototype<FConstant>(FPrimitiveType::GetScalar(ScalarKind_Float));
	Scalar.Float = InX;
	return EmitPrototype(*this, Scalar);
}

FValueRef FEmitter::ConstantFloat2(UE::Math::TVector2<TFloat> InValue)
{
	FValueRef X = ConstantFloat(InValue.X);
	FValueRef Y = ConstantFloat(InValue.Y);
	return Vector2(X, Y);
}

FValueRef FEmitter::ConstantFloat3(UE::Math::TVector<TFloat> InValue)
{
	FValueRef X = ConstantFloat(InValue.X);
	FValueRef Y = ConstantFloat(InValue.Y);
	FValueRef Z = ConstantFloat(InValue.Z);
	return Vector3(X, Y, Z);
}

FValueRef FEmitter::ConstantFloat4(UE::Math::TVector4<TFloat> InValue)
{
	FValueRef X = ConstantFloat(InValue.X);
	FValueRef Y = ConstantFloat(InValue.Y);
	FValueRef Z = ConstantFloat(InValue.Z);
	FValueRef W = ConstantFloat(InValue.W);
	return Vector4(X, Y, Z, W);
}

/*--------------------- Other non-instruction values emission ---------------------*/

FValueRef FEmitter::Poison()
{
	return FPoison::Get();
}

FValueRef FEmitter::ExternalInput(EExternalInput Id)
{
	MIR::FExternalInput Prototype = MakePrototype<MIR::FExternalInput>(GetExternalInputType(Id));
	Prototype.Id = Id;
	return EmitPrototype(*this, Prototype);
}

FValueRef FEmitter::TextureObject(UTexture* Texture, EMaterialSamplerType SamplerType)
{
	if (Texture->GetMaterialType() != MCT_Texture2D)
	{
		Error(TEXT("Only Textures 2D are supported for now."));
		return Poison();
	}

	FTextureObject Proto = MakePrototype<FTextureObject>(FObjectType::GetTexture2D());
	Proto.Texture = Texture;
	Proto.SamplerType = SamplerType;
	return EmitPrototype(*this, Proto);
}

FValueRef FEmitter::Parameter(FName Name, FMaterialParameterMetadata& Metadata, EMaterialSamplerType SamplerType)
{
	// Helper local function that register a parameter (info and metadata) to the module, and returns some uint32 ID.
	auto RegisterParameter = [] (FMaterialIRModule* InModule, FMaterialParameterInfo InInfo, const FMaterialParameterMetadata& InMetadata) -> uint32
	{
		uint32 Id;
		if (Internal::Find(InModule->ParameterInfoToId, InInfo, Id))
		{
			check(InModule->ParameterIdToData[Id].Get<FMaterialParameterMetadata>().Value == InMetadata.Value);
			return Id;
		}

		Id = InModule->ParameterIdToData.Num();
		InModule->ParameterInfoToId.Add(InInfo, Id);
		InModule->ParameterIdToData.Push({ InInfo, InMetadata });
		return Id;
	};

	FMaterialParameterInfo Info{ Name };

	switch (Metadata.Value.Type)
	{
		case EMaterialParameterType::Scalar:
		{
			if (Metadata.PrimitiveDataIndex != INDEX_NONE)
			{
				return CustomPrimitiveData(Metadata.PrimitiveDataIndex);
			}

			MIR::FUniformParameter Prototype = MakePrototype<MIR::FUniformParameter>(FPrimitiveType::GetFloat());
			Prototype.ParameterIdInModule = RegisterParameter(Module, Info, Metadata);
			return EmitPrototype(*this, Prototype);
		}

		case EMaterialParameterType::Vector:
		{
			if (Metadata.PrimitiveDataIndex != INDEX_NONE)
			{
				MIR::FValue* X = CustomPrimitiveData(Metadata.PrimitiveDataIndex + 0);
				MIR::FValue* Y = CustomPrimitiveData(Metadata.PrimitiveDataIndex + 1);
				MIR::FValue* Z = CustomPrimitiveData(Metadata.PrimitiveDataIndex + 2);
				MIR::FValue* W = CustomPrimitiveData(Metadata.PrimitiveDataIndex + 3);
				return Vector4(X, Y, Z, W);
			}

			MIR::FUniformParameter Prototype = MakePrototype<MIR::FUniformParameter>(FPrimitiveType::GetFloat4());
			Prototype.ParameterIdInModule = RegisterParameter(Module, Info, Metadata);
			return EmitPrototype(*this, Prototype);
		}

		case EMaterialParameterType::Texture:
		{
			if (Metadata.Value.Texture->GetMaterialType() != MCT_Texture2D)
			{
				Errorf(TEXT("Unsupported texture type"));
				return MIR::FPoison::Get();
			}

			MIR::FUniformParameter Prototype = MakePrototype<MIR::FUniformParameter>(FObjectType::GetTexture2D());
			Prototype.ParameterIdInModule = RegisterParameter(Module, Info, Metadata);
			Prototype.SamplerType = SamplerType;
			return EmitPrototype(*this, Prototype);
		}

		case EMaterialParameterType::StaticSwitch:
		{
			// Apply eventual parameter override
			for (const FStaticSwitchParameter& Param : StaticParameterSet->GetRuntime().StaticSwitchParameters)
			{
				if (Param.IsOverride() && Param.ParameterInfo.Name == Name)
				{
					Metadata.Value.Bool[0] = Param.Value;
					break;
				}
			}
			return ConstantBool(Metadata.Value.Bool[0]);
		}

		default:
			UE_MIR_TODO();
	}
}

FValueRef FEmitter::CustomPrimitiveData(uint32 PrimitiveDataIndex)
{
	// UE_MIR_TODO();
	return {};
}

/*------------------------------ Instruction emission ------------------------------*/

FSetMaterialOutput* FEmitter::SetMaterialOutput(EMaterialProperty InProperty, FValue* Arg)
{
	FSetMaterialOutput Proto = MakePrototype<FSetMaterialOutput>(nullptr);
	Proto.Property  			= InProperty;
	Proto.Arg 					= Arg;

	// Initialize the instruction block to the root of each stage it is evaluated in.
	for (int i = 0; i < NumStages; ++i)
	{
		if (MaterialOutputEvaluatesInStage(InProperty, (EStage)i))
		{
			Proto.Block[i]	= Module->RootBlock[i];
		}
	}

	// Make the instruction
	FSetMaterialOutput* Instr = static_cast<FSetMaterialOutput*>(EmitPrototype(*this, Proto).Value);

	// Add the instruction to list of outputs of the stages it is evaluated in.
	for (int i = 0; i < NumStages; ++i)
	{
		if (MaterialOutputEvaluatesInStage(InProperty, (EStage)i))
		{
			Module->Outputs[i].Add(Instr);
		}
	}

	return Instr;
}

FValueRef FEmitter::Vector2(FValueRef InX, FValueRef InY)
{
	if (IsAnyNotValid(InX, InY))
	{
		return Poison();
	}

	check(InX->Type->AsScalar());
	check(InX->Type == InY->Type);

	TDimensional<2> Vector = MakePrototype<TDimensional<2>>(FPrimitiveType::GetVector(InX->Type->AsPrimitive()->ScalarKind, 2));
	TArrayView<FValue*> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;

	FValueRef Value = EmitPrototype(*this, Vector);
	if (InX.Input == InY.Input)
	{
		Value.Input = InX.Input;
	}

	return EmitPrototype(*this, Vector);
}

FValueRef FEmitter::Vector3(FValueRef InX, FValueRef InY, FValueRef InZ)
{
	if (IsAnyNotValid(InX, InY, InZ))
	{
		return Poison();
	}

	check(InX->Type->AsScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);

	TDimensional<3> Vector = MakePrototype<TDimensional<3>>(FPrimitiveType::GetVector(InX->Type->AsPrimitive()->ScalarKind, 3));
	TArrayView<FValue*> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;

	FValueRef Value = EmitPrototype(*this, Vector);
	if (InX.Input == InY.Input && InX.Input == InZ.Input)
	{
		Value.Input = InX.Input;
	}

	return Value;
}

FValueRef FEmitter::Vector4(FValueRef InX, FValueRef InY, FValueRef InZ, FValueRef InW)
{
	if (IsAnyNotValid(InX, InY, InZ, InW))
	{
		return Poison();
	}

	check(InX->Type->AsScalar());
	check(InX->Type == InY->Type);
	check(InY->Type == InZ->Type);
	check(InZ->Type == InW->Type);

	TDimensional<4> Vector = MakePrototype<TDimensional<4>>(FPrimitiveType::GetVector(InX->Type->AsPrimitive()->ScalarKind, 4));
	TArrayView<FValue*> Components = Vector.GetMutableComponents();
	Components[0] = InX;
	Components[1] = InY;
	Components[2] = InZ;
	Components[3] = InW;

	FValueRef Value = EmitPrototype(*this, Vector);
	if (InX.Input == InY.Input && InX.Input == InZ.Input && InX.Input == InW.Input)
	{
		Value.Input = InX.Input;
	}

	return EmitPrototype(*this, Vector);
}

/*--------------------------------- Operator emission ---------------------------------*/

template <typename T>
static bool FoldComparisonOperatorScalar(EOperator Operator, T A, T B)
{
	if constexpr (std::is_floating_point_v<T>)
	{
		switch (Operator)
		{
			case UO_IsFinite: return FGenericPlatformMath::IsFinite(A);
			case UO_IsInf: return !FGenericPlatformMath::IsFinite(A);
			case UO_IsNan: return FGenericPlatformMath::IsNaN(A);
			default: break;
		}
	}

	switch (Operator)
	{
		case UO_Not: return !A;
		case BO_GreaterThan: return A > B;
		case BO_GreaterThanOrEquals: return A >= B;
		case BO_LessThan: return A < B;
		case BO_LessThanOrEquals: return A <= B;
		case BO_Equals: return A == B;
		case BO_NotEquals: return A != B;
		default: UE_MIR_UNREACHABLE();
	}
}

template <typename T>
static T ACosh(T x)
{
	static_assert(std::is_floating_point_v<T>);
	check(x >= 1);
	return FGenericPlatformMath::Loge(x + FGenericPlatformMath::Sqrt(x * x - 1));
}

template <typename T>
static T ASinh(T x)
{
	static_assert(std::is_floating_point_v<T>);
	return FGenericPlatformMath::Loge(x + FGenericPlatformMath::Sqrt(x * x + 1));
}

template <typename T>
static T ATanh(T x)
{
	static_assert(std::is_floating_point_v<T>);
    check(x > -1 && x < 1);
    return T(0.5) * FGenericPlatformMath::Loge((1 + x) / (1 - x));
}

template <typename T>
static T FoldScalarOperator(FEmitter& Emitter, EOperator Operator, T A, T B, T C)
{
	if constexpr (std::is_floating_point_v<T>)
	{
		switch (Operator)
		{
			case UO_ACos: return FGenericPlatformMath::Acos(A);
			case UO_ACosh: return MIR::ACosh(A);
			case UO_ASin: return FGenericPlatformMath::Asin(A);
			case UO_ASinh: return MIR::ASinh(A);
			case UO_ATan: return FGenericPlatformMath::Atan(A);
			case UO_ATanh: return MIR::ATanh(A);
			case UO_Ceil:
				if constexpr (std::is_same<T, float>::value)
				{
					return FGenericPlatformMath::CeilToFloat(A);
				}
				else
				{
					return FGenericPlatformMath::CeilToDouble(A);
				}
			case UO_Cos: return FGenericPlatformMath::Cos(A);
			case UO_Cosh: return FGenericPlatformMath::Cosh(A);
			case UO_Exponential: return FGenericPlatformMath::Pow(UE_EULERS_NUMBER, A);
			case UO_Exponential2: return FGenericPlatformMath::Pow(2.0f, A);
			case UO_Floor:
				if constexpr (std::is_same<T, float>::value)
				{
					return FGenericPlatformMath::FloorToFloat(A);
				}
				else
				{
					return FGenericPlatformMath::FloorToDouble(A);
				}
			case UO_Frac:
			{
				return FGenericPlatformMath::Fractional(A);
			}
			case UO_Logarithm:
			case UO_Logarithm2:
			case UO_Logarithm10:
			{
				const T Base = (Operator == UO_Logarithm) ? T(UE_EULERS_NUMBER)
					: Operator == UO_Logarithm2 ? T(2)
					: T(10);
				return FGenericPlatformMath::LogX(Base, A);
			}
			case UO_Round:
				if constexpr (std::is_same<T, float>::value)
				{
					return FGenericPlatformMath::RoundToFloat(A);
				}
				else
				{
					return FGenericPlatformMath::RoundToDouble(A);
				}
			case UO_Sin: return FGenericPlatformMath::Sin(A);
			case UO_Sinh: return FGenericPlatformMath::Sinh(A);
			case UO_Sqrt: return FGenericPlatformMath::Sqrt(A);
			case UO_Tan: return FGenericPlatformMath::Tan(A);
			case UO_Tanh: return FGenericPlatformMath::Tanh(A);
			case UO_Truncate:
				if constexpr (std::is_same<T, float>::value)
				{
					return FGenericPlatformMath::TruncToFloat(A);
				}
				else
				{
					return FGenericPlatformMath::TruncToDouble(A);
				}
			case BO_Fmod: return FGenericPlatformMath::Fmod(A, B);
			case BO_Pow: return FGenericPlatformMath::Pow(A, B);
			case TO_Lerp: return FMath::Lerp<T>(A, B, C);
			case TO_Smoothstep: return FMath::SmoothStep<T>(A, B, C);
			default: break;
		}
	}

	if constexpr (std::is_integral_v<T>)
	{
		switch (Operator)
		{
			case UO_Not: return !A;
			case UO_BitwiseNot: return ~A;
			case BO_And: return A & B;
			case BO_Or: return A | B;
			case BO_BitwiseAnd: return A & B;
			case BO_BitwiseOr: return A | B;
			case BO_BitShiftLeft: return A << B;
			case BO_BitShiftRight: return A >> B;
			case BO_Modulo: return A % B;
			default: break;
		}
	}

	switch (Operator)
	{
		case UO_Abs: return FGenericPlatformMath::Abs<T>(A);
		case UO_Negate: return -A;
		case UO_Saturate: return FMath::Clamp(A, T(0), T(1));
		case BO_Add: return A + B;
		case BO_Subtract: return A - B;
		case BO_Multiply: return A * B;
		case BO_Divide: return A / B;
		case BO_Min: return FMath::Min<T>(A, B);
		case BO_Max: return FMath::Max<T>(A, B);
		case BO_Step: return B >= A ? 1.0f : 0.0f;
		case TO_Clamp: return FMath::Clamp(A, B, C);
		default: UE_MIR_UNREACHABLE();
	}
}

// It tries to apply a known identity of specified operator, e.g. "x + 0 = x ∀ x ∈ R".
// If it returns a value, the operation has been "folded" and the returned value is the
// result (in the example above, it would return "x").
// If it returns null, the end result could not be inferred, but the operator could have
// still been changed to some other (with lower complexity). For example "clamp(x, 0, 1)"
// will change to "saturate(x)".
static FValueRef TrySimplifyOperator(FEmitter& Emitter, EOperator& Op, FValueRef& A, FValueRef& B, FValueRef& C)
{
	switch (Op)
	{
		/* Unary Operators */
		case UO_Length:
			if (A->Type->AsScalar())
			{
				Op = UO_Abs;
			}
			break;

		/* Binary Comparisons */
		case BO_GreaterThan:
		case BO_LessThan:
		case BO_NotEquals:
			if (A->Equals(B))
			{
				return Emitter.ConstantFalse();
			}
			break;

		case BO_GreaterThanOrEquals:
		case BO_LessThanOrEquals:
		case BO_Equals:
			if (A->Equals(B))
			{
				return Emitter.ConstantTrue();
			}
			break;

		/* Binary Arithmetic */
		case BO_Add:
			if (A->AreAllNearlyZero())
			{
				return B;
			}
			else if (B->AreAllNearlyZero())
			{
				return A;
			}
			break;

		case BO_Subtract:
			if (B->AreAllNearlyZero())
			{
				return A;
			}
			else if (A->AreAllNearlyZero())
			{
				return Emitter.Negate(A);
			}
			break;

		case BO_Multiply:
			if (A->AreAllNearlyZero() || B->AreAllNearlyOne())
			{
				return A;
			}
			else if (A->AreAllNearlyOne() || B->AreAllNearlyZero())
			{
				return B;
			}
			break;

		case BO_Divide:
			if (A->AreAllNearlyZero() || B->AreAllNearlyOne())
			{
				return A;
			}
			break;

		case BO_Modulo:
			if (A->AreAllNearlyZero() || B->AreAllNearlyOne())
			{
				return Emitter.ConstantZero(A->Type->AsPrimitive()->ScalarKind);
			}
			break;

		case BO_BitwiseAnd:
			if (A->AreAllExactlyZero())
			{
				return A;
			}
			else if (B->AreAllExactlyZero())
			{
				return B;
			}
			break;

		case BO_BitwiseOr:
			if (A->AreAllExactlyZero())
			{
				return B;
			}
			else if (B->AreAllExactlyZero())
			{
				return A;
			}
			break;

		case BO_BitShiftLeft:
		case BO_BitShiftRight:
			if (A->AreAllExactlyZero() || B->AreAllExactlyZero())
			{
				return A;
			}
			break;

		case BO_Pow:
			if (A->AreAllNearlyZero())
			{
				return A;
			}
			else if (B->AreAllNearlyZero())
			{
				return Emitter.ConstantOne(A->Type->AsPrimitive()->ScalarKind);
			}
			break;

		case TO_Clamp:
			if (B->AreAllNearlyZero() && C->AreAllNearlyOne())
			{
				Op = UO_Saturate;
				B = {};
				C = {};
			}
			break;

		case TO_Lerp:
			if (C->AreAllNearlyZero())
			{
				return A;
			}
			else if (C->AreAllNearlyOne())
			{
				return B;
			}
			break;

		case TO_Select:
			if (A->AreAllTrue())
			{
				return B;
			}
			else if (A->AreAllFalse())
			{
				return C;
			}
			break;

		default:
			break;
	}

	return {};
}

// Tries to fold (statically evaluate) the operator, assuming that the arguments are all scalar.
// It returns either the result of the operator or null if it could not be folded.
static FValueRef TryFoldOperatorScalar(FEmitter& Emitter, EOperator Op, FValueRef A, FValueRef B, FValueRef C)
{
	const FPrimitiveType* PrimitiveType = A->Type->AsPrimitive();

	// Try to simplify the operator. This could potentially change Op, A, B and C.
	if (FValueRef Simplified = TrySimplifyOperator(Emitter, Op, A, B, C))
	{
		return Simplified;
	}

	// If TrySimplifyOperator did not already fold the `select` operator, there is nothing else to do.
	if (Op == TO_Select)
	{
		return nullptr;
	}

	// Verify that both lhs and rhs are constants, otherwise we cannot fold the operation.
	const FConstant* AConstant = As<FConstant>(A.Value);
	const FConstant* BConstant = As<FConstant>(B.Value);
	const FConstant* CConstant = As<FConstant>(C.Value);
	if (!AConstant || (IsBinaryOperator(Op) && !BConstant) || (IsTernaryOperator(Op) && (!BConstant || !CConstant)))
	{
		return nullptr;
	}

	// Call the appropriate helper function depending on what type of operator this is
	if (IsComparisonOperator(Op))
	{
		bool Result;
		switch (PrimitiveType->ScalarKind)
		{
			case ScalarKind_Int:
				Result = FoldComparisonOperatorScalar<TInteger>(Op, AConstant->Integer, BConstant->Integer);
				break;

			case ScalarKind_Float:
				Result = FoldComparisonOperatorScalar<TFloat>(Op, AConstant->Float, BConstant->Float);
				break;

			default:
				UE_MIR_UNREACHABLE();
		}
		return Emitter.ConstantBool(Result);
	}
	else
	{
		switch (PrimitiveType->ScalarKind)
		{
			case ScalarKind_Bool:
			{
				bool Result = FoldScalarOperator<TInteger>(Emitter, Op, AConstant->Boolean, BConstant ? BConstant->Boolean : 0, 0) & 0x1;
				return Emitter.ConstantBool(Result);
			}

			case ScalarKind_Int:
			{
				TInteger Result = FoldScalarOperator<TInteger>(Emitter, Op, AConstant->Integer, BConstant ? BConstant->Integer : 0, CConstant ? CConstant->Integer : 0);
				return Emitter.ConstantInt(Result);
			}

			case ScalarKind_Float:
			{
				TFloat Result = FoldScalarOperator<TFloat>(Emitter, Op, AConstant->Float, BConstant ? BConstant->Float : 0, CConstant ? CConstant->Float : 0);
				return Emitter.ConstantFloat(Result);
			}

			default:
				UE_MIR_UNREACHABLE();
		}
	}
}

/// Used to filter what parameter *primitive* types operators can take.
enum EOperatorParameterTypeFilter
{
	OPTF_Unknown = 0xff,										///< Unspecified
	OPTF_Any = 0,												///< Any primitive type
	OPTF_CastToFirstArgumentType = 1 << 10,						///< Cast the argument to the first argument's type
	OPTF_CastToAnyFloat = 1 << 9,								///< Cast the argument to the floating point primitive type of any dimension
	OPTF_CheckIsBoolean = 1 << 0,								///< Check the type is boolean primitive of any dimension
	OPTF_CheckIsInteger = 1 << 1,								///< Check the type is integer primitive of any dimension
	OPTF_CheckIsArithmetic = 1 << 2,							///< Check the type is arithmetic primitive of any dimension (i.e. that supports arithmetic operations)
	OPTF_CheckIsNotMatrix = 1 << 3,								///< Check the type is any primitive type except matrices
	OPTF_CheckIsVector3 = 1 << 4,								///< Check the type is a 3D vector of any scalar type
	OPTF_CheckIsNonNegativeFloatConst = 1 << 5,					///< Check that if the argument is a constant float, it is not negative (x >= 0)
	OPTF_CheckIsNonZeroFloatConst = 1 << 6,						///< Check that if the argument is a constant float, it is not zero	(x != 0)
	OPTF_CheckIsOneOrGreaterFloatConst = 1 << 7,				///< Check that if the argument is a constant float, it is 1 or greater (xFloat >= 1)
	OPTF_CheckIsBetweenMinusOneAndPlusOneFloatConst = 1 << 8,	///< Check that if the argument is a constant float, it is between -1 and 1 (-1 < x < 1)
	OPTF_CastToCommonType = 1 << 11,	 						///< Cast the argument to the common arguments type
	OPTF_CastToCommonArithmeticType = OPTF_CheckIsArithmetic | OPTF_CastToCommonType,
	OPTF_CastToCommonFloatType = OPTF_CastToAnyFloat | OPTF_CastToCommonType,
};

inline EOperatorParameterTypeFilter operator|(EOperatorParameterTypeFilter A, EOperatorParameterTypeFilter B)
{
	return EOperatorParameterTypeFilter(uint32(A) | uint32(B));
}

/// Used to determine the operator result type based on argument types
enum EOperatorReturnType
{
	ORT_Unknown,							///< Unspecified
	ORT_FirstArgumentType,					///< The same type as the first argument
	ORT_BooleanWithFirstArgumentDimensions,	///< A boolean primitive type with the same dimensions (rows and columns) as the first argument type
	ORT_FirstArgumentTypeToScalar,			///< A scalar primitive type with the same kind as the scalar type of the first argument
	ORT_SecondArgumentType,					///< The same type as the second argument
};

/// The signature of an operator consisting of its parameter and return type information.
struct FOperatorSignature
{
	EOperatorParameterTypeFilter ParameterTypes[3] = { OPTF_Unknown, OPTF_Unknown, OPTF_Unknown };
	EOperatorReturnType ReturnType = ORT_Unknown;
};

/// Returns the signature of an operator.
static const FOperatorSignature* GetOperatorSignature(EOperator Op)
{
	static const FOperatorSignature* Signatures = [] ()
	{
		const FOperatorSignature UnaryFloat = { { OPTF_CheckIsArithmetic | OPTF_CastToAnyFloat }, ORT_FirstArgumentType };
		const FOperatorSignature UnaryFloatToBoolean = { { OPTF_CheckIsArithmetic | OPTF_CastToAnyFloat }, ORT_BooleanWithFirstArgumentDimensions };
		const FOperatorSignature BinaryArithmetic = { { OPTF_CastToCommonArithmeticType, OPTF_CastToCommonArithmeticType }, ORT_FirstArgumentType };
		const FOperatorSignature BinaryInteger = { { OPTF_CheckIsInteger | OPTF_CastToCommonArithmeticType, OPTF_CheckIsInteger | OPTF_CastToCommonArithmeticType }, ORT_FirstArgumentType };
		const FOperatorSignature BinaryFloat = { { OPTF_CastToCommonFloatType , OPTF_CastToCommonFloatType }, ORT_FirstArgumentType };
		const FOperatorSignature BinaryArithmeticComparison = { { OPTF_CastToCommonArithmeticType, OPTF_CastToCommonArithmeticType }, ORT_BooleanWithFirstArgumentDimensions };
		const FOperatorSignature BinaryLogical = { { OPTF_CheckIsBoolean | OPTF_CastToCommonType, OPTF_CheckIsBoolean | OPTF_CastToCommonType }, ORT_FirstArgumentType };
		const FOperatorSignature TernaryArithmetic = { { OPTF_CastToCommonArithmeticType, OPTF_CastToCommonArithmeticType, OPTF_CastToCommonArithmeticType }, ORT_FirstArgumentType };
		const FOperatorSignature TernaryFloat = { { OPTF_CastToCommonArithmeticType | OPTF_CastToAnyFloat, OPTF_CastToCommonArithmeticType, OPTF_CastToCommonArithmeticType }, ORT_FirstArgumentType };

		static FOperatorSignature S[MIR::OperatorCount];

		/* unary operators */
		S[UO_BitwiseNot] 				= { { OPTF_CheckIsInteger }, ORT_FirstArgumentType };
		S[UO_Negate] 					= { { OPTF_CheckIsArithmetic }, ORT_FirstArgumentType };
		S[UO_Not] 						= { { OPTF_CheckIsBoolean }, ORT_FirstArgumentType };

		S[UO_Abs] 						= UnaryFloat;
		S[UO_ACos] 						= UnaryFloat;
		S[UO_ACosh] 					= { { OPTF_CheckIsArithmetic | OPTF_CastToAnyFloat | OPTF_CheckIsOneOrGreaterFloatConst }, ORT_FirstArgumentType };
		S[UO_ASin] 						= UnaryFloat;
		S[UO_ASinh] 					= UnaryFloat;
		S[UO_ATan] 						= UnaryFloat;
		S[UO_ATanh] 					= { { OPTF_CheckIsArithmetic | OPTF_CastToAnyFloat | OPTF_CheckIsBetweenMinusOneAndPlusOneFloatConst }, ORT_FirstArgumentType };
		S[UO_Ceil] 						= UnaryFloat;
		S[UO_Cos] 						= UnaryFloat;
		S[UO_Exponential]				= UnaryFloat;
		S[UO_Exponential2]				= UnaryFloat;
		S[UO_Floor] 					= UnaryFloat;
		S[UO_Frac]						= UnaryFloat;
		S[UO_IsFinite]					= UnaryFloatToBoolean;
		S[UO_IsInf]						= UnaryFloatToBoolean;
		S[UO_IsNan]						= UnaryFloatToBoolean;
		S[UO_Length]					= { { OPTF_CheckIsArithmetic | OPTF_CheckIsNotMatrix | OPTF_CastToAnyFloat }, ORT_FirstArgumentTypeToScalar };
		S[UO_Logarithm] 				= { { OPTF_CheckIsArithmetic | OPTF_CheckIsNonZeroFloatConst | OPTF_CheckIsNonNegativeFloatConst | OPTF_CastToAnyFloat }, ORT_FirstArgumentType };
		S[UO_Logarithm10]				= { { OPTF_CheckIsArithmetic | OPTF_CheckIsNonZeroFloatConst | OPTF_CheckIsNonNegativeFloatConst | OPTF_CastToAnyFloat }, ORT_FirstArgumentType };
		S[UO_Logarithm2] 				= { { OPTF_CheckIsArithmetic | OPTF_CheckIsNonZeroFloatConst | OPTF_CheckIsNonNegativeFloatConst | OPTF_CastToAnyFloat }, ORT_FirstArgumentType };
		S[UO_Round] 					= UnaryFloat;
		S[UO_Saturate]					= UnaryFloat;
		S[UO_Sign]						= UnaryFloat;
		S[UO_Sin] 						= UnaryFloat;
		S[UO_Sqrt]						= { { OPTF_CheckIsArithmetic | OPTF_CheckIsNonNegativeFloatConst | OPTF_CastToAnyFloat }, ORT_FirstArgumentType };
		S[UO_Tan] 						= UnaryFloat;
		S[UO_Tanh] 						= UnaryFloat;
		S[UO_Truncate]					= UnaryFloat;

		/* binary operators */
		S[BO_Equals]					= { { OPTF_CastToCommonType, OPTF_CastToCommonType }, ORT_BooleanWithFirstArgumentDimensions };
		S[BO_GreaterThan]				= BinaryArithmeticComparison;
		S[BO_GreaterThanOrEquals]		= BinaryArithmeticComparison;
		S[BO_LessThan]					= BinaryArithmeticComparison;
		S[BO_LessThanOrEquals]			= BinaryArithmeticComparison;
		S[BO_NotEquals]					= { { OPTF_CastToCommonType, OPTF_CastToCommonType }, ORT_BooleanWithFirstArgumentDimensions };
		
		S[BO_And] 						= BinaryLogical;
		S[BO_Or] 						= BinaryLogical;
		S[BO_Add] 						= BinaryArithmetic;
		S[BO_Subtract] 					= BinaryArithmetic;
		S[BO_Multiply] 					= BinaryArithmetic;
		S[BO_Divide] 					= BinaryArithmetic;
		S[BO_Modulo] 					= BinaryInteger;
		S[BO_BitwiseAnd]				= BinaryInteger;
		S[BO_BitwiseOr]					= BinaryInteger;
		S[BO_BitShiftLeft]				= BinaryInteger;
		S[BO_BitShiftRight]				= BinaryInteger;

		S[BO_Cross] 					= { { OPTF_CheckIsArithmetic | OPTF_CheckIsVector3, OPTF_CastToFirstArgumentType }, ORT_FirstArgumentType };
		S[BO_Distance]					= BinaryFloat;
		S[BO_Dot] 						= { { OPTF_CheckIsArithmetic | OPTF_CheckIsNotMatrix, OPTF_CastToFirstArgumentType }, ORT_FirstArgumentTypeToScalar };
		S[BO_Fmod] 						= BinaryFloat;
		S[BO_Max] 						= BinaryArithmetic;
		S[BO_Min] 						= BinaryArithmetic;
		S[BO_Pow] 						= BinaryFloat;
		S[BO_Step] 						= BinaryArithmetic;

		/* ternary operators */
		S[TO_Clamp]						= TernaryArithmetic;
		S[TO_Lerp] 						= TernaryFloat;
		S[TO_Select]					= { { OPTF_CheckIsBoolean | OPTF_CheckIsNotMatrix, OPTF_CheckIsNotMatrix, OPTF_CheckIsNotMatrix}, ORT_SecondArgumentType }; // Note: this is a special operator, which is handled manually in the Validate function
		S[TO_Smoothstep]				= TernaryFloat;
		return S;
	} ();
	return &Signatures[Op];
}
 
/// Validates that the types of the arguments are valid for specified operator.
/// If valid, it returns the type of the result. Otherwise if it is not valid, it returns nullptr.
static const FPrimitiveType* ValidateOperatorAndGetResultType(FEmitter& Emitter, EOperator Op, FValueRef& A, FValueRef& B, FValueRef& C)
{
	// Argument A must have always been provided.
	check(A);

	// Assert that if C is specified, B must too.
	check(!C || B);

	// Verify that B argument has been provided if operator is binary.
	check(!MIR::IsBinaryOperator(Op) || B);

	// Verify that C argument has been provided if operator is ternary.
	check(!MIR::IsTernaryOperator(Op) || C);

	// Verify that the first argument type is primitive.
	FValueRef Arguments[] = { A, B, C, nullptr };
	const FPrimitiveType* FirstArgumentPrimitiveType = A->Type->AsPrimitive();
	static const TCHAR* ArgumentsStr[] = { TEXT("first"), TEXT("second"), TEXT("third") };
	const FOperatorSignature* Signature = GetOperatorSignature(Op);
	const FType* ArgumentsCommonType = FirstArgumentPrimitiveType;

	bool bValid = true;
	for (int32 i = 0; Arguments[i]; ++i)
	{
		// Check this argument type i primitive.
		Arguments[i] = Emitter.CheckIsPrimitive(Arguments[i]);
		if (!Arguments[i].IsValid())
		{
			return nullptr;
		}
		const FPrimitiveType* ArgumentPrimitiveType = Arguments[i]->Type->AsPrimitive();

		EOperatorParameterTypeFilter Filter = Signature->ParameterTypes[i];
		check(Filter != OPTF_Unknown); // No signature specified for this operator.

		if (Filter & OPTF_CastToFirstArgumentType)
		{
			check(i > 0); // This check can't apply to the first argument.

			// Cast this argument to the first argument primitive type
			ArgumentPrimitiveType = FirstArgumentPrimitiveType;
			Arguments[i] = Emitter.Cast(Arguments[i], ArgumentPrimitiveType);
			bValid &= Arguments[i];
		}
		else if (Filter & OPTF_CastToAnyFloat)
		{
			if (!ScalarKindIsAnyFloat(ArgumentPrimitiveType->ScalarKind))
			{
				ArgumentPrimitiveType = ArgumentPrimitiveType->WithScalarKind(MIR::ScalarKind_Float);
				Arguments[i] = Emitter.Cast(Arguments[i], ArgumentPrimitiveType);
				bValid &= Arguments[i];
			}
		}
		
		if (Filter & OPTF_CheckIsBoolean)
		{
			if (ArgumentPrimitiveType->ScalarKind != ScalarKind_Bool)
			{
				Emitter.Errorf(Arguments[i], TEXT("Expected a boolean."));
				bValid = false;
			}
		}
		
		if (Filter & OPTF_CheckIsArithmetic)
		{
			bValid &= Emitter.CheckIsArithmetic(Arguments[i]).IsValid(); 
		}

		if (Filter & OPTF_CheckIsInteger)
		{
			bValid &= Emitter.CheckIsInteger(Arguments[i]).IsValid(); 
		}
		
		if (Filter & OPTF_CheckIsNotMatrix)
		{
			bValid &= Emitter.CheckIsScalarOrVector(Arguments[i]).IsValid(); 
		}

		if (Filter & OPTF_CheckIsVector3)
		{
			if (!ArgumentPrimitiveType->IsVector() || ArgumentPrimitiveType->GetNumComponents() != 3)
			{
				Emitter.Errorf(Arguments[i], TEXT("Expected a 3D vector."));
				bValid = false;
			}
		}

		if (FConstant* Constant = Arguments[i]->As<FConstant>())
		{
			if (Filter & OPTF_CheckIsNonZeroFloatConst)
			{
				check((Filter & OPTF_CastToAnyFloat) || (Filter & OPTF_CastToCommonFloatType));
				if (Constant->Float == 0)
				{
					Emitter.Errorf(Arguments[i], TEXT("Expected non-zero value."));
					bValid = false;
				}
			}

			if (Filter & OPTF_CheckIsNonNegativeFloatConst)
			{
				check((Filter & OPTF_CastToAnyFloat) || (Filter & OPTF_CastToCommonFloatType));
				if (Constant->Float < 0)
				{
					Emitter.Errorf(Arguments[i], TEXT("Expected non-negative value."));
					bValid = false;
				}
			}

			if (Filter & OPTF_CheckIsOneOrGreaterFloatConst)
			{
				check((Filter & OPTF_CastToAnyFloat) || (Filter & OPTF_CastToCommonFloatType));
				if (Constant->Float < 1)
				{
					Emitter.Errorf(Arguments[i], TEXT("Expected a value equal or greater than 1."));
					bValid = false;
				}
			}

			if (Filter & OPTF_CheckIsBetweenMinusOneAndPlusOneFloatConst)
			{
				check((Filter & OPTF_CastToAnyFloat) || (Filter & OPTF_CastToCommonFloatType));
				if (Constant->Float < -1 || Constant->Float > 1)
				{
					Emitter.Errorf(Arguments[i], TEXT("Expected a value greater than -1 and lower than 1."));
					bValid = false;
				}
			}
		}
		
		// Update the common type.
		if (i >= 1)
		{
			ArgumentsCommonType = Emitter.TryGetCommonType(ArgumentsCommonType, ArgumentPrimitiveType);
		}
	}
	
	// The select operator is special insofar as its first argument is a boolean, while the second and third can be any primitive type.
	if (Op == TO_Select)
	{
		// Cast the second and third argument types to primitive. This is safe as it was already checked earlier.
		const FPrimitiveType* BPrimitiveType = static_cast<const FPrimitiveType*>(Arguments[1]->Type);
		const FPrimitiveType* CPrimitiveType = static_cast<const FPrimitiveType*>(Arguments[2]->Type);

		// Compute the maximum number of vector components between all arguments. We know they're scalar or vectors, as it was checked before.
		int32 MaxNumComponents = FMath::Max3(FirstArgumentPrimitiveType->GetNumComponents(), BPrimitiveType->GetNumComponents(), CPrimitiveType->GetNumComponents());

		// Cast the first argument (the boolean condition) to a bool vector of the maximum number of components.
		Arguments[0] = Emitter.Cast(Arguments[0], FPrimitiveType::Get(ScalarKind_Bool, MaxNumComponents, 1));

		// Compute the common type between the second and third argument types with a number of components equal to the max of all three.
		const FType* CommonTypeBetweenSecondAndThirdArguments = Emitter.TryGetCommonType(
			FPrimitiveType::Get(BPrimitiveType->ScalarKind, MaxNumComponents, 1),
			FPrimitiveType::Get(CPrimitiveType->ScalarKind, MaxNumComponents, 1));
		
		// Getting the common type should always be possible.
		check(CommonTypeBetweenSecondAndThirdArguments);

		// Cast second and third arguments to their common type.
		Arguments[1] = Emitter.Cast(Arguments[1], CommonTypeBetweenSecondAndThirdArguments);
		Arguments[2] = Emitter.Cast(Arguments[2], CommonTypeBetweenSecondAndThirdArguments);

		// Update the valid flag to the results of the conditions above.
		bValid &= !Arguments[0]->IsPoison() && !Arguments[1]->IsPoison() && !Arguments[2]->IsPoison();
	}
	else
	{
		// Cast every argument with the `CastToCommon` to the common type, if necessary.
		for (int32 i = 0; Arguments[i]; ++i)
		{
			EOperatorParameterTypeFilter Filter = Signature->ParameterTypes[i];
			if (Filter & OPTF_CastToCommonType)
			{
				check(ArgumentsCommonType && ArgumentsCommonType->AsPrimitive());
				Arguments[i] = Emitter.Cast(Arguments[i], ArgumentsCommonType);
				bValid &= !Arguments[i]->IsPoison();
			}
		}
	}

	// Arguments might have changed, update the references.
	A = Arguments[0];
	B = Arguments[1];
	C = Arguments[2];

	if (!bValid)
	{
		return {};
	}
	
	// Update the first argument type, as it might have changed.
	FirstArgumentPrimitiveType = Arguments[0]->Type->AsPrimitive();

	// Finally, determine operator result type.
	switch (Signature->ReturnType)
	{
		case ORT_Unknown:
			UE_MIR_UNREACHABLE(); // missing operator signature declaration

		case ORT_FirstArgumentType:
			return FirstArgumentPrimitiveType;

		case ORT_BooleanWithFirstArgumentDimensions:
			return FPrimitiveType::Get(ScalarKind_Bool, FirstArgumentPrimitiveType->NumRows, FirstArgumentPrimitiveType->NumColumns);

		case ORT_FirstArgumentTypeToScalar:
			return FPrimitiveType::GetScalar(FirstArgumentPrimitiveType->ScalarKind);

		case ORT_SecondArgumentType:
			return static_cast<const FPrimitiveType*>(B->Type);
	}

	UE_MIR_UNREACHABLE();
}

/// Returns whether the operator supports componentwise application. In other words, if the following is true:
/// 	op(v, w) == [op(v_0, w_0), ..., op(v_n, w_n)]
static bool IsComponentwiseOperator(EOperator Op)
{
	return Op != BO_Dot && Op != BO_Cross;
}

/// Tries to fold the operator by applying the operator componentwise on arguments components.
/// If a value is returned, it will be a dimensional with some component folded to a constant. If some argument
/// isn't a dimensional, or all arguments components are non-constant, the folding will not be carried out.
/// If no folding is carried out, this function simply returns nullptr.
static FValue* TryFoldComponentwiseOperator(FEmitter& Emitter, EOperator Op, FValue* A, FValue* B, FValue* C, const FPrimitiveType* ResultType)
{
	// Check that at least one component of the resulting dimensional value would folded.
	// If all components of resulting dimensional value are not folded, then instead of emitting
	// an individual operator instruction for each component, simply emit a single binary operator
	// instruction applied between lhs and rhs as a whole. (v1 + v2 rather than float2(v1.x + v2.x, v1.y + v2.y)
	bool bSomeResultComponentWasFolded = false;
	bool bResultIsIdenticalToA = true;
	bool bResultIsIdenticalToB = true;
	bool bResultIsIdenticalToC = true;

	// Allocate the temporary array to store the folded component results
	FMemMark Mark(FMemStack::Get());
	TArrayView<FValue*> TempResultComponents = MakeTemporaryArray<FValue*>(Mark, ResultType->GetNumComponents());
	
	for (int32 i = 0; i < ResultType->GetNumComponents(); ++i)
	{
		// Extract the arguments individual components
		FValue* AComponent = Emitter.Subscript(A, i);
		FValue* BComponent = B ? Emitter.Subscript(B, i) : nullptr;
		FValue* CComponent = C ? Emitter.Subscript(C, i) : nullptr;

		// Try folding the operation, it may return null
		FValue* ResultComponent = TryFoldOperatorScalar(Emitter, Op, AComponent, BComponent, CComponent);

		// Update the flags
		bSomeResultComponentWasFolded |= (bool)ResultComponent;
		bResultIsIdenticalToA &= ResultComponent && ResultComponent->Equals(AComponent);
		bResultIsIdenticalToB &= BComponent && ResultComponent && ResultComponent->Equals(BComponent);
		bResultIsIdenticalToC &= CComponent && ResultComponent && ResultComponent->Equals(CComponent);

		// Cache the results
		TempResultComponents[i] = ResultComponent;
	}

	// If result is identical to either lhs or rhs, simply return it
	if (bResultIsIdenticalToA)
	{
		return A;
	}
	else if (bResultIsIdenticalToB)
	{
		return B;
	}
	else if (bResultIsIdenticalToC)
	{
		return C;
	}

	// If some component was folded (it is either constant or the operation was a NOP), it is worth
	// build the operation as a separate operation for each component, that is like
	//    float2(a.x + b.x, a.y + b.y)
	// rather than
	//    a + b
	// so that we retain as much compile-time information as possible.
	if (bSomeResultComponentWasFolded)
	{
		// If result type is scalar, simply return the single folded result (instead of creating a dimensional value)
		if (ResultType->IsScalar())
		{
			check(TempResultComponents[0]);
			return TempResultComponents[0];
		}

		// Make the new dimensional value
		FDimensional* Result = NewDimensionalValue(Emitter, ResultType);

		// Fetch the components array from the result dimensional
		TArrayView<FValue*> ResultComponents = Result->GetMutableComponents();

		// Also cache the type of a single component
		const FPrimitiveType* ComponentType = ResultType->ToScalar();

		// Create the operator instruction for each component pair
		for (int32 i = 0; i < ResultType->GetNumComponents(); ++i)
		{
			// Reuse cached result if possible
			ResultComponents[i] = TempResultComponents[i];

			// Otherwise emit the binary operation between the two components (this will create a new instruction)
			if (!ResultComponents[i])
			{
				FOperator Proto = MakePrototype<FOperator>(ComponentType);
				Proto.Op = Op;
				Proto.AArg = Emitter.Subscript(A, i);
				Proto.BArg = B ? Emitter.Subscript(B, i) : nullptr;
				Proto.CArg = C ? Emitter.Subscript(C, i) : nullptr;
				ResultComponents[i] = EmitPrototype(Emitter, Proto);
			}
		}
		
		return EmitNew(Emitter, Result);
	}

	return {};
}

/// If V is a dimensional and all its components are constants, it unpacks the components into OutComponents and returns true.
/// If this is not possible for any reason, it returns false.
static bool TryUnpackConstantScalarOrVector(FValue* V, TArrayView<FConstant*> OutComponents, int32& OutNumComponents)
{
	// V not specified? Or not a scalar/vector?
	FDimensional* Dimensional = As<FDimensional>(V);
	if (!Dimensional || V->Type->AsPrimitive()->IsMatrix())
	{
		return false;
	}

	TConstArrayView<FValue*> Components = Dimensional->GetComponents();
	for (int32 i = 0; i < Components.Num(); ++i)
	{
		OutComponents[i] = As<FConstant>(Components[i]);
		if (!OutComponents[i])
		{
			return false;
		}
	}

	OutNumComponents = Components.Num();
	return true;
}

/// Computes the dot product on two arrays of constant float components.
static MIR::TFloat ConstantDotFloat(TArrayView<FConstant*> AComponents, TArrayView<FConstant*> BComponents, int32 NumComponents)
{
	float Result = 0.0f;
	for (int32 i = 0; i < NumComponents; ++i)
	{
		Result += AComponents[i]->Float * BComponents[i]->Float;
	}
	return Result;
}

/// Tries to fold the operator, that is to evaluate its result now at translation time if its arguments are constant.
/// If the operator could not be folded in any way, it returns nullptr.
static FValue* TryFoldOperator(FEmitter& Emitter, EOperator Op, FValue* A, FValue* B, FValue* C, const FPrimitiveType* ResultType)
{
	FConstant* AComponents[4];
	int32      ANumComponents;
	
	// Some operations like Length, Dot and Cross are not defined on individual scalar components.
	// For instance length(V) is not the same as [length(V.x), ..., length(V.z)]. These operations
	// folding is handled here as special cases.
	// First, try to unpack the first argument to an array of constants.
	if (TryUnpackConstantScalarOrVector(A, AComponents, ANumComponents))
	{
		FConstant* BComponents[4];
		int32      BNumComponents;
	
		if (Op == UO_Length)
		{
			if (ResultType->ScalarKind == ScalarKind_Float)
			{
				float Result = FMath::Sqrt(ConstantDotFloat(AComponents, AComponents, ANumComponents));
				return Emitter.ConstantFloat(Result);
			}
			else
			{
				UE_MIR_UNREACHABLE();
			}
		}
		else if ((Op == BO_Dot || Op == BO_Cross) && TryUnpackConstantScalarOrVector(B, BComponents, BNumComponents))
		{
			// Verified before the operation is folded, here as a safety check.
			check(ANumComponents == BNumComponents);

			if (Op == BO_Dot)
			{
				if (ResultType->ScalarKind == ScalarKind_Float)
				{
					float Result = ConstantDotFloat(AComponents, BComponents, ANumComponents);
					return Emitter.ConstantFloat(Result);
				}
				else
				{
					UE_MIR_UNREACHABLE();
				}
			}
			else
			{
				check(Op == BO_Cross);
				if (ResultType->ScalarKind == ScalarKind_Float)
				{
					FVector3f AVector{ AComponents[0]->Float, AComponents[1]->Float, AComponents[2]->Float };
					FVector3f BVector{ BComponents[0]->Float, BComponents[1]->Float, BComponents[2]->Float };
					FVector3f Result = AVector.Cross(BVector);
					return Emitter.ConstantFloat3(Result);
				}
				else
				{
					UE_MIR_UNREACHABLE();
				}
			}
		}
	}

	// If the operation supports componentwise application, try folding the operator componentwise.
	if (IsComponentwiseOperator(Op))
	{
		return TryFoldComponentwiseOperator(Emitter, Op, A, B, C, ResultType);
	}

	// No folding was possible, simply return null to indicate this.
	return nullptr;
}

FValueRef FEmitter::Operator(EOperator Op, FValueRef A, FValueRef B, FValueRef C)
{
	if (!A.IsValid() || (B && !B.IsValid()) || (C && !C.IsValid()))
	{
		return Poison();
	}

	// Validate the operation and retrieve the result type.
	const FPrimitiveType* ResultType = ValidateOperatorAndGetResultType(*this, Op, A, B, C);
	if (!ResultType)
	{
		return Poison();
	}

	// Try to apply some operator identity to simplify the operator.
	if (FValueRef Simplified = TrySimplifyOperator(*this, Op, A, B, C))
	{
		return Simplified;
	}

	// Try folding the operator first.
	if (FValue* FoldedValue = TryFoldOperator(*this, Op, A, B, C, ResultType))
	{
		return FoldedValue;
	}

	// Otherwise, we must emit a new instruction that executes the operator.
	FOperator Proto = MakePrototype<FOperator>(ResultType);
	Proto.Op = Op;
	Proto.AArg = A;
	Proto.BArg = B;
	Proto.CArg = C;

	return EmitPrototype(*this, Proto);
}

FValueRef FEmitter::Branch(FValueRef Condition, FValueRef True, FValueRef False)
{
	if (IsAnyNotValid(Condition, True, False))
	{
		return Poison();
	}

	// Condition must be of type bool
	Condition = Cast(Condition, FPrimitiveType::GetBool());
	if (!Condition)
	{
		return Poison();
	}	

	// If the condition is a scalar constant, then simply evaluate the result now.
	if (const FConstant* ConstCondition = As<FConstant>(Condition))
	{
		return ConstCondition->Boolean ? True : False;
	}

	// If the condition is not static, make both true and false arguments have the same type,
	// by casting false argument into the true's type.
	const FType* CommonType = GetCommonType(True->Type, False->Type);
	if (!CommonType)
	{
		return Poison();
	}

	True = Cast(True, CommonType);
	False = Cast(False, CommonType);
	if (!True || !False)
	{
		return Poison();
	}

	// Create the branch instruction.
	FBranch Proto = MakePrototype<FBranch>(CommonType);
	Proto.ConditionArg = Condition;
	Proto.TrueArg = True;
	Proto.FalseArg = False;

	return EmitPrototype(*this, Proto);
}

FValueRef FEmitter::Subscript(FValueRef Value, int32 Index)
{
	if (!Value.IsValid())
	{
		return Value;
	}

	const FPrimitiveType* PrimitiveType = Value->Type->AsPrimitive();
	if (!PrimitiveType)
	{
		Errorf(Value, TEXT("Value of type '%s' cannot be subscripted."), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}

	// Getting first component and Value is already a scalar, just return itself.
	if (Index == 0 && Value->Type->AsScalar())
	{
		return Value;
	}

	if (Index >= PrimitiveType->GetNumComponents())
	{
		Errorf(Value, TEXT("Value of type '%s' has fewer dimensions than subscript index `%d`."), Value->Type->GetSpelling().GetData(), Index);
		return Value.ToPoison();
	}

	// Getting first component and Value is already a scalar, just return itself.
	if (FDimensional* DimensionalValue = As<FDimensional>(Value))
	{
		check(Index < DimensionalValue->GetComponents().Num());
		return Value.To(DimensionalValue->GetComponents()[Index]);
	}
	
	// Avoid subscripting a subscript (e.g. no value.xy.x)
	if (FSubscript* Subscript = As<FSubscript>(Value))
	{
		Value = Value.To(Subscript->Arg);
	}

	// We can't resolve it at compile time: emit subscript value.
	FSubscript Prototype = MakePrototype<FSubscript>(PrimitiveType->ToScalar());
	Prototype.Arg = Value;
	Prototype.Index = Index;

	return Value.To(EmitPrototype(*this, Prototype));
}

FValueRef FEmitter::Swizzle(FValueRef Value, FSwizzleMask Mask)
{
	if (!Value.IsValid())
	{
		return Value;
	}

	// At least one component must have been specified.
	check(Mask.NumComponents > 0);

	// We can only swizzle on non-matrix primitive types.
	const FPrimitiveType* PrimitiveType = Value->Type->AsVector();
	if (!PrimitiveType || PrimitiveType->IsMatrix())
	{
		Errorf(Value, TEXT("Cannot swizzle a '%s' value."), Value->Type->GetSpelling().GetData());
		return Value.ToPoison();
	}

	// Make sure each component in the mask fits the number of components in Value.
	for (EVectorComponent Component : Mask)
	{
		if ((int32)Component >= PrimitiveType->NumRows)
		{
			Errorf(Value, TEXT("Value of type '%s' has no component '%s'."), PrimitiveType->Spelling.GetData(), VectorComponentToString(Component));
			return Value.ToPoison();
		}
	}

	// If the requested number of components is the same as Value and the order in which the components
	// are specified in the mask is sequential (e.g. x, y, z) then this is a no op, simply return Value as is.
	if (Mask.NumComponents == PrimitiveType->GetNumComponents())
	{
		bool InOrder = true;
		for (int32 i = 0; i < Mask.NumComponents; ++i)
		{
			if (Mask.Components[i] != (EVectorComponent)i)
			{
				InOrder = false;
				break;
			}
		}

		if (InOrder)
		{
			return Value;
		}
	}
	
	// If only one component is requested, we can use Subscript() to return the single component.
	if (Mask.NumComponents == 1)
	{
		return Value.To(Subscript(Value, (int32)Mask.Components[0]));
	}

	// Make the result vector type.
	const FPrimitiveType* ResultType = FPrimitiveType::GetVector(PrimitiveType->ScalarKind, Mask.NumComponents);
	FDimensional* Result = NewDimensionalValue(*this, ResultType);
	TArrayView<FValue*> ResultComponents = Result->GetMutableComponents();

	for (int32 i = 0; i < Mask.NumComponents; ++i)
	{
		ResultComponents[i] = Subscript(Value, (int32)Mask.Components[i]);
	}

	return Value.To(EmitNew(*this, Result));
}

static FValue* CastConstant(FEmitter& Emitter, FConstant* Constant, EScalarKind ConstantScalarKind, EScalarKind TargetKind)
{
	if (ConstantScalarKind == TargetKind)
	{
		return Constant;
	}

	switch (ConstantScalarKind)
	{
		case ScalarKind_Bool:
		case ScalarKind_Int:
		{
			switch (TargetKind)
			{
				case ScalarKind_Bool: return {};
				case ScalarKind_Int: return Emitter.ConstantInt(Constant->Integer);
				case ScalarKind_Float: return Emitter.ConstantFloat((TFloat)Constant->Integer);
				default: UE_MIR_UNREACHABLE();
			}
		}

		case ScalarKind_Float:
		{
			switch (TargetKind)
			{
				case ScalarKind_Bool: return {};
				case ScalarKind_Int: return Emitter.ConstantInt((int32)Constant->Float);
				default: UE_MIR_UNREACHABLE();
			}
		}

		default: break;
	}

	UE_MIR_UNREACHABLE();
}

static FValue* CastValueToPrimitiveType(FEmitter& Emitter, FValueRef Value, const FPrimitiveType* TargetPrimitiveType)
{
	const FPrimitiveType* ValuePrimitiveType = Value->Type->AsPrimitive();
	if (!ValuePrimitiveType)
	{
		Emitter.Errorf(Value, TEXT("Cannot construct a '%s' from non primitive type '%s'."), TargetPrimitiveType->Spelling.GetData(), Value->Type->GetSpelling().GetData());
		return FPoison::Get();
	}

	// Construct a scalar from another scalar.
	if (TargetPrimitiveType->IsScalar())
	{
		// 
		Value = Emitter.Subscript(Value, 0);
		ValuePrimitiveType = Value->Type->AsPrimitive();
		
		//
		if (ValuePrimitiveType == TargetPrimitiveType)
		{
			return Value;
		}

		// Construct the scalar from a constant.
		if (FConstant* ConstantInitializer = As<FConstant>(Value))
		{
			return CastConstant(Emitter, ConstantInitializer, ValuePrimitiveType->ScalarKind, TargetPrimitiveType->ScalarKind);
		}
		else
		{
			// Emit the cast to the target type of the subscript value
			FCast Prototype = MakePrototype<FCast>(TargetPrimitiveType);
			Prototype.Arg = Value;
			return EmitPrototype(Emitter, Prototype);
		}
	}

	// Construct a vector or matrix from a scalar. E.g. 3.14f -> float3(3.14f, 3.14f, 3.14f)
	// Note: we know target isn't scalar as it's been handled above.
	if (ValuePrimitiveType->IsScalar())
	{
		// Create the result dimensional value.
		FDimensional* Result = NewDimensionalValue(Emitter, TargetPrimitiveType);

		// Create a dimensional and initialize each of its components to the conversion
		// of initializer value to the single component type.
		FValue* Component = Emitter.Cast(Value, TargetPrimitiveType->ToScalar());

		// Get the mutable array of components.
		TArrayView<FValue*> ResultComponents = Result->GetMutableComponents();

		// Initialize all result components to the same scalar.
		for (int32 i = 0; i < TargetPrimitiveType->GetNumComponents(); ++i)
		{
			ResultComponents[i] = Component;
		}
		
		return EmitNew(Emitter, Result);
	}

	// Construct a vector from another vector. If constructed vector is larger, initialize
	// remaining components to zero. If it's smaller, truncate initializer vector and only use
	// the necessary components.
	if (TargetPrimitiveType->IsVector() && ValuePrimitiveType->IsVector())
	{
		// #todo-massimo.tristano Use swizzle when scalartypes are the same, and target num components is less than initializer's.

		int32 TargetNumComponents = TargetPrimitiveType->GetNumComponents();
		int32 InitializerNumComponents = ValuePrimitiveType->GetNumComponents();

		// Create the result dimensional value.
		FDimensional* Result = NewDimensionalValue(Emitter, TargetPrimitiveType);
		TArrayView<FValue*> ResultComponents = Result->GetMutableComponents();

		// Determine the result component type (scalar).
		const FPrimitiveType* ResultComponentType = TargetPrimitiveType->ToScalar();

		// For iterating over the components of the result dimensional value.
		int32 Index = 0;
		
		// Convert components from the initializer vector.
		const int32 MinNumComponents = FMath::Min(TargetNumComponents, InitializerNumComponents);
		for (; Index < MinNumComponents; ++Index)
		{
			ResultComponents[Index] = Emitter.Cast(Emitter.Subscript(Value, Index), ResultComponentType);
		}

		// Initialize remaining result dimensional components to zero.
		for (; Index < TargetNumComponents; ++Index)
		{
			ResultComponents[Index] = Emitter.ConstantZero(ResultComponentType->ScalarKind);
		}

		return EmitNew(Emitter, Result);
	}
	
	// The two primitive types are identical matrices that differ only by their scalar type.
	if (TargetPrimitiveType->NumRows 	 == ValuePrimitiveType->NumRows &&
		TargetPrimitiveType->NumColumns == ValuePrimitiveType->NumColumns)
	{
		check(TargetPrimitiveType->IsMatrix());

		//  If initializer is a dimensional, we can subscript components of interest.
		if (FDimensional* DimensionalInitializer = As<FDimensional>(Value))
		{
			// Create the result dimensional value.
			FDimensional* Result = NewDimensionalValue(Emitter, TargetPrimitiveType);
			TArrayView<FValue*> ResultComponents = Result->GetMutableComponents();
			
			// Determine the result component type (scalar).
			const FPrimitiveType* ResultComponentType = TargetPrimitiveType->ToScalar();

			// Convert components from the initializer vector.
			for (int32 Index = 0, Num = TargetPrimitiveType->GetNumComponents(); Index < Num; ++Index)
			{
				ResultComponents[Index] = Emitter.Cast(DimensionalInitializer->GetComponents()[Index], ResultComponentType);
			}

			return EmitNew(Emitter, Result);
		}
		else
		{
			// Initializer is an unknown value, construct target value casting initializer.
			FCast Prototype = MakePrototype<FCast>(TargetPrimitiveType);
			Prototype.Arg = Value;
			return EmitPrototype(Emitter, Prototype);
		}
	}

	// Initializer value cannot be used to construct this primitive type.
	return Emitter.Poison();
}

FValueRef FEmitter::Cast(FValueRef Value, const FType* TargetType)
{
	if (!Value.IsValid())
	{
		return Value;
	}

	// If target type matches initializer's, simply return the same value.
	const FType* InitializerType = Value->Type;
	if (InitializerType == TargetType)
	{
		return Value;
	}
	
	FValue* Result;
	if (const FPrimitiveType* PrimitiveType = TargetType->AsPrimitive())
	{
		Result = CastValueToPrimitiveType(*this, Value, PrimitiveType);
	}
	else // No other legal conversions applicable. Report error if we haven't converted the value.
	{
		Errorf(Value, TEXT("Cannot construct a '%s' from a '%s'."), TargetType->GetSpelling().GetData(), Value->Type->GetSpelling().GetData());
		return Poison();
	}
	
	return Result;
}

FValueRef FEmitter::CastToScalar(FValueRef Value)
{
	Value = CheckIsPrimitive(Value);
	return Value.IsValid()
		? Cast(Value, FPrimitiveType::GetScalar(Value->Type->AsPrimitive()->ScalarKind))
		: Value;
}

FValueRef FEmitter::CastToBool(FValueRef Value, int NumRows)
{
	return Cast(Value, FPrimitiveType::GetVector(ScalarKind_Bool, NumRows));
}

FValueRef FEmitter::CastToInt(FValueRef Value, int NumRows)
{
	return Cast(Value, FPrimitiveType::GetVector(ScalarKind_Int, NumRows));
}

FValueRef FEmitter::CastToFloat(FValueRef Value, int NumRows)
{
	return Cast(Value, FPrimitiveType::GetVector(ScalarKind_Float, NumRows));
}

FValueRef FEmitter::StageSwitch(const FType* Type, TConstArrayView<FValueRef> ValuePerStage)
{
	check(ValuePerStage.Num() <= NumStages);
	FStageSwitch Prototype = MakePrototype<FStageSwitch>(Type);
	for (int i = 0; i < ValuePerStage.Num(); ++i)
	{
		Prototype.Args[i] = ValuePerStage[i];
	}
	return EmitPrototype(*this, Prototype);
}

FValueRef FEmitter::TextureGather(FValueRef Texture, FValueRef TexCoord, ETextureReadMode GatherMode, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType)
{
	check(GatherMode >= ETextureReadMode::GatherRed && GatherMode <= ETextureReadMode::GatherAlpha);

	if (IsAnyNotValid(Texture, TexCoord))
	{
		return Poison();
	}

	FTextureRead Prototype = MakePrototype<FTextureRead>(FPrimitiveType::GetFloat4());
	Prototype.TextureObject = Texture;
	Prototype.TexCoord = TexCoord;
	Prototype.Mode = GatherMode;
	Prototype.SamplerSourceMode = SamplerSourceMode;
	Prototype.SamplerType = SamplerType;

	return EmitPrototype(*this, Prototype);
}

FValueRef FEmitter::TextureSample(FValueRef Texture, FValueRef TexCoord, bool bAutomaticViewMipBias, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType)
{
	if (IsAnyNotValid(Texture, TexCoord))
	{
		return Poison();
	}

	FTextureRead PrototypeHw = MakePrototype<FTextureRead>(FPrimitiveType::GetFloat4());
	PrototypeHw.TextureObject = Texture;
	PrototypeHw.TexCoord = TexCoord;
	PrototypeHw.Mode = ETextureReadMode::MipAuto;
	PrototypeHw.SamplerSourceMode = SamplerSourceMode;
	PrototypeHw.SamplerType = SamplerType;

	FTextureRead PrototypeAn = PrototypeHw;
	PrototypeAn.Mode = ETextureReadMode::Derivatives;
	PrototypeAn.TexCoordDdx = AnalyticalPartialDerivative(TexCoord, EDerivativeAxis::X);
	PrototypeAn.TexCoordDdy = AnalyticalPartialDerivative(TexCoord, EDerivativeAxis::Y);	

	if (bAutomaticViewMipBias)
	{
		FValueRef ViewMaterialTextureMipBias = ExternalInput(EExternalInput::ViewMaterialTextureMipBias);
		PrototypeHw.Mode = ETextureReadMode::MipBias;
		PrototypeHw.MipValue = ViewMaterialTextureMipBias;

		FValueRef Exp2ViewMaterialTextureMipBias = Operator(UO_Exponential2, ViewMaterialTextureMipBias);
		PrototypeAn.TexCoordDdx = Operator(BO_Multiply, PrototypeAn.TexCoordDdx, Exp2ViewMaterialTextureMipBias);
		PrototypeAn.TexCoordDdy = Operator(BO_Multiply, PrototypeAn.TexCoordDdy, Exp2ViewMaterialTextureMipBias);
	}

	FStageSwitch StageSwitch = MakePrototype<FStageSwitch>(PrototypeHw.Type);
	StageSwitch.SetArgs(EmitPrototype(*this, PrototypeHw), EmitPrototype(*this, PrototypeAn));

	return EmitPrototype(*this, StageSwitch);
}

FValueRef FEmitter::TextureSampleLevel(FValueRef Texture, FValueRef TexCoord, FValueRef MipLevel, bool bAutomaticViewMipBias, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType)
{
	if (IsAnyNotValid(Texture, TexCoord, MipLevel))
	{
		return Poison();
	}

	FTextureRead Prototype = MakePrototype<FTextureRead>(FPrimitiveType::GetFloat4());
	Prototype.TextureObject = Texture;
	Prototype.TexCoord = TexCoord;
	Prototype.MipValue = MipLevel;
	Prototype.Mode = ETextureReadMode::MipLevel;
	Prototype.SamplerSourceMode = SamplerSourceMode;
	Prototype.SamplerType = SamplerType;

	if (bAutomaticViewMipBias)
	{
		Prototype.MipValue = Operator(BO_Add, MipLevel, ExternalInput(EExternalInput::ViewMaterialTextureMipBias));
	}

	return EmitPrototype(*this, Prototype);
}

FValueRef FEmitter::TextureSampleBias(FValueRef Texture, FValueRef TexCoord, FValueRef MipBias, bool bAutomaticViewMipBias, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType)
{
	if (IsAnyNotValid(Texture, TexCoord, MipBias))
	{
		return Poison();
	}

	if (bAutomaticViewMipBias)
	{
		MipBias = Operator(BO_Add, MipBias, ExternalInput(EExternalInput::ViewMaterialTextureMipBias));
	}

	FTextureRead PrototypeHw = MakePrototype<FTextureRead>(FPrimitiveType::GetFloat4());
	PrototypeHw.TextureObject = Texture;
	PrototypeHw.TexCoord = TexCoord;
	PrototypeHw.MipValue = MipBias;
	PrototypeHw.Mode = ETextureReadMode::MipBias;
	PrototypeHw.SamplerSourceMode = SamplerSourceMode;
	PrototypeHw.SamplerType = SamplerType;

	FTextureRead PrototypeAn = PrototypeHw;
	PrototypeAn.Mode = ETextureReadMode::Derivatives;

	FValueRef Exp2MipBias = Operator(UO_Exponential2, MipBias);
	PrototypeAn.TexCoordDdx = Operator(BO_Multiply, AnalyticalPartialDerivative(TexCoord, EDerivativeAxis::X), Exp2MipBias);
	PrototypeAn.TexCoordDdy = Operator(BO_Multiply, AnalyticalPartialDerivative(TexCoord, EDerivativeAxis::Y), Exp2MipBias);	

	FStageSwitch StageSwitch = MakePrototype<FStageSwitch>(PrototypeHw.Type);
	StageSwitch.SetArgs(EmitPrototype(*this, PrototypeHw), EmitPrototype(*this, PrototypeAn));

	return EmitPrototype(*this, StageSwitch);
}

FValueRef FEmitter::TextureSampleGrad(FValueRef Texture, FValueRef TexCoord, FValueRef TexCoordDdx, FValueRef TexCoordDdy, bool bAutomaticViewMipBias, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType)
{
	if (IsAnyNotValid(Texture, TexCoord, TexCoordDdx, TexCoordDdy))
	{
		return Poison();
	}

	FTextureRead Prototype = MakePrototype<MIR::FTextureRead>(FPrimitiveType::GetFloat4());
	Prototype.TextureObject = Texture;
	Prototype.TexCoord = TexCoord;
	Prototype.TexCoordDdx = TexCoordDdx;
	Prototype.TexCoordDdy = TexCoordDdy;
	Prototype.Mode = ETextureReadMode::Derivatives;
	Prototype.SamplerSourceMode = SamplerSourceMode;
	Prototype.SamplerType = SamplerType;

	if (bAutomaticViewMipBias)
	{
		FValueRef ViewMaterialTextureDerivativeMultiply = ExternalInput(EExternalInput::ViewMaterialTextureDerivativeMultiply);
		Prototype.TexCoordDdx = Operator(BO_Multiply, Prototype.TexCoordDdx, ViewMaterialTextureDerivativeMultiply);
		Prototype.TexCoordDdy = Operator(BO_Multiply, Prototype.TexCoordDdy, ViewMaterialTextureDerivativeMultiply);
	}

	return EmitPrototype(*this, Prototype);
}

FValueRef FEmitter::PartialDerivative(FValueRef Value, EDerivativeAxis Axis)
{
	// Any operation on poison arguments is a poison.
	if (!Value.IsValid())
	{
		return Value;
	}

	// Differentiation is only valid on primitive types.
	const MIR::FPrimitiveType* ValuePrimitiveType = Value->Type->AsPrimitive();
	if (!ValuePrimitiveType || !ScalarKindIsAnyFloat(ValuePrimitiveType->ScalarKind))
	{
		Errorf(Value, TEXT("Trying to differentiate a value of type `%s` is invalid. Expected a float type."), Value->Type->GetSpelling().GetData());
		return Poison();
	}

	// Make the hardware derivative instruction.
	FHardwarePartialDerivative HwDerivativeProto = MakePrototype<FHardwarePartialDerivative>(Value->Type);
	HwDerivativeProto.Arg = Value;
	HwDerivativeProto.Axis = Axis;
	FValueRef HwDerivative = EmitPrototype(*this, HwDerivativeProto);

	// Compute the analytical derivative for stages that don't support hardware derivatives.
	FValueRef AnalyticalDerivative = AnalyticalPartialDerivative(Value, Axis);

	// Emit the stage switch instruction so that hardware derivatives are used on stages
	// that support it and analytical deriatives in the other stages.
	FValueRef StageValues[NumStages] = {};
	for (int i = 0; i < NumStages; ++i)
	{
		StageValues[i] = (i == Stage_Pixel) ? HwDerivative : AnalyticalDerivative;
	}
	return StageSwitch(Value->Type, StageValues);
}

static FValue* DifferentiateExternalInput(FEmitter& Emitter, FExternalInput* ExternalInput, EDerivativeAxis Axis)
{
	// Texture coordinate external inputs have their own matching DDX/DDY inputs.
	if (IsExternalInputTexCoord(ExternalInput->Id))
	{
		int32 TexCoordIndex = ExternalInputToTexCoordIndex(ExternalInput->Id);

		EExternalInput PartialDerivativeExternalInput = (Axis == EDerivativeAxis::X) 
			? EExternalInput((int32)EExternalInput::TexCoord0_Ddx + TexCoordIndex)
			: EExternalInput((int32)EExternalInput::TexCoord0_Ddy + TexCoordIndex);

		return Emitter.ExternalInput(PartialDerivativeExternalInput);
	}

	// All other inputs are assumed constant.
	const MIR::FPrimitiveType* PrimType = static_cast<const MIR::FPrimitiveType*>(ExternalInput->Type);
	return Emitter.Cast(Emitter.ConstantZero(PrimType->ScalarKind), PrimType);
}

static FValue* DifferentiateOperator(FEmitter& E, FOperator* Op, EDerivativeAxis Axis)
{
	const MIR::FPrimitiveType* PrimType = static_cast<const MIR::FPrimitiveType*>(Op->Type);

	// Considering an operator acting on f(x), g(x) and h(x) arguments (e.g. "f(x) + g(x)"),
	// calculate base terms and 
	FValue* F = Op->AArg;
	FValue* G = Op->BArg;
	FValue* H = Op->CArg;
	FValue* dF = F && !F->Type->IsBoolean() ? E.AnalyticalPartialDerivative(F, Axis) : nullptr; // Note: select's first argument is a boolean, avoid making the derivative then
	FValue* dG = G ? E.AnalyticalPartialDerivative(G, Axis) : nullptr;
	FValue* dH = H ? E.AnalyticalPartialDerivative(H, Axis) : nullptr;

	// Convenience local functions as multiplications and division operations are common in derivatives.
	auto Zero = [&E, PrimType] () { return E.ConstantZero(PrimType->ScalarKind); };
	auto One = [&E, PrimType] () { return E.ConstantOne(PrimType->ScalarKind); };
	auto Constant = [&E, PrimType] (TFloat FromFloat) { return E.ConstantScalar(PrimType->ScalarKind, FromFloat); };

	// Some constants
	constexpr TFloat Ln2 = 0.69314718055994530941723212145818;
	constexpr TFloat Ln10 = 2.3025850929940456840179914546844;
	
	switch (Op->Op)
	{
		// d/dx -f(x) = -f'(x)
		case UO_Negate:
			return E.Negate(dF);

		// d/dx |f(x)| = f(x) f'(x) / |f(x)|
		case UO_Abs:
			return E.Divide(E.Multiply(F, dF), Op);

		// d/dx arccos(f(x)) = -1 / sqrt(1 - f(x)^2) * f'(x)
		case UO_ACos:
			return E.Negate(E.Divide(dF, E.Sqrt(E.Subtract(One(), E.Multiply(F, F)))));
			
		// d/dx acosh(f(x)) = 1 / sqrt(f(x)^2 - 1) * f'(x)
		case UO_ACosh:
			return E.Divide(dF, E.Sqrt(E.Subtract(E.Multiply(F, F), One())));
			
		// d/dx arcsin(f(x)) = 1 / sqrt(1 - f(x)^2) * f'(x)
		case UO_ASin:
			return E.Divide(dF, E.Sqrt(E.Subtract(One(), E.Multiply(F, F))));
		
		// d/dx asinh(f(x)) = 1 / sqrt(f(x)^2 + 1) * f'(x)
		case UO_ASinh:
			return E.Divide(dF, E.Sqrt(E.Add(E.Multiply(F, F), One())));

		// d/dx arctan(f(x)) = 1 / (1 + f(x)^2) * f'(x)
		case UO_ATan:
			return E.Divide(dF, E.Add(One(), E.Multiply(F, F)));

    	// d/dx atanh(f(x)) = f'(x) / (1 - f(x)^2)
		case UO_ATanh:
    		return E.Divide(dF, E.Subtract(One(), E.Multiply(F, F)));

		// d/dx cos(f(x)) = -sin(f(x)) * f'(x)
		case UO_Cos:
			return E.Negate(E.Multiply(E.Sin(F), dF));

		// d/dx cosh(f(x)) = sinh(f(x)) * f'(x)
		case UO_Cosh:
			return E.Multiply(E.Sinh(F), dF);

		// d/dx e^f(x) = e^f(x) * f'(x)
		case UO_Exponential:
			return E.Multiply(Op, dF);

		// d/dx 2^f(x) = ln(2) * 2^f(x) * f'(x)
		case UO_Exponential2:
			return E.Multiply(E.Multiply(Constant(Ln2), Op), dF);

		// d/dx frac(f(x)) = f'(x), since frac(x) = x - floor(x)
		case UO_Frac:
			return dF;

		// d/dx |f(x)| (length in vector case) = f(x) f'(x) / |f(x)|
		case UO_Length:
			return E.Divide(E.Multiply(F, dF), Op);

		// d/dx log(f(x)) = 1 / f(x) * f'(x)
		case UO_Logarithm:
			return E.Divide(dF, F);

		// d/dx log2(f(x)) = 1 / (f(x) * ln(2)) * f'(x)
		case UO_Logarithm2:
			return E.Divide(dF, E.Multiply(F, Constant(Ln2)));

		// d/dx log10(f(x)) = 1 / (f(x) * ln(10)) * f'(x)
		case UO_Logarithm10:
			return E.Divide(dF, E.Multiply(F, Constant(Ln10)));

		// d/dx saturate(f(x)) = f'(x) if f(x) is inside (0-1) range, 0 otherwise
		case UO_Saturate:
			return E.Select(E.And(
							E.LessThan(Zero(), F), // 0 < f(x)
							E.LessThan(F, One())), // f(x) < 1
						dF, Zero());

		// d/dx sin(f(x)) = cos(f(x)) * f'(x)
		case UO_Sin:
			return E.Multiply(E.Cos(F), dF);
		
		// d/dx sinh(f(x)) = cosh(f(x)) * f'(x)
		case UO_Sinh:
			return E.Multiply(E.Cosh(F), dF);

		// d/dx sqrt(f(x)) = 1 / (2 * sqrt(f(x))) * f'(x)
		case UO_Sqrt:
			return E.Divide(dF, E.Multiply(Constant(2), E.Sqrt(F)));

		// d/dx tan(f(x)) = 1 / cos^2(f(x)) * f'(x)
		case UO_Tan:
		{
			FValue* CosVal = E.Cos(F);
			return E.Divide(dF, E.Multiply(CosVal, CosVal));
		}
		
		// d/dx tanh(f(x)) = (1 - tanh(f(x))^2) * f'(x)
		case UO_Tanh:
			return E.Multiply(E.Subtract(One(), E.Multiply(Op, Op)), dF);

		// These functions are piecewise constant, that is mostly constant with some
		// discontinuities. We assume they're always constant, as they're not differentiable
		// at the discontinuities.
		case UO_Ceil:
		case UO_Floor:
		case UO_Round:
		case UO_Truncate:
			return Zero();

		// d/dx (f(x) + g(x)) = f'(x) + g'(x)
		case BO_Add:
			return E.Add(dF, dG);

		// d/dx (f(x) - g(x)) = f'(x) - g'(x)
		case BO_Subtract:
			return E.Subtract(dF, dG);

		// d/dx (f(x) * g(x)) = f'(x) * g(x) + f(x) * g'(x)
		case BO_Multiply:
			return E.Add(E.Multiply(dF, G), E.Multiply(F, dG));

		// d/dx (f(x) / g(x)) = (f'(x) * g(x) - f(x) * g'(x)) / g(x)^2
		case BO_Divide:
			return E.Divide(E.Subtract(E.Multiply(dF, G), E.Multiply(F, dG)), E.Multiply(G, G));

		// fmod(f(x), g(x)) = f(x) - g(x) * floor(f(x) / g(x)).
		// Thus:
		//     d/dx fmod(f(x), g(x)) = f'(x) - g(x) * floor(f(x) / g(x))
		// since `floor` is piecewise constant.
		case BO_Fmod:
			return E.Subtract(dF, E.Multiply(dG, E.Operator(UO_Floor, E.Divide(F, G))));

		// d/dx max(f(x), g(x)) = f'(x) if f(x) > g(x), else g'(x)
		case BO_Max:
			return E.Select(E.Operator(BO_GreaterThan, F, G), dF, dG);

		// d/dx min(f(x), g(x)) = f'(x) if f(x) < g(x), else g'(x)
		case BO_Min:
			return E.Select(E.LessThan( F, G), dF, dG);

		// d/dx pow(f(x), g(x)) = f(x)^g(x) * (g'(x) * ln(f(x)) + g(x) * f'(x) / f(x))
		case BO_Pow:
		{
			FValueRef Term1 = E.Multiply(dG, E.Logarithm(F)); // g'(x) * ln(f(x))
			FValueRef Term2 = E.Divide(E.Multiply(G, dF), F); // g(x) * f'(x) / f(x)
			return E.Multiply(Op, E.Add(Term1, Term2));
		}

		// The multiplication rule applies for the dot product too.
		// d/dx (f(x) • g(x)) = f'(x) • g(x) + f(x) • g'(x)
		case BO_Dot:
			return E.Add(E.Operator(BO_Dot, dF, G), E.Operator(BO_Dot, F, dG));

		// The multiplication rule applies for the cross product too.
		// d/dx (f(x) × g(x)) = f'(x) × g(x) + f(x) × g'(x)
		case BO_Cross:
			return E.Add(E.Operator(BO_Cross, dF, G), E.Operator(BO_Cross, F, dG));

		// clamp(x, min, max) (F=x, min=G, max=H)
		// The derivative is defined when x is between min and max (f'(x)). At and outside
		// bounds, the clamp result is constant and thus the derivative is zero.
		case TO_Clamp:
			return E.Select(E.And(
					E.LessThan(G, F), 
					E.LessThan(F, H)),
				dF, Zero());

		// lerp(a, b, t) = a + t * (b - a)
		// d/dx lerp(f(x), g(x), h(x)) = f'(x) + d/dx (h(x) * ((g(x) - f(x)))
		// d/dx (h(x) * ((g(x) - f(x))) = h'(x) * ((g(x) - f(x))) + h(x) * (g'(x) - f'(x))
		case TO_Lerp:
			return E.Add(dF, 
					   E.Add(E.Multiply(dH, E.Subtract(G, F)), 
						   E.Multiply(H, E.Subtract(dG, dF))));

		// d/dx select(F, g(x), h(x)) ≈ select(F, g'(x), h'(x))
		case TO_Select:
			return E.Select(F, dG, dH);

		// smoothstep(f(x), g(x), h(x)) = 3 z^2 - 2 z^3  with z = saturate((h - f) / (g - f))
		case TO_Smoothstep:
		{
			FValue* Z  = E.Saturate(E.Divide(E.Subtract(H, F), E.Subtract(G, F)));
			FValue* dZ = E.AnalyticalPartialDerivative(Z, Axis);
			// d/dx 3 z(x)^2 - 2 z(x)^3 = 6 * z(x) * z'(x) - 6 * z(x)^2 * z'(x) = 6 * (z(x) - z(x)^2) * z'(x)
			return E.Multiply(dZ, E.Multiply(Constant(6), E.Subtract(Z, E.Multiply(Z, Z))));
		}

		// these are either invalid or constant
		case UO_BitwiseNot:
		case UO_IsFinite:
		case UO_IsInf:
		case UO_IsNan:
		case UO_Sign:
		case BO_Modulo:
		case BO_BitwiseAnd:
		case BO_BitwiseOr:
		case BO_BitShiftLeft:
		case BO_BitShiftRight:
		case BO_Step:
			return Zero();

		default:
			UE_MIR_UNREACHABLE();
	}
}

FValueRef FEmitter::AnalyticalPartialDerivative(FValueRef Value, EDerivativeAxis Axis)
{
	// Any operation on poison arguments is a poison.
	if (!Value.IsValid())
	{
		return Value;
	}

	// Differentiation is only valid on primitive types.
	const MIR::FPrimitiveType* ValuePrimitiveType = Value->Type->AsPrimitive();
	if (!ValuePrimitiveType || !ScalarKindIsAnyFloat(ValuePrimitiveType->ScalarKind))
	{
		Errorf(Value, TEXT("Trying to differentiate a value of type `%s` is invalid. Expected a float type."), Value->Type->GetSpelling().GetData());
		return Poison();
	}

	switch (Value->Kind)
	{
		case MIR::VK_ExternalInput:
			return DifferentiateExternalInput(*this, Value->As<FExternalInput>(), Axis);

		case MIR::VK_Dimensional:
		{
			TConstArrayView<FValue*> ValueComponents = Value->As<FDimensional>()->GetComponents();
			FDimensional* Derivative = NewDimensionalValue(*this, ValuePrimitiveType);
			TArrayView<FValue*> DerivativeComponents = Derivative->GetMutableComponents();
			for (int32 i = 0; i < ValueComponents.Num(); ++i)
			{
				DerivativeComponents[i] = AnalyticalPartialDerivative(ValueComponents[i], Axis);
			}
			return Derivative;
		}

		case MIR::VK_Operator:
			return DifferentiateOperator(*this, Value->As<FOperator>(), Axis);
		
		case MIR::VK_Branch:
		{
			FBranch* AsBranch = Value->As<FBranch>();
			return Branch(AsBranch->ConditionArg,
				AnalyticalPartialDerivative(AsBranch->TrueArg, Axis),
				AnalyticalPartialDerivative(AsBranch->FalseArg, Axis));
		}

		case MIR::VK_Subscript:
		{
			FSubscript* AsSubscript = Value->As<FSubscript>();
			return Subscript(AnalyticalPartialDerivative(AsSubscript->Arg, Axis), AsSubscript->Index);
		}

		case MIR::VK_Cast:
		{
			FCast* AsCast = Value->As<FCast>();
			return Cast(AnalyticalPartialDerivative(AsCast->Arg, Axis), AsCast->Type);
		}

		// These values are uniform (constant), thus their value is always zero.
		case MIR::VK_Constant:
		case MIR::VK_UniformParameter:
		case MIR::VK_TextureRead:
		case MIR::VK_InlineHLSL:
			return Cast(ConstantZero(ValuePrimitiveType->ScalarKind), ValuePrimitiveType);

		default:
			UE_MIR_UNREACHABLE();
	}
}

static FValueRef EmitInlineHLSL(FEmitter& Emitter, const FType* Type, const FMaterialExternalCodeDeclaration* InExternalCodeDeclaration, const TCHAR* Code, TConstArrayView<FValueRef> InputValues, EValueFlags ValueFlags, EGraphProperties UsedGraphProperties)
{
	MIR::FInlineHLSL Prototype = MakePrototype<MIR::FInlineHLSL>(Type);
	Prototype.Type = Type;
	Prototype.Flags = ValueFlags;
	Prototype.GraphProperties = UsedGraphProperties;
	
	if (InExternalCodeDeclaration)
	{
		check(!Code);
		Prototype.ExternalCodeDeclaration = InExternalCodeDeclaration;
	}
	else
	{
		Prototype.Code = Code;
	}

	if (!InputValues.IsEmpty())
	{
		checkf(InputValues.Num() < MIR::FInlineHLSL::MaxNumArguments, TEXT("Number of arguments for inline-HLSL out of bounds: %d was specified, but upper bound is %d"), InputValues.Num(), MIR::FInlineHLSL::MaxNumArguments);
		Prototype.NumArguments = InputValues.Num();
		for (int32 i = 0; i < InputValues.Num(); ++i)
		{
			Prototype.NumArguments = InputValues[i];
		}
	}

	return EmitPrototype(Emitter, Prototype);
}

FValueRef FEmitter::InlineHLSL(const FType* Type, FString Code, TConstArrayView<FValueRef> InputValues, EValueFlags ValueFlags, EGraphProperties UsedGraphProperties)
{
	if (IsAnyNotValid(InputValues))
	{
		return Poison();
	}
	
	return EmitInlineHLSL(*this, Type, nullptr, Module->PushUserString(MoveTemp(Code)), InputValues, ValueFlags | EValueFlags::HasDynamicHLSLCode, UsedGraphProperties);
}

FValueRef FEmitter::InlineHLSL(const FMaterialExternalCodeDeclaration* InExternalCodeDeclaration, TConstArrayView<FValueRef> InputValues, EValueFlags ValueFlags, EGraphProperties UsedGraphProperties)
{
	if (IsAnyNotValid(InputValues))
	{
		return Poison();
	}

	check(InExternalCodeDeclaration != nullptr);
	const FType* ReturnType = FType::FromMaterialValueType(InExternalCodeDeclaration->GetReturnTypeValue());
	return EmitInlineHLSL(*this, ReturnType, InExternalCodeDeclaration, nullptr, InputValues, ValueFlags, UsedGraphProperties);
}

const FType* FEmitter::TryGetCommonType(const FType* A, const FType* B)
{
	// Trivial case: types are equal
	if (A == B)
	{
		return A;
	}

	const FPrimitiveType* PrimitiveA = A->AsPrimitive();
	const FPrimitiveType* PrimitiveB = B->AsPrimitive();
	if (!PrimitiveA || !PrimitiveB)
	{
		return nullptr;
	}
	
	// If both A and B are matrices, their dimensions must match (equality check above didn't trigger).
	if (PrimitiveA->IsMatrix() || PrimitiveB->IsMatrix())
	{
		return nullptr;
	}

	// Neither A nor B are matrices, but single scalar or vector. Return the largest.
	check(PrimitiveA->NumColumns == 1 && PrimitiveB->NumColumns == 1);
	EScalarKind ScalarKind = FMath::Max(PrimitiveA->ScalarKind, PrimitiveB->ScalarKind);
	int32 NumRows = FMath::Max(PrimitiveA->NumRows, PrimitiveB->NumRows);
	return FPrimitiveType::Get(ScalarKind, NumRows, 1);
}

void FEmitter::Initialize()
{
	// Create and reference the true/false constants.
	FConstant Temp = MakePrototype<FConstant>(FPrimitiveType::GetBool());

	Temp.Boolean = true;
	TrueConstant = EmitPrototype(*this, Temp);

	Temp.Boolean = false;
	FalseConstant = EmitPrototype(*this, Temp);
}

bool FEmitter::FValueKeyFuncs::Matches(KeyInitType A, KeyInitType B)
{
	return A->Equals(B);
}

uint32 FEmitter::FValueKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return Internal::HashBytes((char*)Key, Key->GetSizeInBytes());
}

} // namespace MIR

#endif // #if WITH_EDITOR
