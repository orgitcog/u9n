// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

#define UE_MIR_CHECKPOINT(Emitter) if (Emitter.CurrentExpressionHasErrors()) return

namespace MIR {

enum class EVectorComponent : uint8_t
{
	X, Y, Z, W, 
};

// Return the lower case string representation of given component (e.g. "x")
const TCHAR* VectorComponentToString(EVectorComponent);

// This utility data structure is used to define how a swizzle operation should be performed.
struct FSwizzleMask
{
	// Which component should be extracted from the argument, in order. E.g. [Z, Z, X] to model "MyVec.zzx".
	EVectorComponent Components[4];

	// How many components have been defined for swizzle should be made of (maximum four).
	int NumComponents{};

	// Convenience ".xyz" swizzle mask.
	static FSwizzleMask XYZ();

	FSwizzleMask() {}
	FSwizzleMask(EVectorComponent X);
	FSwizzleMask(EVectorComponent X, EVectorComponent Y);
	FSwizzleMask(EVectorComponent X, EVectorComponent Y, EVectorComponent Z);
	FSwizzleMask(EVectorComponent X, EVectorComponent Y, EVectorComponent Z, EVectorComponent W);

	// Pushes a component to this swizzle mask.
	void Append(EVectorComponent Component);

	const EVectorComponent* begin() const { return Components; }
	const EVectorComponent* end() const { return Components + NumComponents; }
};

// A lightweight wrapper around FValue* that also optionally tracks the source expression input.
// Used to carry both value and its origin in the expression graph.
// A null reference evaluates has the FValue* set to nullptr and evaluates false. It is
// returned by the FEmitter::TryInput() value to indicate a missing value. An invalid reference
// is either null or has a poison value. It is usually generated as the result of an invalid
// operation.
// Important: The Emitter APIs can safely take both null and poison values. Operations that
// take a single argument simply return the argument if it is invalid (null or poison),
// whereas operations that take multiple arguments return poison if any is invalid.
//
// When your intention is to test for whether a value is not-null (available), leverage the
// operator bool()
//
//		if (Value) { /* handle case when value is available */ }
//
// Otherwise, if you would like the value to be available AND valid (not-poison) you should
// test for IsValid()
//
//		if (Value.IsValid()) {
//			/* handle case when the operations that produced the value were all succesful */
//		}
//
// Note: In user Expression::Build() implementations, you should avoid testing values validity
// directly and instead leverage the UE_MIR_CHECKPOINT() macro to verify that all emitter
// performed so far were all valid, and thus the values you have computed so far are what
// you expect.
struct FValueRef
{
	// Default constructor. Creates an invalid reference.
	FValueRef() {}

	// Constructs from a value pointer.
	FValueRef(FValue* Value) : Value{ Value } {}

	// Constructs from a value pointer and its corresponding input source.
	FValueRef(FValue* Value, const FExpressionInput* Input) : Value{ Value }, Input{ Input } {}

	// Returns whether this reference is valid, that is it references to a non-null value.
	operator bool() const { return Value != nullptr; }

	// Returns the referenced value, which must be non-null.
	operator FValue*() const { return Value; }

	// Returns the referenced value, which must be non-null.
	FValue* operator->() const { return Value; }
	
	// Checks if the reference holds a valid non-null value.
	bool IsValid() const;

	// Creates a new reference with the same input but a different value.
	FValueRef To(FValue* Value) const;

	// Creates a poison version of the current reference (same input, but poison value).
	FValueRef ToPoison() const;
	
	// The referenced value.
	FValue* Value{};

	// The expression input being read.
	const FExpressionInput* Input{};
};

// The FEmitter is a helper object responsible for emitting MIR values and instructions.
// It is primarily used within UMaterialExpression::Build() implementations to lower the
// high-level semantics of a material expression into corresponding lower-level MIR nodes.
//
// During the emission process, the emitter automatically performs various
// simplifications and constant folding optimizations where possible.
//
// An instance of the emitter is created and managed by the FMaterialIRModuleBuilder.
// This instance is then passed to the UMaterialExpression::Build() functions
// so that they can emit their corresponding IR values.
//
// IMPORTANT: Users should *not* assume the exact MIR::FType of the FValue* returned
// by EmitXXX() functions. The emitter is free to optimize operations, which may
// result in a returned value having a different type than naively expected (e.g.,
// folding an operation into a constant).
//
// The emitter handles invalid inputs gracefully. If an operation is attempted
// with invalid arguments (e.g., acting on arguments of unexpected types), the emitter
// automatically reports an error and returns the MIR::FPoison value. Furthermore, all
// EmitXXX() functions are robust to receiving MIR::FPoison as input; they will simply
// propagate the poison value without attempting the operation.
class FEmitter
{
public:
	/*--------------------------------- Error handling ---------------------------------*/

	// Returns whether the current expression generated an error.
	// You can also use UE_MIR_CHECKPOINT() in your code to check this and automatically return if an error occurred.
	bool CurrentExpressionHasErrors() const { return bCurrentExpressionHasErrors; }

	// Reports an error using printf-like format and arguments to format the error message.
	template <typename... TArgs>
	void Errorf(FValueRef Source, UE::Core::TCheckedFormatString<FString::FmtCharType, TArgs...> Format, TArgs... Args)
	{
		Error(Source, FString::Printf(Format, Args...));
	}

	// Reports an error using printf-like format and arguments to format the error message.
	template <typename... TArgs>
	void Errorf(UE::Core::TCheckedFormatString<FString::FmtCharType, TArgs...> Format, TArgs... Args)
	{
		Error(FString::Printf(Format, Args...));
	}

	// Reports an error with given error message.
	void Error(FValueRef Source, FStringView Message);

	// Reports an error with given error message.
	void Error(FStringView Message);

	/*--------------------------------- Type handling ----------------------------------*/

	// Tries to find a common type between A and B.
	// The trivial case is when A and B are the same type, in which case that type is
	// the common type. When A and B are primitive, the common type is defined as the type
	// with the highest dimensions between the two and with a scalar kind that follows the
	// progression: bool -> int -> float
	// As an example, the common type between a float3 and a bool4 will be a float4.
	// Note; that this may cause a loss of data when casting from int to float.
	// It returns the common type if it exists, nullptr otherwise.
	const FType* TryGetCommonType(const FType* A, const FType* B);

	// Gets the common type between A and B and reports an error if no such type exists.
	// See TryGetCommonType()
	const FType* GetCommonType(const FType* A, const FType* B);

	/*-------------------------------- Input management --------------------------------*/

	// Retrieves the value from the given input, if present. A null reference otherwise.
	// Note: it does not report an error if the value is missing.
	// It is safe to pass returned value to other Emitter functions as argument even if null.
	// For instance, the following is safe and idiomatic
	//     
	//    FValueRef Value = Emitter.CheckIsArithmetic(Emitter.TryInput(&A));
	//    UE_MIR_CHECKPOINT(Emitter);
	//    if (Value) { /* handle case when value is provided */ }
	// 
	// Keep in mind that a reference can be null (its Value is null) or invalid (which means
	// that it's null OR poison).
	FValueRef TryInput(const FExpressionInput* Input);

	// Retrieves the value flowing into given input.
	// If the value is missing, it reports an error and returns poison.
	FValueRef Input(const FExpressionInput* Input);

	// If the input has no value flowing in (e.g., because the input is disconnected),
	// these DefaultXXX() functions will emit a constant value, bind it to the input, and
	// return it.
	// Note that these functions may return poison if the value flowing into this input
	// was poison in	FValueRef DefaultBool(FValueRef Value, bool Default);
	FValueRef InputDefaultBool(const FExpressionInput* Input, bool Default);
	FValueRef InputDefaultInt(const FExpressionInput* Input, TInteger Default);
	FValueRef InputDefaultInt2(const FExpressionInput* Input, UE::Math::TIntVector2<TInteger> Default);
	FValueRef InputDefaultInt3(const FExpressionInput* Input, UE::Math::TIntVector3<TInteger> Default);
	FValueRef InputDefaultInt4(const FExpressionInput* Input, UE::Math::TIntVector4<TInteger> Default);
	FValueRef InputDefaultFloat(const FExpressionInput* Input, TFloat Default);
	FValueRef InputDefaultFloat2(const FExpressionInput* Input, UE::Math::TVector2<TFloat> Default);
	FValueRef InputDefaultFloat3(const FExpressionInput* Input, UE::Math::TVector<TFloat> Default);
	FValueRef InputDefaultFloat4(const FExpressionInput* Input, UE::Math::TVector4<TFloat> Default);

	// Retrieves the value from the given input of given type kind.
	// If the value is missing or its type does not have the given kind, it reports an error and returns poison.
	FValueRef CheckTypeIsKind(FValueRef Value, ETypeKind Kind);
	
	// Validates that the value is of a primitive type. If not, reports an error and returns poison.
	FValueRef CheckIsPrimitive(FValueRef Value);
	
	// Validates that the value is of an arithmetic type (int or float). Reports error and returns poison otherwise.
	FValueRef CheckIsArithmetic(FValueRef Value);

	// Validates that the value is of a integral type. If not, reports an error and returns poison.
	FValueRef CheckIsInteger(FValueRef Value);

	// Validates that the value is a scalar. If not, reports error and returns poison.
	FValueRef CheckIsScalar(FValueRef Value);

	// Validates that the value is either a scalar or a vector. Reports error and returns poison otherwise.
	FValueRef CheckIsScalarOrVector(FValueRef Value);
	
	// Retrieves the texture value flowing into given input.
	// If the value is missing or cannot be cast to a texture, it reports an error and returns poison.
	FValueRef CheckIsTexture(FValueRef Value);

	// Casts the value flowing into this input to a constant boolean and returns it.
	// If the value is missing, or is not a constant boolean it reports an error and returns false.
	bool ToConstantBool(FValueRef Value);

	/*-------------------------------- Output management -------------------------------*/
	
	// Flows given `Value` out of the expression output with given `OutputIndex`.
	FEmitter& Output(int32 OutputIndex, FValueRef Value);

	// Flows given `Value` out of given expression `Output`.
	FEmitter& Output(const FExpressionOutput* Output, FValueRef Value);
	
	/*------------------------------- Constants emission -------------------------------*/

	// Converts the given UE::Shader::FValue to a MIR constant value and returns it.
	FValueRef ConstantFromShaderValue(const UE::Shader::FValue& InValue);

	// Emits a scalar constant value of given kind with value 0 and returns it.
	FValueRef ConstantZero(EScalarKind Kind);

	// Emits a scalar constant value of given kind with value 1 and returns it.
	FValueRef ConstantOne(EScalarKind Kind);
	
	// Casts the given float value to the given scalar kind and returns its value.
	FValueRef ConstantScalar(EScalarKind Kind, TFloat FromFloat);

	// Returns the constant boolean scalar `true`.
	FValueRef ConstantTrue();

	// Returns the constant boolean scalar `false`.
	FValueRef ConstantFalse();

	// Returns the given constant boolean scalar.
	FValueRef ConstantBool(bool InX);

	// Returns the given constant integer scalar.
	FValueRef ConstantInt(TInteger InX);
	
	// Emits a constant integer 2D column vector and returns it.
	FValueRef ConstantInt2(UE::Math::TIntVector2<TInteger> InValue);
	
	// Emits a constant integer 3D column vector and returns it.
	FValueRef ConstantInt3(UE::Math::TIntVector3<TInteger>  InValue);

	// Emits a constant integer 4D column vector and returns it.
	FValueRef ConstantInt4(UE::Math::TIntVector4<TInteger>  InValue);

	// Returns the given constant float scalar.
	FValueRef ConstantFloat(TFloat InX);

	// Emits a constant float 2D column vector and returns it.
	FValueRef ConstantFloat2(UE::Math::TVector2<TFloat> InValue);

	// Emits a constant float 3D column vector and returns it.
	FValueRef ConstantFloat3(UE::Math::TVector<TFloat> InValue);

	// Emits a constant float 4D column vector and returns it.
	FValueRef ConstantFloat4(UE::Math::TVector4<TFloat> InValue);
	
	/*--------------------- Other non-instruction values emission ---------------------*/

	// Emits the poison value.
	FValueRef Poison();

	// Emits an external input value of given input Id and returns it.
	FValueRef ExternalInput(EExternalInput Id);

	// Emits a texture object value of fiven sampler type and returns it.
	FValueRef TextureObject(UTexture* Texture, EMaterialSamplerType SamplerType);

	// Emits a material parameter value with given name, metadata and sampler type (if it is a texture) and returns it.
	FValueRef Parameter(FName Name, FMaterialParameterMetadata& Metadata, EMaterialSamplerType SamplerType = SAMPLERTYPE_Color);

	// Emits a custom primitive data value with given index.
	FValueRef CustomPrimitiveData(uint32 PrimitiveDataIndex);
	
	/*----------------------------- Instructions emission -----------------------------*/

	// Emits an instruction that sets the material attribute with given property to given argument.
	// Note: this API is used by the builder and you should not use it manually.
	FSetMaterialOutput* SetMaterialOutput(EMaterialProperty InProperty, FValue* Arg);

	// Emits a 2D column vector dimensional value and returns it.
	FValueRef Vector2(FValueRef InX, FValueRef InY);

	// Emits a 3D column vector dimensional value and returns it.
	FValueRef Vector3(FValueRef InX, FValueRef InY, FValueRef InZ);

	// Emits a 4D column vector dimensional value and returns it.
	FValueRef Vector4(FValueRef InX, FValueRef InY, FValueRef InZ, FValueRef InW);
	
	// Emits a mathematical operation instruction with given operator and arguments.
	// Note: this function will try to simplify the operation at translation time if possible.
	// The returned value is therefore not guaranteed to be an FOperator instruction instance.
	FValueRef Operator(EOperator Operator, FValueRef A, FValueRef B = {}, FValueRef C = {});
	
	// Emits a branch instruction.
	// When the result of the Condition argument is true, the instruction will evaluate
	// the True argument, otherwise the False argument.
	// This instruction will place the as much of the other instructions whose results
	// serve the computation of True or False within separate inner scopes, in order to
	// avoid unnecessarily computing the inactive argument.
	FValueRef Branch(FValueRef Condition, FValueRef True, FValueRef False);
		
	// Casts the given value to the specified target type. Returns poison if the cast is invalid.
	FValueRef Cast(FValueRef Value, const FType* TargetType);
	
	// Casts the given arithmetic value to a scalar. If this is a vector, it will convert to its first component.
	// Returns if the cast is invalid.
	FValueRef CastToScalar(FValueRef Value);

	// These functions cast the given value to the a primitive scalar or column vector type of given rows.
	// If the value is missing ore could not be cast to the relative primitive type, they repors an error and return poison.
	FValueRef CastToBool(FValueRef Value, int NumRows);
	FValueRef CastToInt(FValueRef Value, int NumRows);
	FValueRef CastToFloat(FValueRef Value, int NumRows);
	
	// Extracts the component with given index from the given argument value.
	// The argument value must be a scalar or a vector primitive type, otherwise it reports an error and returns poison.
	// Note that if no error occurred, the returned value is guaranteed to be a primitive scalar.
	FValueRef Subscript(FValueRef Value, int32 Index);

	// Swizzles the given argument by given mask.
	// The argument value must be a scalar or vector type. If the operation is invalid on
	// given argument and mask it reports an error and returns poison.
	FValueRef Swizzle(FValueRef Value, FSwizzleMask Mask);

	// Emits a stage switch instruction. Use this instruction to select a different value
	// in each execution stage.
	// Note: ValuePerStage must have no more than MIR::NumStages elements.
	FValueRef StageSwitch(const FType* Type, TConstArrayView<FValueRef> ValuePerStage);

	// Emits a texture gather instruction.
	// 
	// @param Texture The texture to sample. Must be of type MIR::FObjectType and represent a valid texture.
	// @param TexCoord The UV coordinates at which to sample the texture.
	// @param GatherMode What channel to gather (any ::GatherXXX value).
	// @param SamplerSourceMode Which sampler source mode to use.
	// @param SamplerType Which sampler type to use.
	FValueRef TextureGather(FValueRef Texture, FValueRef TexCoord, ETextureReadMode GatherMode, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType);

	// Emits a texture sample instruction.
	// 
	// @param Texture The texture to sample. Must be of type MIR::FObjectType and represent a valid texture.
	// @param TexCoord The UV coordinates at which to sample the texture.
	// @param bAddViewMaterialMipBias Whether to add the view material texture mip bias to the mip level.
	// @param SamplerSourceMode Which sampler source mode to use.
	// @param SamplerType Which sampler type to use.
	FValueRef TextureSample(FValueRef Texture, FValueRef TexCoord, bool bAddViewMaterialMipBias, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType);

	// Emits a texture sample instruction with a manually given mip level.
	// 
	// @param Texture The texture to sample. Must be of type MIR::FObjectType and represent a valid texture.
	// @param TexCoord The UV coordinates at which to sample the texture.
	// @param MipLevel Which mip level to use for this sample.
	// @param bAddViewMaterialMipBias Whether to add the view material texture mip bias to the mip level.
	// @param SamplerSourceMode Which sampler source mode to use.
	// @param SamplerType Which sampler type to use.
	FValueRef TextureSampleLevel(FValueRef Texture, FValueRef TexCoord, FValueRef MipLevel, bool bAddViewMaterialMipBias, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType);

	// Emits a texture sample instruction with bias added to the automatically selected mip level.
	// 
	// @param Texture The texture to sample. Must be of type MIR::FObjectType and represent a valid texture.
	// @param TexCoord The UV coordinates at which to sample the texture.
	// @param MipBias The bias to add to the automatically selected mip level.
	// @param bAddViewMaterialMipBias Whether to add the view material texture mip bias to the mip level.
	// @param SamplerSourceMode Which sampler source mode to use.
	// @param SamplerType Which sampler type to use.
	FValueRef TextureSampleBias(FValueRef Texture, FValueRef TexCoord, FValueRef MipBias, bool bAddViewMaterialMipBias, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType);
	
	// Emits a texture sample instruction using user provided partial derivatives.
	// 
	// @param Texture The texture to sample. Must be of type MIR::FObjectType and represent a valid texture.
	// @param TexCoord The UV coordinates at which to sample the texture.
	// @param TexCoordDdx The partial derivative of the texture coordinates along the screen space x axis.
	// @param TexCoordDdx The partial derivative of the texture coordinates along the screen space y axis.
	// @param bAddViewMaterialMipBias Whether to add the view material texture mip bias to the mip level.
	// @param SamplerSourceMode Which sampler source mode to use.
	// @param SamplerType Which sampler type to use.
	FValueRef TextureSampleGrad(FValueRef Texture, FValueRef TexCoord, FValueRef TexCoordDdx, FValueRef TexCoordDdy, bool bAddViewMaterialMipBias, ESamplerSourceMode SamplerSourceMode, EMaterialSamplerType SamplerType);
	
	// Emits the partial derivative of given value.
	// This function differentiates the given value with respect to the given axis.
	// If differentiation is not possible, it returns a poison value to indicate an error.
	// This API can be used regardless of the stages value will execute in. Hardware partial
	// derivatives will be used by default where available. If not available, the analytical
	// derivative will be computed instead.
	// @param Value The value to differentiate.
	// @param Axis The axis along which to compute the partial derivative (X for ddx, Y for ddy).
	// @return A new FValueRef representing the partial derivative, or a poison value if invalid.
	FValueRef PartialDerivative(FValueRef Value, EDerivativeAxis Axis);

	// Computes and emits the analytical partial derivative of a give value.
	// @param Value The value to differentiate.
	// @param Axis The axis along which to compute the partial derivative (X for ddx, Y for ddy).
	// @return A new FValueRef representing the analytical derivative, or a poison value if invalid.
	FValueRef AnalyticalPartialDerivative(FValueRef Value, EDerivativeAxis Axis);

	// Emit a new HLSL function inline within the material shader.
	// @param Type The expected output type of the HLSL code.
	// @param Code The HLSL code to be emitted.
	// @param InputValues An array of input values required by the HLSL code.
	// @param ValueFlags Flags that specify additional properties for this value (default is None).
	// @param UsedGraphProperties Properties used during graph processing (default is None).
	FValueRef InlineHLSL(const FType* Type, FString Code, TConstArrayView<FValueRef> InputValues, EValueFlags ValueFlags = EValueFlags::None, EGraphProperties UsedGraphProperties = EGraphProperties::None);

	// Emit a new HLSL function inline within the material shader based on an external code declaration.
	// @param InExternalCodeDeclaration The external HLSL code declaration to use for emission.
	// @param InputValues An array of input values required by the HLSL code.
	// @param UsedGraphProperties Properties used during graph processing (default is None).
	FValueRef InlineHLSL(const FMaterialExternalCodeDeclaration* InExternalCodeDeclaration, TConstArrayView<FValueRef> InputValues, EValueFlags ValueFlags = EValueFlags::None, EGraphProperties UsedGraphProperties = EGraphProperties::None);

	/*--------------------------- Unary operators shortcuts ----------------------------*/

	FValueRef BitwiseNot(FValueRef A) { return Operator(UO_BitwiseNot, A); }
	FValueRef Negate(FValueRef A) { return Operator(UO_Negate, A); }
	FValueRef Not(FValueRef A) { return Operator(UO_Not, A); }
	FValueRef Abs(FValueRef A) { return Operator(UO_Abs, A); }
	FValueRef ACos(FValueRef A) { return Operator(UO_ACos, A); }
	FValueRef ACosh(FValueRef A) { return Operator(UO_ACosh, A); }
	FValueRef ASin(FValueRef A) { return Operator(UO_ASin, A); }
	FValueRef ASinh(FValueRef A) { return Operator(UO_ASinh, A); }
	FValueRef ATan(FValueRef A) { return Operator(UO_ATan, A); }
	FValueRef ATanh(FValueRef A) { return Operator(UO_ATan, A); }
	FValueRef Ceil(FValueRef A) { return Operator(UO_Ceil, A); }
	FValueRef Cos(FValueRef A) { return Operator(UO_Cos, A); }
	FValueRef Cosh(FValueRef A) { return Operator(UO_Cosh, A); }
	FValueRef Exponential(FValueRef A) { return Operator(UO_Exponential, A); }
	FValueRef Exponential2(FValueRef A) { return Operator(UO_Exponential2, A); }
	FValueRef Floor(FValueRef A) { return Operator(UO_Floor, A); }
	FValueRef Frac(FValueRef A) { return Operator(UO_Frac, A); }
	FValueRef Length(FValueRef A) { return Operator(UO_Length, A); }
	FValueRef Logarithm(FValueRef A) { return Operator(UO_Logarithm, A); }
	FValueRef Logarithm2(FValueRef A) { return Operator(UO_Logarithm2, A); }
	FValueRef Logarithm10(FValueRef A) { return Operator(UO_Logarithm10, A); }
	FValueRef Round(FValueRef A) { return Operator(UO_Round, A); }
	FValueRef Saturate(FValueRef A) { return Operator(UO_Saturate, A); }
	FValueRef Sin(FValueRef A) { return Operator(UO_Sin, A); }
	FValueRef Sinh(FValueRef A) { return Operator(UO_Sinh, A); }
	FValueRef Sqrt(FValueRef A) { return Operator(UO_Sqrt, A); }
	FValueRef Tan(FValueRef A) { return Operator(UO_Tan, A); }
	FValueRef Truncate(FValueRef A) { return Operator(UO_Truncate, A); }

	/*-------------------------- Binary operators shortcuts ----------------------------*/
	
	FValueRef GreaterThan(FValueRef A, FValueRef B) { return Operator(BO_GreaterThan, A, B); }
	FValueRef GreaterThanOrEquals(FValueRef A, FValueRef B) { return Operator(BO_GreaterThanOrEquals, A, B); }
	FValueRef LessThan(FValueRef A, FValueRef B) { return Operator(BO_LessThan, A, B); }
	FValueRef LessThanOrEquals(FValueRef A, FValueRef B) { return Operator(BO_LessThanOrEquals, A, B); }
	FValueRef Equals(FValueRef A, FValueRef B) { return Operator(BO_Equals, A, B); }
	FValueRef NotEquals(FValueRef A, FValueRef B) { return Operator(BO_NotEquals, A, B); }
	FValueRef And(FValueRef A, FValueRef B) { return Operator(BO_And, A, B); }
	FValueRef Or(FValueRef A, FValueRef B) { return Operator(BO_Or, A, B); }
	FValueRef Add(FValueRef A, FValueRef B) { return Operator(BO_Add, A, B); }
	FValueRef Subtract(FValueRef A, FValueRef B) { return Operator(BO_Subtract, A, B); }
	FValueRef Multiply(FValueRef A, FValueRef B) { return Operator(BO_Multiply, A, B); }
	FValueRef Divide(FValueRef A, FValueRef B) { return Operator(BO_Divide, A, B); }
	FValueRef BitwiseAnd(FValueRef A, FValueRef B) { return Operator(BO_BitwiseAnd, A, B); }
	FValueRef BitwiseOr(FValueRef A, FValueRef B) { return Operator(BO_BitwiseOr, A, B); }
	FValueRef BitShiftLeft(FValueRef A, FValueRef B) { return Operator(BO_BitShiftLeft, A, B); }
	FValueRef BitShiftRight(FValueRef A, FValueRef B) { return Operator(BO_BitShiftRight, A, B); }
	FValueRef Fmod(FValueRef A, FValueRef B) { return Operator(BO_Fmod, A, B); }
	FValueRef Max(FValueRef A, FValueRef B) { return Operator(BO_Max, A, B); }
	FValueRef Min(FValueRef A, FValueRef B) { return Operator(BO_Min, A, B); }
	FValueRef Dot(FValueRef A, FValueRef B) { return Operator(BO_Dot, A, B); }
	FValueRef Cross(FValueRef A, FValueRef B) { return Operator(BO_Cross, A, B); }

	/*-------------------------- Ternary operators shortcuts ---------------------------*/

	FValueRef Clamp(FValueRef A, FValueRef B, FValueRef C) { return Operator(TO_Clamp, A, B, C); }
	FValueRef Lerp(FValueRef A, FValueRef B, FValueRef C) { return Operator(TO_Lerp, A, B, C); }
	FValueRef Select(FValueRef A, FValueRef B, FValueRef C) { return Operator(TO_Select, A, B, C); }
	FValueRef Smoothstep(FValueRef A, FValueRef B, FValueRef C) { return Operator(TO_Smoothstep, A, B, C); }

	// Internal
	struct FPrivate;

private:
	struct FValueKeyFuncs : DefaultKeyFuncs<FValue*>
	{
		static bool Matches(KeyInitType A, KeyInitType B);
		static uint32 GetKeyHash(KeyInitType Key);
	};

private:
	FEmitter() {}

	// Initializes the emitter, called by the friend builder.
	void Initialize();

private:
	// Pointer to the builder internal implementation.
	FMaterialIRModuleBuilderImpl* BuilderImpl{};

	// The material being translated.
	UMaterial* Material{};

	// The IR module being built.
	FMaterialIRModule* Module{};

	// The set of static parameter assignments to use during this translation.
	const FStaticParameterSet* StaticParameterSet{};

	// The current expression being built (set by the builder).
	UMaterialExpression* Expression{};

	// Whether the current expression has reported any error.
	bool bCurrentExpressionHasErrors = false;

	// Global "true" constant.
	FValue* TrueConstant{};

	// Global "false" constant.
	FValue* FalseConstant{};

	// The set of all values previously emitted. It is used to avoid duplicating identical
	// values, explointing the strict SSA data-flow paradigm of MIR to efficiently reuse
	// calculations.
	TSet<FValue*, FValueKeyFuncs> ValueSet{};

	friend FMaterialIRModuleBuilder;
	friend FMaterialIRModuleBuilderImpl;
};

} // namespace MIR

#endif // #if WITH_EDITOR
