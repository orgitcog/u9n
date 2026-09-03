// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Materials/MaterialIRCommon.h"
#include "MaterialTypes.h"

#if WITH_EDITOR

struct FMaterialExternalCodeDeclaration;

namespace MIR
{

// Identifies the block of execution in which an instruction runs.
// Note: If you introduce a new stage, make sure you update EValueFlags accordingly.
enum EStage
{
	Stage_Vertex,
	Stage_Pixel,
	Stage_Compute,
	NumStages
};

// Returns the string representation of given stage.
const TCHAR* LexToString(EStage Stage);

// Returns whether specified property is evaluated in specified stage.
bool MaterialOutputEvaluatesInStage(EMaterialProperty Property, EStage Stage);

// A collection of bit flags for a specific Value instance.
enum class EValueFlags : uint8
{
	None = 0,

	// Value has been analyzed for the vertex stage.
	AnalyzedForStageVertex = 1 << 0,

	// Value has been analyzed for the pixel stage.
	AnalyzedForStagePixel = 1 << 1,

	// Value has been analyzed for the compute stage.
	AnalyzedForStageCompute = 1 << 2,

	// Mask that includes all "analyzed in stage" flags.
	AnalyzedInAnyStageMask = AnalyzedForStageVertex | AnalyzedForStagePixel | AnalyzedForStageCompute,

	// Whether to skip substution of abstract tokens like "<PREV>". Only true for custom nodes.
	SubstituteTagsInInlineHLSL = 1 << 3,

	// Whether 'FInlineHLSL::Code' field is used. Otherwise, 'ExternalCodeDeclaration' is used.
	HasDynamicHLSLCode = 1 << 4, 

	// External code declaration must emit the secondary definition for the DDX derivative.
	DerivativeDDX = 1 << 5,

	// External code declaration must emit the secondary definition for the DDY derivative.
	DerivativeDDY = 1 << 6,
};

ENUM_CLASS_FLAGS(EValueFlags);

static_assert(uint32(1 << NumStages) - 1 == uint32(EValueFlags::AnalyzedInAnyStageMask), "Discrepancy between the number of stages in NumStages and the flags in EValueFlags.");

// A collection of graph properties used by a value.
//
// If a graph property is true, it means that either the value itself makes has of that
// property, or one of its dependencies (direct or indirect) has it. This entails these
// flags are automatically propagated to all dependant values (values that depend on a given one).
// As an example, if "ReadsPixelNormal" is true on a specific value it means that either
// that value itself or some other upstream value that value is dependent on reads
// the pixel normal.
enum class EGraphProperties : uint8
{
	None = 0,

	// Some value reads the pixel normal.
	ReadsPixelNormal = 1 << 0,
};

ENUM_CLASS_FLAGS(EGraphProperties);

// Enumeration of all different structs deriving from FValue. Used for dynamic casting.
enum EValueKind
{
	// Values

	VK_Poison,
	VK_Constant,
	VK_ExternalInput,
	VK_TextureObject,
	VK_UniformParameter,

	// Instructions

	VK_InstructionBegin,

	VK_SetMaterialOutput,
	VK_Dimensional,
	VK_Operator,
	VK_Branch,
	VK_Subscript,
	VK_Cast,
	VK_TextureRead,
	VK_InlineHLSL,
	VK_StageSwitch,
	VK_HardwarePartialDerivative,

	VK_InstructionEnd,
};

// Returns the string representation of given value kind.
const TCHAR* LexToString(EValueKind Kind);

// Values

// Base entity of all IR graph nodes.
//
// An IR module is a graph of values, connected by their "uses" relations. The graph of IR
// values is built by the `MaterialIRModuleBuilder` as the result of crawling through and
// analyizing the MaterialExpression graph contained in the translated Material. During
// this processing, IR values are emitted, and linked together. After the graph is
// constructed, it is itself analyzed: the builder will call `FMaterialIRValueAnalyzer::Analyze()`
// in each active* (i.e. truly used) value in the graph, making sure a value is analyzed only
// after its dependencies have been analyzed.
// A few notes:
// - FValues are automatically zero-initialized.
// - FValues are intended to be simple and inert data records. They cannot have non-trivia
//   ctor, dtor or copy operators.
// - The ModuleBuilder relies on this property to efficiently hashing values so that it
//   will reuse the same value instance instead of creating multiple instances of the
//   same computation (for a more efficient output shader).
// - All values have a MIR type.
// - Pure FValue instances are values that do not have other dependencies (called "uses").
// - If a value has some other value as dependency, it means that it is the result of a
//   calculation on those values. Values that have dependencies are Instructions (they
//   derive from FInstruction).
struct FValue
{
	// Used to discern the concrete C++ type of this value (e.g. Subscript)
	EValueKind Kind : 8;

	// Set of fundamental flags true for this value.
	EValueFlags Flags;

	// The set of properties that are true for this value. Some flags might have been
	// set but some upstream dependency leading to this value and not this value directly.
	// See `EGraphProperties` for more information.
	EGraphProperties GraphProperties;

	// The runtime type this value has.
	const FType* Type;

	// Returns whether this value has been analyzed for specified stage.
	bool IsAnalyzed(EStage State) const;

	// Returns whether specified flags are true for this value.
	bool HasFlags(EValueFlags InFlags) const;

	// Enables the specified flags without affecting others.
	void SetFlags(EValueFlags InFlags);

	// Disables the specified flags without affecting others.
	void ClearFlags(EValueFlags InFlags);

	// Returns whether specified properties are true for this value.
	bool HasSubgraphProperties(EGraphProperties Properties) const;

	// Enables specified properties for this value.
	void UseSubgraphProperties(EGraphProperties Properties);

	// Returns the size in bytes of this value instance.
	uint32 GetSizeInBytes() const;

	// Returns whether this value is of specified kind.
	bool IsA(EValueKind InKind) const;

	// Gets the immutable array of all this value uses.
	// An use is another value referenced by this one (e.g. the operands of a binary expression).
	// Returns the immutable array of uses.
	TConstArrayView<FValue*> GetUses() const;

	// Gets the immutable array of this value uses filtered for a specific stage stage.
	//
	// Returns the immutable array of uses.
	TConstArrayView<FValue*> GetUsesForStage(MIR::EStage Stage) const;

	// Returns whether this value is poison.
	bool IsPoison() const;

	// Returns whether this value exactly equals Other.
	bool Equals(const FValue* Other) const;

	// Returns whether this value is a scalar (its type is Primitive with exactly 1 component).
	bool IsScalar() const;

	// Returns whether this value is a vector (tis type is Primitive with 1-4 rows and exactly 1 column).
	bool IsVector() const;

	// Returns whether this value is a constant boolean with value true.
	bool IsTrue() const;

	// Returns whether this value is a constant boolean with value false.
	bool IsFalse() const;

	// Returns whether this value is boolean and all components are true.
	bool AreAllTrue() const;

	// Returns whether this value is boolean and all components are false.
	bool AreAllFalse() const;

	// Returns whether this value is arithmetic and all components are exactly zero.
	bool AreAllExactlyZero() const;

	// Returns whether this value is arithmetic and all components are approximately zero.
	bool AreAllNearlyZero() const;

	// Returns whether this value is arithmetic and all components are exactly one.
	bool AreAllExactlyOne() const;

	// Returns whether this value is arithmetic and all components are approximately one.
	bool AreAllNearlyOne() const;

	// Tries to cast this value to specified type T and returns the casted pointer, if possible (nullptr otherwise).
	template <typename T>
	T* As() { return IsA(T::TypeKind) ? static_cast<T*>(this) : nullptr; }

	// Tries to cast this value to specified type T and returns the casted pointer, if possible (nullptr otherwise).
	template <typename T>
	const T* As() const { return IsA(T::TypeKind) ? static_cast<const T*>(this) : nullptr; }
};

// Tries to cast a value to a derived type.
// If specified value is not null, it tries to cast this value T and returns it. Otherwise, it returns null.
template <typename T>
T* As(FValue* Value)
{
	return Value && Value->IsA(T::TypeKind) ? static_cast<T*>(Value) : nullptr;
}

// Tries to cast a value to a derived type.
// If specified value is not null, it tries to cast this value T and returns it. Otherwise, it returns null.
template <typename T>
const T* As(const FValue* Value)
{
	return Value && Value->IsA(T::TypeKind) ? static_cast<const T*>(Value) : nullptr;
}

// Casts a value to an instruction.
// If specified value is not null, it tries to cast this value to an instruction and returns it. Otherwise, it returns null.
FInstruction* AsInstruction(FValue* Value);

// Casts a value to an instruction.
// If specified value is not null, it tries to cast this value to an instruction and returns it. Otherwise, it returns null.
const FInstruction* AsInstruction(const FValue* Value);

template <EValueKind TTypeKind>
struct TValue : FValue
{
	static constexpr EValueKind TypeKind = TTypeKind;
};

// A placeholder for an invalid value.
//
// A poison value represents an invalid value. It is produced by the emitter when an
// invalid operation is performed. Poison values can be passed as arguments to other operations,
// but they are "contagious": any instruction emitted with a poison value as an argument
// will itself produce a poison value.
struct FPoison : TValue<VK_Poison>
{
	static FPoison* Get();
};

// The integer type used inside MIR.
using TInteger = int64_t;

// The floating point type used inside MIR.
using TFloat = float;

// A constant value.
//
// A constant represents a translation-time known scalar primitive value. Operations on
// constant values can be folded by the builder, that is they can be evaluated statically
// while the builder constructs the IR graph of an input material.
struct FConstant : TValue<VK_Constant>
{
	union
	{
		bool  		Boolean;
		TInteger	Integer;
		TFloat 		Float;
	};

	// Returns whether this constant is a boolean.
	bool IsBool() const;

	// Returns whether this constant is an integer.
	bool IsInteger() const;

	// Returns whether this constant is a float.
	bool IsFloat() const;

	// Returns the constant value of given type T. The type must be bool, integral or floating point.
	template <typename T>
	T Get() const
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return Boolean;
		}
		else if constexpr (std::is_integral_v<T>)
		{
			return Integer;
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			return Float;
		}
		else
		{
			check(false && "unexpected type T.");
		}
	}
};

// Enumeration of all supported material external inputs.
enum class EExternalInput
{
	None,

	TexCoord0,
	TexCoord1,
	TexCoord2,
	TexCoord3,
	TexCoord4,
	TexCoord5,
	TexCoord6,
	TexCoord7,

	TexCoord0_Ddx,
	TexCoord1_Ddx,
	TexCoord2_Ddx,
	TexCoord3_Ddx,
	TexCoord4_Ddx,
	TexCoord5_Ddx,
	TexCoord6_Ddx,
	TexCoord7_Ddx,

	TexCoord0_Ddy,
	TexCoord1_Ddy,
	TexCoord2_Ddy,
	TexCoord3_Ddy,
	TexCoord4_Ddy,
	TexCoord5_Ddy,
	TexCoord6_Ddy,
	TexCoord7_Ddy,

	ViewMaterialTextureMipBias,
	ViewMaterialTextureDerivativeMultiply,

	Count,
};

// The number of texcoord external input groups (uv, ddx, ddy).
static constexpr int32 TexCoordGroups = 3;

// Maximum number of supported texture coordinates.
static constexpr int32 TexCoordMaxNum = 8;

// Returns the string representation of given given external input.
const TCHAR* LexToString(EExternalInput Input);

// Returns the given external input type.
const FType* GetExternalInputType(EExternalInput Id);

// Converts the given texture coordinate index to its corresponding external input mapping.
MIR::EExternalInput TexCoordIndexToExternalInput(int32 TexCoordIndex);

// Returns the index of the specified texture coordinates external input.
int32 ExternalInputToTexCoordIndex(EExternalInput Id);

// Returns whether the given external is a texture coordinate.
bool IsExternalInputTexCoord(EExternalInput Id);

// Returns whether the given external is a texture coordinate ddx.
bool IsExternalInputTexCoordDdx(EExternalInput Id);

// Returns whether the given external is a texture coordinate ddy.
bool IsExternalInputTexCoordDdy(EExternalInput Id);

// Returns whether the given external is a texture coordinate, a ddx or ddy.
bool IsExternalInputTexCoordOrPartialDerivative(EExternalInput Id);

// It represents an external input (like texture coordinates) value.
struct FExternalInput : TValue<VK_ExternalInput>
{
	EExternalInput Id;
};

// It represents a texture object value.
struct FTextureObject : TValue<VK_TextureObject>
{
	// The texture object.
	UTexture* Texture;

	// The sampler type associated to this texture object.
	EMaterialSamplerType SamplerType;

	// Index of this parameter in the uniform expression set.
	// Note: This field is automatically set by the builder.
	uint32 Analysis_UniformParameterIndex;
};

// It represents a material uniform parameter.
struct FUniformParameter: TValue<VK_UniformParameter>
{
	// Index of the parameter as registered in the module.
	uint32 ParameterIdInModule;

	// Eventual sampler type to use to sample this parameter's texture (if it is one)
	EMaterialSamplerType SamplerType;

	// Index of this parameter in the uniform expression set
	// Note: This field is automatically set by the builder.
	uint32 Analysis_UniformParameterIndex;
};

/*------------------------------------ Instructions ------------------------------------*/

// A block of instructions.
//
// A block is a sequence of instructions in order of execution. Blocks are organized in a
// tree-like structure, used to model blocks nested inside other blocks.
struct FBlock
{
	// This block's parent block, if any. If this is null, this is a *root block.
	FBlock* Parent;

	// The linked-list head of the instructions contained in this block.
	// Links are contained in the FInstruction::Next field.
	FInstruction* Instructions;

	// Depth of this block in the tree structure (root blocks have level zero).
	int32 Level;

	// Finds and returns the common block between this and Other, if any. It returns null
	// if no common block was found.
	// Note: This has O(n) complexity, where n is the maximum depth of the tree structure.
	FBlock* FindCommonParentWith(MIR::FBlock* Other);
};

// Base struct of an instruction value.
//
// An instruction is a value in a well defined order of evaluation.
// Instructions have a parent block and are organized in a linked list: the Next field
// indicates the instruction that will execute immediately after this one.
// Since the material shader has multiple stages, the same instruction can belong to two
// different graphs of execution, which explains why all fields in this structure have
// a different possible value per stage.
//
// Note: All fields in this struct are not expected to be set by the user when emitting
//      an instruction, and are instead automatically populated by the builder.
struct FInstruction : FValue
{
	// The next instruction executing after this one in all stages of execution.
	FInstruction* Next[NumStages];

	// This instruction parent block in all stages of execution.
	FBlock* Block[NumStages];

	// How many users (i.e., dependencies) this instruction has in each stage of execution.
	uint32 NumUsers[NumStages];

	// The number of users processed during instruction graph linking in each stage of execution.
	// Note: This information combined with NumUsers is used by the builder to push instructions
	//      in the appropriate block automatically.
	uint32 NumProcessedUsers[NumStages];

	// Returns the block in which the dependency with specified index should execute.
	FBlock* GetDesiredBlockForUse(EStage Stage, int32 UseIndex);
};

template <EValueKind TTypeKind, uint32 TNumStaticUses>
struct TInstruction : FInstruction
{
	// The kind of this instruction.
	static constexpr EValueKind TypeKind = TTypeKind;

	// The number of values this instruction uses statically. Some instructions have a
	// dynamic number of uses, in which case NumStaticUses is 0.
	static constexpr uint32 NumStaticUses = TNumStaticUses;
};

// An aggregate of other values.
//
// A dimensional is a fixed array of other values. This value is used to model vectors and matrices.
struct FDimensional : TInstruction<VK_Dimensional, 0>
{
	// Returns the constant array of component values.
	TConstArrayView<FValue*> GetComponents() const;

	// Returns the mutable array of component values.
	TArrayView<FValue*> GetMutableComponents();

	// Returns whether all components are constant (i.e., they're instances of FConstant).
	bool AreComponentsConstant() const;
};

template <int TDimension>
struct TDimensional : FDimensional
{
	FValue* Components[TDimension];
};

// Instruction that sets material attribute (i.e., "BaseColor") to its value.
struct FSetMaterialOutput : TInstruction<VK_SetMaterialOutput, 1>
{
	// The value this material attribute should be set to.
	FValue* Arg;

	// The material attribute to set.
	EMaterialProperty Property;
};

// Inline HLSL instruction.
//
// A value that is the result of an arbitrary snippet of HLSL code.
//
// Note: See BaseMaterialExpressions.ini file.
struct FInlineHLSL : TInstruction<VK_InlineHLSL, 0>
{
	static constexpr int32 MaxNumArguments = 16;

	// Array of argument values.
	FValue* Arguments[MaxNumArguments];

	// Number of arguments from another expression node.
	int32 NumArguments;

	union
	{
		// The declaration this instruction refers to.
		const FMaterialExternalCodeDeclaration* ExternalCodeDeclaration;

		// The arbitrary inlined HLSL code snippet
		const TCHAR* Code;
	};
};

// Operator enumeration.
//
// Note: If you modify this enum, update the implementations of the helper functions below.
enum EOperator
{
	O_Invalid,

	// Unary
	UO_FirstUnaryOperator,

	// Unary operators
	UO_BitwiseNot = UO_FirstUnaryOperator, // ~(x)
	UO_Negate, // Arithmetic negation: negate(5) -> -5
	UO_Not, // Logical negation: not(true) -> false

	// Unary intrinsics
	UO_Abs,
	UO_ACos,
	UO_ACosh,
	UO_ASin,
	UO_ASinh,
	UO_ATan,
	UO_ATanh,
	UO_Ceil,
	UO_Cos,
	UO_Cosh,
	UO_Exponential,
	UO_Exponential2,
	UO_Floor,
	UO_Frac,
	UO_IsFinite,
	UO_IsInf,
	UO_IsNan,
	UO_Length,
	UO_Logarithm,
	UO_Logarithm10,
	UO_Logarithm2,
	UO_Round,
	UO_Saturate,
	UO_Sign,
	UO_Sin,
	UO_Sinh,
	UO_Sqrt,
	UO_Tan,
	UO_Tanh,
	UO_Truncate,

	BO_FirstBinaryOperator,

	// Binary comparisons
	BO_Equals = BO_FirstBinaryOperator,
	BO_GreaterThan,
	BO_GreaterThanOrEquals,
	BO_LessThan,
	BO_LessThanOrEquals,
	BO_NotEquals,

	// Binary logical
	BO_And,
	BO_Or,

	// Binary arithmetic
	BO_Add,
	BO_Subtract,
	BO_Multiply,
	BO_Divide,
	BO_Modulo,
	BO_BitwiseAnd,
	BO_BitwiseOr,
	BO_BitShiftLeft,
	BO_BitShiftRight,
	
	// Binary intrinsics
	BO_Cross,
	BO_Distance,
	BO_Dot,
	BO_Fmod,
	BO_Max,
	BO_Min,
	BO_Pow,
	BO_Step,

	TO_FirstTernaryOperator,

	// Ternary intrinsics
	TO_Clamp = TO_FirstTernaryOperator,
	TO_Lerp,
	TO_Select,
	TO_Smoothstep,

	OperatorCount,
};

// Whether the given operator identifies a comparison operation (e.g., ">=", "==").
bool IsComparisonOperator(EOperator Op);

// Whether the given operator identifies a unary operator.
bool IsUnaryOperator(EOperator Op);

// Whether the given operator identifies a binary operator.
bool IsBinaryOperator(EOperator Op);

// Whether the given operator identifies a ternary operator.
bool IsTernaryOperator(EOperator Op);

// Returns the arity of the operator (the number of arguments it take, 1, 2 or 3).
int GetOperatorArity(EOperator Op);

// Returns the string representation of the given operator.
const TCHAR* LexToString(EOperator Op);

// A mathematical operator instruction.
//
// This instruction identifies a built-in operation on one, two or three argument values.
struct FOperator : TInstruction<VK_Operator, 3>
{
	// The first argument of the operation. This value is never null.
	FValue* AArg;

	// The second argument of the operation. This value is null for unary operators.
	FValue* BArg;

	// The third argument of the operation. This value is null for unary and binary operators.
	FValue* CArg;

	// It identifies which supported operation to carry.
	EOperator Op;
};

// A branch instruction.
//
// This instruction evaluates to one or another argument based on whether a third boolean
// condition argument is true or false. The builder will automatically place as many
// instructions as possible in the true/false inner blocks whilst respecting dependency
// requirements. This is done in an effort to avoid the unnecessary computation of the
// input value that was not selected by the condition.
struct FBranch : TInstruction<VK_Branch, 3>
{
	// The boolean condition argument used to forward the "true" or "false" argument.
	FValue* ConditionArg;

	// Value this branch evaluates to when the condition is true.
	FValue* TrueArg;

	// Value this branch evaluates to when the condition is false.
	FValue* FalseArg;

	// The inner block (in each execution stage) the subgraph evaluating `TrueArg` should be placed in.
	FBlock TrueBlock[NumStages];

	// The inner block (in each execution stage) the subgraph evaluating `FalseArg` should be placed in.
	FBlock FalseBlock[NumStages];
};

// A subscript instruction.
//
// This instruction is used to pull the inner value making a compound one. For example,
// it is used to extract an individual component of a vector value.
struct FSubscript : TInstruction<VK_Subscript, 1>
{
	// The argument to subscript.
	FValue* Arg;

	// The subscript index, i.e. the index of the component to extract.
	int Index;
};

// A type cast instruction.
//
// This instruction casts an argument value to a different type.
struct FCast : TInstruction<VK_Cast, 1>
{
	// The argument value to cast.
	FValue* Arg;
};

// What texture gather mode to use in a texture read instruction (none indicates a sample).
enum class ETextureReadMode
{
	// Gather the four red components in a 2x2 pixel block.
	GatherRed,

	// Gather the four green components in a 2x2 pixel block.
	GatherGreen,

	// Gather the four blue components in a 2x2 pixel block.
	GatherBlue,

	// Gather the four alpha components in a 2x2 pixel block.
	GatherAlpha,

	// Texture gather with automatically calculated mip level
	MipAuto,

	// Texture gather with user specified mip level
	MipLevel,

	// Texture gather with automatically calculated mip level plus user specified bias
	MipBias,

	// Texture gather using automatically caluclated mip level based on user provided partial derivatives
	Derivatives,
};

// Returns the string representation of given mode.
const TCHAR* LexToString(ETextureReadMode Mode);

// This instruction performs texture read operaation (sample or gather).
struct FTextureRead : TInstruction<VK_TextureRead, 5>
{
	// The texture object to sample.
	FValue* TextureObject;

	// The texture coordinate at which to sample.
	FValue* TexCoord;

	// Optional. The mip index to sample, if any provided.
	FValue* MipValue;

	// Optional. The analytical partial derivative of the coordinates along the X axis.
	FValue* TexCoordDdx;

	// Optional. The analytical partial derivative of the coordinates along the Y axis.
	FValue* TexCoordDdy;

	// The mip value mode to use for sampling.
	ETextureReadMode Mode : 8;

	// The sampler source mode to use for sampling.
	ESamplerSourceMode SamplerSourceMode : 8;

	// The sampler type to use for sampling.
	EMaterialSamplerType SamplerType : 8;
};

// Utility value for selecting a different value based on the execution stage.
struct FStageSwitch : TInstruction<VK_StageSwitch, 1>
{
	// The argument for to be bypassed in each stage. e
	FValue* Args[NumStages];

	// Use the specified value argument in the pixel stage, and another specified
	// argument for other stages.
	void SetArgs(FValue* PixelStageArg, FValue* OtherStagesArg);
};

// Specifies the axis for computing screen-space derivatives.
//
// This enum is used to indicate whether a partial derivative should be taken
// along the X or Y screen-space direction, corresponding to HLSL's ddx and ddy
// functions.
enum class EDerivativeAxis
{
	X, // Corresponding to d/dx
	Y, // Corresponding to d/dy
};

// Instruction that maps to hardware ddx()/ddy().
//
// Note: This is only available in stages that support hardware derivatives (e.g. pixel shader)
struct FHardwarePartialDerivative : TInstruction<VK_HardwarePartialDerivative, 1>
{
	// The value argument of ddx()/ddy()
	FValue* Arg;

	// The direction of partial derivative
	EDerivativeAxis Axis;
};

} // namespace MIR
#endif // WITH_EDITOR
