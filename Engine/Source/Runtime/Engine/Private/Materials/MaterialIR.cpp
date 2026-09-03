// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"
#include "Engine/Texture.h"

#if WITH_EDITOR

namespace MIR
{

const TCHAR* LexToString(EStage Stage)
{
	switch (Stage)
	{
		case Stage_Vertex:	return TEXT("Vertex");
		case Stage_Pixel: 	return TEXT("Pixel");
		case Stage_Compute: return TEXT("Compute");
		case NumStages: UE_MIR_UNREACHABLE();
	}
	return nullptr;
}

bool MaterialOutputEvaluatesInStage(EMaterialProperty Property, EStage Stage)
{
	if (Property == MP_WorldPositionOffset)
	{
		return Stage == Stage_Vertex;
	}
	else
	{
		return Stage != Stage_Vertex;
	}
}

const TCHAR* LexToString(EValueKind Kind)
{
	switch (Kind)
	{
		case VK_Poison: return TEXT("Poison");
		case VK_Constant: return TEXT("Constant");
		case VK_ExternalInput: return TEXT("ExternalInput");
		case VK_TextureObject: return TEXT("TextureObject");
		case VK_UniformParameter: return TEXT("UniformParameter");
		case VK_Dimensional: return TEXT("Dimensional");
		case VK_SetMaterialOutput: return TEXT("SetMaterialOutput");
		case VK_Operator: return TEXT("Operator");
		case VK_Branch: return TEXT("Branch");
		case VK_Subscript: return TEXT("Subscript");
		case VK_Cast: return TEXT("Cast");
		case VK_TextureRead: return TEXT("TextureRead");
		case VK_InlineHLSL: return TEXT("InlineHLSL");
		case VK_StageSwitch: return TEXT("StageSwitch");
		case VK_HardwarePartialDerivative: return TEXT("HardwarePartialDerivative");
		/* invalid entries */
		case VK_InstructionBegin:
		case VK_InstructionEnd:
			UE_MIR_UNREACHABLE();
	}
	return nullptr;
}

bool FValue::IsAnalyzed(EStage Stage) const
{
	return (Flags & EValueFlags(1 << Stage)) != EValueFlags::None;
}

bool FValue::HasFlags(EValueFlags InFlags) const
{
	return (Flags & InFlags) == InFlags;
}

void FValue::SetFlags(EValueFlags InFlags)
{
	Flags |= InFlags;
}

void FValue::ClearFlags(EValueFlags InFlags)
{
	Flags &= ~InFlags;
}

bool FValue::HasSubgraphProperties(EGraphProperties Properties) const
{
	return (GraphProperties & Properties) == Properties;
}

void FValue::UseSubgraphProperties(EGraphProperties Properties)
{
	GraphProperties |= Properties;
}

uint32 FValue::GetSizeInBytes() const
{
	switch (Kind)
	{
		case VK_Poison: return sizeof(FPoison);
		case VK_Constant: return sizeof(FConstant);
		case VK_ExternalInput: return sizeof(FExternalInput);
		case VK_TextureObject: return sizeof(FTextureObject);
		case VK_UniformParameter: return sizeof(FUniformParameter);
		case VK_Dimensional: return sizeof(FDimensional) + sizeof(FValue*) * static_cast<const FDimensional*>(this)->GetComponents().Num();
		case VK_SetMaterialOutput: return sizeof(FSetMaterialOutput);
		case VK_Operator: return sizeof(FOperator);
		case VK_Branch: return sizeof(FBranch);
		case VK_Subscript: return sizeof(FSubscript);
		case VK_Cast: return sizeof(FCast);
		case VK_TextureRead: return sizeof(FTextureRead);
		case VK_InlineHLSL: return sizeof(FInlineHLSL);
		case VK_StageSwitch: return sizeof(FStageSwitch);
		case VK_HardwarePartialDerivative: return sizeof(FHardwarePartialDerivative);
		/* invalid entries */
		case VK_InstructionBegin:
		case VK_InstructionEnd:
			UE_MIR_UNREACHABLE();
	}
	return 0;
}

bool FValue::IsA(EValueKind InKind) const
{
	return Kind == InKind;
}

bool FValue::IsPoison() const
{
	return Kind == VK_Poison;
}

bool FValue::Equals(const FValue* Other) const
{
	if (this == Other)
	{
		return true;
	}

	// Get the size of this value in bytes. It should match that of Other, since the value kinds are the same.
	uint32 SizeInBytes = GetSizeInBytes();
	if (SizeInBytes != Other->GetSizeInBytes())
	{
		return false;
	}

	// Values are PODs by design, therefore simply comparing bytes is sufficient.
	return FMemory::Memcmp(this, Other, SizeInBytes) == 0;
}

TConstArrayView<FValue*> FValue::GetUses() const
{
	// Values have no uses by definition.
	if (Kind < VK_InstructionBegin)
	{
		return {};
	}

	switch (Kind)
	{
		case VK_Dimensional:
		{
			auto This = static_cast<const FDimensional*>(this);
			return This->GetComponents();
		}

		case VK_SetMaterialOutput:
		{
			auto This = static_cast<const FSetMaterialOutput*>(this);
			return { &This->Arg, FSetMaterialOutput::NumStaticUses };
		}

		case VK_Operator:
		{
			auto This = static_cast<const FOperator*>(this);
			return { &This->AArg, FOperator::NumStaticUses };
		}

		case VK_Branch:
		{
			auto This = static_cast<const FBranch*>(this);
			return { &This->ConditionArg, FBranch::NumStaticUses };
		}

		case VK_Subscript:
		{
			auto This = static_cast<const FSubscript*>(this);
			return { &This->Arg, FSubscript::NumStaticUses };
		}

		case VK_Cast:
		{
			auto This = static_cast<const FCast*>(this);
			return { &This->Arg, FCast::NumStaticUses };
		}
			
		case VK_TextureRead:
		{
			auto This = static_cast<const FTextureRead*>(this);
			return { &This->TextureObject, FTextureRead::NumStaticUses };
		}

		case VK_InlineHLSL:
		{
			auto This = static_cast<const FInlineHLSL*>(this);
			return { This->Arguments, This->NumArguments };
		}

		case VK_StageSwitch:
		{
			auto This = static_cast<const FStageSwitch*>(this);
			return { This->Args, FStageSwitch::NumStaticUses * NumStages };
		}

		case VK_HardwarePartialDerivative:
		{
			auto This = static_cast<const FHardwarePartialDerivative*>(this);
			return { &This->Arg, FHardwarePartialDerivative::NumStaticUses };
		}

		default: UE_MIR_UNREACHABLE();
	}
}

TConstArrayView<FValue*> FValue::GetUsesForStage(MIR::EStage Stage) const
{
	if (const FStageSwitch* This = As<FStageSwitch>())
	{
		return { &This->Args[int32(Stage)], FStageSwitch::NumStaticUses };
	}
	return GetUses();
}

bool FValue::IsScalar() const
{
	return Type->AsScalar() != nullptr;
}

bool FValue::IsVector() const
{
	return Type->AsVector() != nullptr;
}

bool FValue::IsTrue() const
{
	const MIR::FConstant* Constant = As<MIR::FConstant>();
	return Constant && Constant->IsBool() && Constant->Boolean == true;
}

bool FValue::IsFalse() const
{
	const MIR::FConstant* Constant = As<MIR::FConstant>();
	return Constant && Constant->IsBool() && Constant->Boolean == false;
}

bool FValue::AreAllTrue() const
{
	if (const MIR::FDimensional* Dimensional = As<MIR::FDimensional>())
	{
		for (const MIR::FValue* Component : Dimensional->GetComponents())
		{
			if (!Component->IsTrue())
			{
				return false;
			}
		}
	}
	else
	{
		return IsTrue();
	}
	return false;
}

bool FValue::AreAllFalse() const
{
	if (const MIR::FDimensional* Dimensional = As<MIR::FDimensional>())
	{
		for (const MIR::FValue* Component : Dimensional->GetComponents())
		{
			if (!Component->IsFalse())
			{
				return false;
			}
		}
	}
	else
	{
		return IsFalse();
	}
	return false;
}

bool FValue::AreAllExactlyZero() const
{
	if (const MIR::FDimensional* Dimensional = As<MIR::FDimensional>())
	{
		for (const MIR::FValue* Component : Dimensional->GetComponents())
		{
			if (!Component->AreAllExactlyZero())
			{
				return false;
			}
		}
	}
	else if (const MIR::FConstant* Constant = As<MIR::FConstant>())
	{
		return (Constant->IsInteger() && Constant->Integer == 0)
			|| (Constant->IsFloat() && Constant->Float == 0.0f);
	}
	return false;
}

bool FValue::AreAllNearlyZero() const
{
	if (const MIR::FDimensional* Dimensional = As<MIR::FDimensional>())
	{
		for (const MIR::FValue* Component : Dimensional->GetComponents())
		{
			if (!Component->AreAllNearlyZero())
			{
				return false;
			}
		}
	}
	else if (const MIR::FConstant* Constant = As<MIR::FConstant>())
	{
		return (Constant->IsInteger() && Constant->Integer == 0)
			|| (Constant->IsFloat() && FMath::IsNearlyZero(Constant->Float));
	}
	return false;
}

bool FValue::AreAllExactlyOne() const
{
	if (const MIR::FDimensional* Dimensional = As<MIR::FDimensional>())
	{
		for (const MIR::FValue* Component : Dimensional->GetComponents())
		{
			if (!Component->AreAllExactlyOne())
			{
				return false;
			}
		}
	}
	else if (const MIR::FConstant* Constant = As<MIR::FConstant>())
	{
		return (Constant->IsInteger() && Constant->Integer == 1)
			|| (Constant->IsFloat() && Constant->Float == 1.0f);
	}
	return false;
}

bool FValue::AreAllNearlyOne() const
{
	if (const MIR::FDimensional* Dimensional = As<MIR::FDimensional>())
	{
		for (const MIR::FValue* Component : Dimensional->GetComponents())
		{
			if (!Component->AreAllNearlyOne())
			{
				return false;
			}
		}
	}
	else if (const MIR::FConstant* Constant = As<MIR::FConstant>())
	{
		return (Constant->IsInteger() && Constant->Integer == 1)
			|| (Constant->IsFloat() && FMath::IsNearlyEqual(Constant->Float, 1.0f));
	}
	return false;
}

FInstruction* AsInstruction(FValue* Value)
{
	return Value && (Value->Kind > VK_InstructionBegin && Value->Kind < VK_InstructionEnd) ? static_cast<FInstruction*>(Value) : nullptr;
}

const FInstruction* AsInstruction(const FValue* Value)
{
	return AsInstruction(const_cast<FValue*>(Value));
}

FPoison* FPoison::Get()
{
	static FPoison Poison = [] {
		FPoison P;
		P.Kind = VK_Poison;
		P.Type = FType::GetPoison();
		return P;
	} ();
	return &Poison;
}

bool FConstant::IsBool() const
{
	return Type->IsBoolScalar();
}

bool FConstant::IsInteger() const
{
	return Type == FPrimitiveType::GetInt();
}

bool FConstant::IsFloat() const
{
	return Type == FPrimitiveType::GetFloat();
}

const TCHAR* LexToString(EExternalInput Input)
{
	switch (Input)
	{
		case EExternalInput::TexCoord0:     return TEXT("TexCoord0");
		case EExternalInput::TexCoord1:     return TEXT("TexCoord1");
		case EExternalInput::TexCoord2:     return TEXT("TexCoord2");
		case EExternalInput::TexCoord3:     return TEXT("TexCoord3");
		case EExternalInput::TexCoord4:     return TEXT("TexCoord4");
		case EExternalInput::TexCoord5:     return TEXT("TexCoord5");
		case EExternalInput::TexCoord6:     return TEXT("TexCoord6");
		case EExternalInput::TexCoord7:     return TEXT("TexCoord7");
		case EExternalInput::TexCoord0_Ddx: return TEXT("TexCoord0_Ddx");
		case EExternalInput::TexCoord1_Ddx: return TEXT("TexCoord1_Ddx");
		case EExternalInput::TexCoord2_Ddx: return TEXT("TexCoord2_Ddx");
		case EExternalInput::TexCoord3_Ddx: return TEXT("TexCoord3_Ddx");
		case EExternalInput::TexCoord4_Ddx: return TEXT("TexCoord4_Ddx");
		case EExternalInput::TexCoord5_Ddx: return TEXT("TexCoord5_Ddx");
		case EExternalInput::TexCoord6_Ddx: return TEXT("TexCoord6_Ddx");
		case EExternalInput::TexCoord7_Ddx: return TEXT("TexCoord7_Ddx");
		case EExternalInput::TexCoord0_Ddy: return TEXT("TexCoord0_Ddy");
		case EExternalInput::TexCoord1_Ddy: return TEXT("TexCoord1_Ddy");
		case EExternalInput::TexCoord2_Ddy: return TEXT("TexCoord2_Ddy");
		case EExternalInput::TexCoord3_Ddy: return TEXT("TexCoord3_Ddy");
		case EExternalInput::TexCoord4_Ddy: return TEXT("TexCoord4_Ddy");
		case EExternalInput::TexCoord5_Ddy: return TEXT("TexCoord5_Ddy");
		case EExternalInput::TexCoord6_Ddy: return TEXT("TexCoord6_Ddy");
		case EExternalInput::TexCoord7_Ddy: return TEXT("TexCoord7_Ddy");
		case EExternalInput::ViewMaterialTextureMipBias: return TEXT("ViewMaterialTextureMipBias");
		case EExternalInput::ViewMaterialTextureDerivativeMultiply: return TEXT("ViewMaterialTextureDerivativeMultiply");
		default: UE_MIR_UNREACHABLE();
	}
}

const FType* GetExternalInputType(EExternalInput Id)
{
	if (Id >= EExternalInput::TexCoord0 && Id <= EExternalInput::TexCoord7_Ddy)
	{
		return FPrimitiveType::GetFloat2();
	}
	
	switch (Id)
	{
		case EExternalInput::ViewMaterialTextureMipBias: return FPrimitiveType::GetFloat();
		case EExternalInput::ViewMaterialTextureDerivativeMultiply: return FPrimitiveType::GetFloat();
		default: UE_MIR_UNREACHABLE();
	}
}

MIR::EExternalInput TexCoordIndexToExternalInput(int32 TexCoordIndex)
{
	check(TexCoordIndex < TexCoordMaxNum);
	return (MIR::EExternalInput)((int)MIR::EExternalInput::TexCoord0 + TexCoordIndex);
}

int32 ExternalInputToTexCoordIndex(EExternalInput Id)
{
	int32 IntId = int32(Id) - 1;
	if (IntId < 0 || IntId > TexCoordMaxNum * TexCoordGroups)
	{
		return UINT32_MAX;
	}
	return IntId % TexCoordMaxNum;
}

bool IsExternalInputTexCoord(EExternalInput Id)
{
	return Id >= EExternalInput::TexCoord0 && Id <= EExternalInput::TexCoord7;
}

bool IsExternalInputTexCoordDdx(EExternalInput Id)
{
	return Id >= EExternalInput::TexCoord0_Ddx && Id <= EExternalInput::TexCoord7_Ddx;
}

bool IsExternalInputTexCoordDdy(EExternalInput Id)
{
	return Id >= EExternalInput::TexCoord0_Ddy && Id <= EExternalInput::TexCoord7_Ddy;
}

bool IsExternalInputTexCoordOrPartialDerivative(EExternalInput Id)
{
	return Id >= EExternalInput::TexCoord0 && Id <= EExternalInput::TexCoord7_Ddy;
}

FBlock* FBlock::FindCommonParentWith(MIR::FBlock* Other)
{
	FBlock* A = this;
	FBlock* B = Other;

	if (A == B)
	{
		return A;
	}

	while (A->Level > B->Level)
	{
		A = A->Parent;
	}

	while (B->Level > A->Level)
	{
		B = B->Parent;
	}

	while (A != B)
	{
		A = A->Parent;
		B = B->Parent;
	}

	return A;
}

TConstArrayView<FValue*> FDimensional::GetComponents() const
{
	const FPrimitiveType* PrimitiveType = Type->AsPrimitive();
	check(PrimitiveType);

	auto Ptr = static_cast<const TDimensional<1>*>(this)->Components;
	return { Ptr, PrimitiveType->NumRows };
}

TArrayView<FValue*> FDimensional::GetMutableComponents()
{
	// const_cast is okay here as it's used only to get the array of components
	TConstArrayView<FValue*> Components = static_cast<const FDimensional*>(this)->GetComponents();
	return { const_cast<FValue**>(Components.GetData()), Components.Num() };
}

bool FDimensional::AreComponentsConstant() const
{
	for (FValue const* Component : GetComponents())
	{
		if (!Component->As<FConstant>())
		{
			return false;
		}
	}
	return true;
}

FBlock* FInstruction::GetDesiredBlockForUse(EStage Stage, int32 UseIndex)
{
	if (auto Branch = As<FBranch>())
	{
		switch (UseIndex)
		{
			case 0: return Block[Stage]; 				// ConditionArg goes into the same block as this instruction's
			case 1: return &Branch->TrueBlock[Stage]; 	// TrueArg
			case 2: return &Branch->FalseBlock[Stage]; 	// FalseArg
			default: UE_MIR_UNREACHABLE();
		}
	}

	// By default, dependencies can go in the same block as this instruction
	return Block[Stage];
}

bool IsComparisonOperator(EOperator Op)
{
	switch (Op)
	{
		case UO_Not:
		case UO_IsFinite:
		case UO_IsInf:
		case UO_IsNan:
		case BO_Equals:
		case BO_GreaterThan:
		case BO_GreaterThanOrEquals:
		case BO_LessThan:
		case BO_LessThanOrEquals:
		case BO_NotEquals:
			return true;
		default:
			return false;
	}
}

bool IsUnaryOperator(EOperator Op)
{
	return Op >= UO_FirstUnaryOperator && Op < BO_FirstBinaryOperator;
}

bool IsBinaryOperator(EOperator Op)
{
	return Op >= BO_FirstBinaryOperator && Op < TO_FirstTernaryOperator;
}

bool IsTernaryOperator(EOperator Op)
{
	return Op >= TO_FirstTernaryOperator;
}

int GetOperatorArity(EOperator Op)
{
	return IsUnaryOperator(Op) ? 1
		: IsBinaryOperator(Op) ? 2
		: 3;
}

const TCHAR* LexToString(EOperator Op)
{
	// Note: sorted alphabetically
	switch (Op)
	{
		/* Unary operators */
		case UO_Abs: return TEXT("Abs");
		case UO_ACos: return TEXT("ACos");
		case UO_ACosh: return TEXT("ACosh");
		case UO_ASin: return TEXT("ASin");
		case UO_ASinh: return TEXT("ASinh");
		case UO_ATan: return TEXT("ATan");
		case UO_ATanh: return TEXT("ATanh");
		case UO_BitwiseNot: return TEXT("BitwiseNot");
		case UO_Ceil: return TEXT("Ceil");
		case UO_Cos: return TEXT("Cos");
		case UO_Cosh: return TEXT("Cosh");
		case UO_Exponential: return TEXT("Exponential");
		case UO_Exponential2: return TEXT("Exponential2");
		case UO_Floor: return TEXT("Floor");
		case UO_Frac: return TEXT("Frac");
		case UO_IsFinite: return TEXT("IsFinite");
		case UO_IsInf: return TEXT("IsInf");
		case UO_IsNan: return TEXT("IsNan");
		case UO_Length: return TEXT("Length");
		case UO_Logarithm: return TEXT("Logarithm");
		case UO_Logarithm10: return TEXT("Logarithm10");
		case UO_Logarithm2: return TEXT("Logarithm2");
		case UO_Negate: return TEXT("Negate");
		case UO_Not: return TEXT("Not");
		case UO_Round: return TEXT("Round");
		case UO_Saturate: return TEXT("Saturate");
		case UO_Sign: return TEXT("Sign");
		case UO_Sin: return TEXT("Sin");
		case UO_Sinh: return TEXT("Sinh");
		case UO_Sqrt: return TEXT("Sqrt");
		case UO_Tan: return TEXT("Tan");
		case UO_Tanh: return TEXT("Tanh");
		case UO_Truncate: return TEXT("Truncate");

		/* Binary operators */
		case BO_Add: return TEXT("Add");
		case BO_And: return TEXT("And");
		case BO_BitShiftLeft: return TEXT("BitShiftLeft");
		case BO_BitShiftRight: return TEXT("BitShiftRight");
		case BO_BitwiseAnd: return TEXT("BitwiseAnd");
		case BO_BitwiseOr: return TEXT("BitwiseOr");
		case BO_Cross: return TEXT("Cross");
		case BO_Distance: return TEXT("Distance");
		case BO_Divide: return TEXT("Divide");
		case BO_Dot: return TEXT("Dot");
		case BO_Equals: return TEXT("Equals");
		case BO_Fmod: return TEXT("Fmod");
		case BO_GreaterThan: return TEXT("GreaterThan");
		case BO_GreaterThanOrEquals: return TEXT("GreaterThanOrEquals");
		case BO_LessThan: return TEXT("LessThan");
		case BO_LessThanOrEquals: return TEXT("LessThanOrEquals");
		case BO_Max: return TEXT("Max");
		case BO_Min: return TEXT("Min");
		case BO_Modulo: return TEXT("Module");
		case BO_Multiply: return TEXT("Multiply");
		case BO_NotEquals: return TEXT("NotEquals");
		case BO_Or: return TEXT("Or");
		case BO_Pow: return TEXT("Pow");
		case BO_Step: return TEXT("Step");
		case BO_Subtract: return TEXT("Subtract");

		/* Ternary operators */
		case TO_Clamp: return TEXT("Clamp");
		case TO_Lerp: return TEXT("Lerp");
		case TO_Select: return TEXT("Select");
		case TO_Smoothstep: return TEXT("Smoothstep");
		
		case O_Invalid: return TEXT("Invalid");
		case OperatorCount: UE_MIR_UNREACHABLE();
	}

	return TEXT("???");
}

const TCHAR* LexToString(ETextureReadMode Mode)
{
	switch (Mode)
	{
		case ETextureReadMode::GatherRed: return TEXT("GatherRed");
		case ETextureReadMode::GatherGreen: return TEXT("GatherGreen");
		case ETextureReadMode::GatherBlue: return TEXT("GatherBlue");
		case ETextureReadMode::GatherAlpha: return TEXT("GatherAlpha");
		case ETextureReadMode::MipAuto: return TEXT("MipAuto");
		case ETextureReadMode::MipLevel: return TEXT("MipLevel");
		case ETextureReadMode::MipBias: return TEXT("MipBias");
		case ETextureReadMode::Derivatives: return TEXT("Derivatives");
		default: UE_MIR_UNREACHABLE();
	}
}

void FStageSwitch::SetArgs(FValue* PixelStageArg, FValue* OtherStagesArg)
{
	for (uint32 i = 0; i < NumStages; ++i)
	{
		Args[i]= (i == Stage_Pixel) ? PixelStageArg : OtherStagesArg;
	}
}

} // namespace MIR

#endif // #if WITH_EDITOR
