// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRToHLSLTranslator.h"
#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIRTypes.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialExternalCodeRegistry.h"
#include "MaterialIRInternal.h"

#include "ShaderCore.h"
#include "MaterialShared.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h"
#include "RenderUtils.h"
#include "Engine/Texture.h"

#include <inttypes.h>

#if WITH_EDITOR

enum ENoOp { NoOp };
enum ENewLine { NewLine };
enum EEndOfStatement { EndOfStatement };
enum EOpenBrace { OpenBrace };
enum ECloseBrace { CloseBrace };
enum EIndentation { Indentation };
enum EBeginArgs { BeginArgs };
enum EEndArgs { EndArgs };
enum EListSeparator { ListSeparator };

#define TAB "    "

struct FHLSLPrinter
{
	FString Buffer;
	bool bFirstListItem = false;
	int32 Tabs = 0;

	FHLSLPrinter& operator<<(const TCHAR* Text)
	{
		Buffer.Append(Text);
		return *this;
	}
	
	FHLSLPrinter& operator<<(const FString& Text)
	{
		Buffer.Append(Text);
		return *this;
	}

	FHLSLPrinter& operator<<(const FStringView& Text)
	{
		Buffer.Append(Text);
		return *this;
	}

	FHLSLPrinter& operator<<(int32 Value)
	{
		Buffer.Appendf(TEXT("%d"), Value);
		return *this;
	}

    FHLSLPrinter& operator<<(ENoOp)
	{
		return *this;
	}

    FHLSLPrinter& operator<<(ENewLine)
    {
		Buffer.AppendChar('\n');
		operator<<(Indentation);
        return *this;
    }

	FHLSLPrinter& operator<<(EIndentation)
	{
		for (int32 i = 0; i < Tabs; ++i)
		{
			Buffer.AppendChar('\t');
		}
		return *this;
	}

	FHLSLPrinter& operator<<(EEndOfStatement)
	{
		Buffer.AppendChar(';');
        *this << NewLine;
        return *this;
	}

    FHLSLPrinter& operator<<(EOpenBrace)
    {
        Buffer.Append("{");
        ++Tabs;
        *this << NewLine;
        return *this;
    }

    FHLSLPrinter& operator<<(ECloseBrace)
    {
        --Tabs;
        Buffer.LeftChopInline(1); // undo tab
        Buffer.AppendChar('}');
        return *this;
    }
	
    FHLSLPrinter& operator<<(EBeginArgs)
    {
        Buffer.AppendChar('(');
		BeginList();
        return *this;
    }

    FHLSLPrinter& operator<<(EEndArgs)
    {
        Buffer.AppendChar(')');
        return *this;
    }

	FHLSLPrinter& operator<<(EListSeparator)
    {
		PrintListSeparator();
        return *this;
    }

	void BeginList()
	{
		bFirstListItem = true;
	}

	void PrintListSeparator()
	{
		if (!bFirstListItem)
		{
			Buffer.Append(TEXT(", "));
		}
		bFirstListItem = false;
	}
};

static const TCHAR* GetHLSLTypeString(EMaterialValueType Type)
{
	switch (Type)
	{
		case MCT_Float1: return TEXT("MaterialFloat");
		case MCT_Float2: return TEXT("MaterialFloat2");
		case MCT_Float3: return TEXT("MaterialFloat3");
		case MCT_Float4: return TEXT("MaterialFloat4");
		case MCT_Float: return TEXT("MaterialFloat");
		case MCT_Texture2D: return TEXT("texture2D");
		case MCT_TextureCube: return TEXT("textureCube");
		case MCT_Texture2DArray: return TEXT("texture2DArray");
		case MCT_VolumeTexture: return TEXT("volumeTexture");
		case MCT_StaticBool: return TEXT("static bool");
		case MCT_Bool:  return TEXT("bool");
		case MCT_MaterialAttributes: return TEXT("FMaterialAttributes");
		case MCT_TextureExternal: return TEXT("TextureExternal");
		case MCT_TextureVirtual: return TEXT("TextureVirtual");
		case MCT_VTPageTableResult: return TEXT("VTPageTableResult");
		case MCT_ShadingModel: return TEXT("uint");
		case MCT_UInt: return TEXT("uint");
		case MCT_UInt1: return TEXT("uint");
		case MCT_UInt2: return TEXT("uint2");
		case MCT_UInt3: return TEXT("uint3");
		case MCT_UInt4: return TEXT("uint4");
		case MCT_Substrate: return TEXT("FSubstrateData");
		case MCT_TextureCollection: return TEXT("FResourceCollection");
		default: return TEXT("unknown");
	};
}

static const TCHAR* GetShadingModelParameterName(EMaterialShadingModel InModel)
{
	switch (InModel)
	{
		case MSM_Unlit: return TEXT("MATERIAL_SHADINGMODEL_UNLIT");
		case MSM_DefaultLit: return TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT");
		case MSM_Subsurface: return TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE");
		case MSM_PreintegratedSkin: return TEXT("MATERIAL_SHADINGMODEL_PREINTEGRATED_SKIN");
		case MSM_ClearCoat: return TEXT("MATERIAL_SHADINGMODEL_CLEAR_COAT");
		case MSM_SubsurfaceProfile: return TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE_PROFILE");
		case MSM_TwoSidedFoliage: return TEXT("MATERIAL_SHADINGMODEL_TWOSIDED_FOLIAGE");
		case MSM_Hair: return TEXT("MATERIAL_SHADINGMODEL_HAIR");
		case MSM_Cloth: return TEXT("MATERIAL_SHADINGMODEL_CLOTH");
		case MSM_Eye: return TEXT("MATERIAL_SHADINGMODEL_EYE");
		case MSM_SingleLayerWater: return TEXT("MATERIAL_SHADINGMODEL_SINGLELAYERWATER");
		case MSM_ThinTranslucent: return TEXT("MATERIAL_SHADINGMODEL_THIN_TRANSLUCENT");
		default: UE_MIR_UNREACHABLE();
	}
}

static bool IsFoldable(const MIR::FInstruction* Instr, MIR::EStage Stage)
{
	if (auto Branch = Instr->As<MIR::FBranch>())
	{
		return !Branch->TrueBlock[Stage].Instructions && !Branch->FalseBlock[Stage].Instructions;
	}

	return true;
}

/// Returns whether the operator in HLSL is infix between its arguments, e.g. "4 + 4"
static bool IsOperatorInfix(MIR::EOperator Op)
{
	switch (Op)
	{
		case MIR::BO_GreaterThan:
		case MIR::BO_GreaterThanOrEquals:
		case MIR::BO_LessThan:
		case MIR::BO_LessThanOrEquals:
		case MIR::BO_Equals:
		case MIR::BO_NotEquals:
		case MIR::BO_Add:
		case MIR::BO_Subtract:
		case MIR::BO_Multiply:
		case MIR::BO_Divide:
		case MIR::BO_Max:
		case MIR::BO_Min:
			return true;
		default: return false;
	}
}

struct FTranslator : FMaterialIRToHLSLTranslation
{
	int32 NumLocals{};
	TMap<const MIR::FInstruction*, FString> LocalIdentifier;
	MIR::EStage CurrentStage;
	FHLSLPrinter Printer;
	FString PixelAttributesHLSL;
	FString WorldPositionOffsetHLSL;
	FString EvaluateNormalMaterialAttributeHLSL[MIR::NumStages];
	FString EvaluateOtherMaterialAttributesHLSL[MIR::NumStages];

	void GeneratePixelAttributesHLSL()
	{
		for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
		{
			EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
			if (!MIR::Internal::IsMaterialPropertyEnabled(Property) || !MIR::MaterialOutputEvaluatesInStage(Property, MIR::Stage_Pixel))
			{
				continue;
			}
		
			check(FMaterialAttributeDefinitionMap::GetShaderFrequency(Property) == SF_Pixel);
			
			// Special case MP_SubsurfaceColor as the actual property is a combination of the color and the profile but we don't want to expose the profile
			FString PropertyName = (Property == MP_SubsurfaceColor) ? "Subsurface" : FMaterialAttributeDefinitionMap::GetAttributeName(Property);
			EMaterialValueType Type = (Property == MP_SubsurfaceColor) ? MCT_Float4 : FMaterialAttributeDefinitionMap::GetValueType(Property);
			check(PropertyName.Len() > 0);

			PixelAttributesHLSL.Appendf(TEXT(TAB "%s %s;\n"), GetHLSLTypeString(Type), *PropertyName);
		}
	}
	
	void GenerateVertexStageHLSL()
	{
		BeginStage(MIR::Stage_Vertex);

		LowerBlock(Module->GetRootBlock(MIR::Stage_Vertex));

		WorldPositionOffsetHLSL = MoveTemp(Printer.Buffer);
	}

	void GenerateOtherStageHLSL(MIR::EStage Stage)
	{
		BeginStage(Stage);

		LowerBlock(Module->GetRootBlock(CurrentStage));

		Printer << TEXT("PixelMaterialInputs.FrontMaterial = GetInitialisedSubstrateData()") << EndOfStatement;
		Printer << TEXT("PixelMaterialInputs.Subsurface = 0") << EndOfStatement;

		EvaluateOtherMaterialAttributesHLSL[CurrentStage] = MoveTemp(Printer.Buffer);
	}
	
	void BeginStage(MIR::EStage Stage)
	{
		Printer = {};
		Printer.Tabs = 1;
		Printer << Indentation;
		CurrentStage = Stage;
	}
	
	ENoOp LowerBlock(const MIR::FBlock& Block)
	{
		int32 OldNumLocals = NumLocals;
		for (MIR::FInstruction* Instr = Block.Instructions; Instr; Instr = Instr->Next[CurrentStage])
		{
            if (Instr->NumUsers[CurrentStage] == 1 && IsFoldable(Instr, CurrentStage))
			{
                continue;
            }
            
			if (Instr->NumUsers[CurrentStage] >= 1)
			{
                FString LocalStr = FString::Printf(TEXT("_%d"), NumLocals);
                ++NumLocals;

                Printer << LowerType(Instr->Type) << TEXT(" ") << LocalStr;

                LocalIdentifier.Add(Instr, MoveTemp(LocalStr));
                if (IsFoldable(Instr, CurrentStage))
				{
                    Printer << TEXT(" = ";)
                }
            }

			LowerInstruction(Instr);

			if (Printer.Buffer.EndsWith(TEXT("}")))
			{
				Printer << NewLine;
			}
			else
			{
				Printer << EndOfStatement;
			}

			// Store the code needed to evaluate the normal in a separate chunk than the other material attributes
			// since this needs to be emitted before the others in the material template.
			const MIR::FSetMaterialOutput* SetMaterialOutput = MIR::As<MIR::FSetMaterialOutput>(Instr);
			if (SetMaterialOutput && SetMaterialOutput->Property == MP_Normal) 
			{
				EvaluateNormalMaterialAttributeHLSL[CurrentStage] = MoveTemp(Printer.Buffer);
			}
        }

        NumLocals = OldNumLocals;

		return NoOp;
	}
	
	ENoOp LowerValue(MIR::FValue* InValue)
	{
		if (MIR::FInstruction* Instr = MIR::AsInstruction(InValue))
		{
			if (Instr->NumUsers[CurrentStage] <= 1 && IsFoldable(Instr, CurrentStage))
			{
				Printer << LowerInstruction(Instr);
			}
			else
			{
				Printer << LocalIdentifier[Instr];
			}

			return NoOp;
		}

		switch (InValue->Kind)
		{
			case MIR::VK_Constant: LowerConstant(static_cast<const MIR::FConstant*>(InValue)); break;
			case MIR::VK_ExternalInput: LowerExternalInput(static_cast<const MIR::FExternalInput*>(InValue)); break;
			case MIR::VK_TextureObject: LowerTextureObject(static_cast<const MIR::FTextureObject*>(InValue)); break;
			case MIR::VK_UniformParameter: LowerUniformParameter(static_cast<const MIR::FUniformParameter*>(InValue)); break;
			default: UE_MIR_UNREACHABLE();
		}

		return NoOp;
	}

	ENoOp LowerInstruction(MIR::FInstruction* Instr)
	{
		switch (Instr->Kind)
		{
			case MIR::VK_Dimensional: LowerDimensional(static_cast<MIR::FDimensional*>(Instr)); break;
			case MIR::VK_SetMaterialOutput: LowerSetMaterialOutput(static_cast<MIR::FSetMaterialOutput*>(Instr)); break;
			case MIR::VK_Operator: LowerOperator(static_cast<MIR::FOperator*>(Instr)); break;
			case MIR::VK_Branch: LowerBranch(static_cast<MIR::FBranch*>(Instr)); break;
			case MIR::VK_Subscript: LowerSubscript(static_cast<MIR::FSubscript*>(Instr)); break;
			case MIR::VK_TextureRead: LowerTextureRead(static_cast<MIR::FTextureRead*>(Instr)); break;
			case MIR::VK_InlineHLSL: LowerInlineHLSL(static_cast<const MIR::FInlineHLSL*>(Instr)); break;
			case MIR::VK_StageSwitch: LowerStageSwitch(static_cast<const MIR::FStageSwitch*>(Instr)); break;
			case MIR::VK_HardwarePartialDerivative: LowerHardwarePartialDerivative(static_cast<const MIR::FHardwarePartialDerivative*>(Instr)); break;
			default: UE_MIR_UNREACHABLE();
		}
	
		return NoOp;
	}

	void LowerConstant(const MIR::FConstant* Constant)
	{
		const MIR::FPrimitiveType* checkNoEntry = Constant->Type->AsPrimitive();
		check(checkNoEntry && checkNoEntry->IsScalar());

		switch (checkNoEntry->ScalarKind)
		{
			case MIR::ScalarKind_Bool:
				Printer.Buffer.Append(Constant->Boolean ? TEXT("true") : TEXT("false"));
				break;
				
			case MIR::ScalarKind_Int:
				Printer.Buffer.Appendf(TEXT("%") PRId64, Constant->Integer);
				break;
			
			case MIR::ScalarKind_Float:
				if (FGenericPlatformMath::IsNaN(Constant->Float))
				{
					Printer.Buffer.Append(TEXT("(0.0f / 0.0f)"));
				}
				else if (!FGenericPlatformMath::IsFinite(Constant->Float))
				{
					Printer.Buffer.Append(TEXT("INFINITE_FLOAT"));
				}
				else
				{
					Printer.Buffer.Appendf(TEXT("%.8f"), Constant->Float);
				}
				break;
		}
	}

	void LowerExternalInput(const MIR::FExternalInput* ExternalInput)
	{
		int32 ExternalInputIndex = (int32)ExternalInput->Id;
		if (MIR::IsExternalInputTexCoord(ExternalInput->Id))
		{
			int32 Index = ExternalInputIndex - (int32)MIR::EExternalInput::TexCoord0;
			Printer.Buffer.Appendf(TEXT("Parameters.TexCoords[%d]"), Index);
		}
		else if (MIR::IsExternalInputTexCoordDdx(ExternalInput->Id))
		{
			int32 Index = ExternalInputIndex - (int32)MIR::EExternalInput::TexCoord0_Ddx;
			Printer.Buffer.Appendf(TEXT("Parameters.TexCoords_DDX[%d]"), Index);
		}
		else if (MIR::IsExternalInputTexCoordDdy(ExternalInput->Id))
		{
			int32 Index = ExternalInputIndex - (int32)MIR::EExternalInput::TexCoord0_Ddy;
			Printer.Buffer.Appendf(TEXT("Parameters.TexCoords_DDY[%d]"), Index);
		}
		else
		{
			const TCHAR* Code{};
			switch (ExternalInput->Id)
			{
				case MIR::EExternalInput::ViewMaterialTextureMipBias: Code = TEXT("View.MaterialTextureMipBias"); break;
				case MIR::EExternalInput::ViewMaterialTextureDerivativeMultiply: Code = TEXT("View.MaterialTextureDerivativeMultiply"); break;
				default: UE_MIR_UNREACHABLE();
			}
			Printer << Code;
		}
	}

	void LowerInlineHLSLWithArgumentsInternal(const FStringView& Code, const TConstArrayView<MIR::FValue*>& InArguments)
	{
		// Substitute argument tokens with instruction arguments
		int32 ArgumentTokenStart = 0, ArgumentTokenEnd = 0;

		// Prints the pending substring of the code and moves the range forward.
		auto FlushCodeSubstring = [this, &ArgumentTokenEnd, &Code](int32 EndIndex) -> void
			{
				if (EndIndex > ArgumentTokenEnd)
				{
					Printer << Code.Mid(ArgumentTokenEnd, EndIndex - ArgumentTokenEnd);
					ArgumentTokenEnd = EndIndex;
				}
			};

		auto SubstituteNextArgument = [this, &ArgumentTokenStart, &ArgumentTokenEnd, &Code, &InArguments]() -> void
			{
				// Scan digits for argument index
				int32 ArgumentIndexValue = 0;
				int32 NumDigits = 0;

				while (ArgumentTokenStart + NumDigits < Code.Len())
				{
					TCHAR CodeCharacter = Code[ArgumentTokenStart + NumDigits];
					if (!FChar::IsDigit(CodeCharacter))
					{
						break;
					}
					ArgumentIndexValue *= 10;
					ArgumentIndexValue += (CodeCharacter - TEXT('0'));
					++NumDigits;
				}

				checkf(NumDigits > 0, TEXT("Failed to scan integer in inline-HLSL after token '$':\n\"%.*s\""), Code.Len(), Code.GetData());

				checkf(
					ArgumentIndexValue < InArguments.Num(), TEXT("Failed to substitute token $%d in inline-HLSL with given number of arguments (%d):\n\"%.*s\""),
					ArgumentIndexValue, InArguments.Num(), Code.Len(), Code.GetData()
				);

				LowerValue(InArguments[ArgumentIndexValue]);

				ArgumentTokenEnd = ArgumentTokenStart + NumDigits;
			};

		auto MatchCharacter = [&Code](int32& Position, TCHAR InCharacter) -> bool
			{
				if (Position < Code.Len() && Code[Position] == InCharacter)
				{
					++Position;
					return true;
				}
				return false;
			};

		// Find all argument token characters '$'. For example "MyFunction($1, $0.xxxw)" can be subsituted with "MyFunction(MySecondArgument, MyFirstArgument.xxxw)"
		while ((ArgumentTokenStart = Code.Find(TEXT("$"), ArgumentTokenEnd)) != INDEX_NONE)
		{
			FlushCodeSubstring(ArgumentTokenStart);

			++ArgumentTokenStart;
			if (MatchCharacter(ArgumentTokenStart, TEXT('{')))
			{
				SubstituteNextArgument();
				const bool bMatchedArgumentToken = MatchCharacter(ArgumentTokenEnd, TEXT('}'));
				checkf(bMatchedArgumentToken, TEXT("Failed to match argument token in inline-HLSL with syntax '${N}':\n\"%.*s\""), Code.Len(), Code.GetData());
			}
			else
			{
				SubstituteNextArgument();
			}
		}

		FlushCodeSubstring(Code.Len());
	}

	void LowerInlineHLSL(const MIR::FInlineHLSL* ExternalCode)
	{
		auto PrintHLSLCode = [this, ExternalCode](const FStringView& Code) -> void
			{
				if (ExternalCode->NumArguments > 0)
				{
					check(ExternalCode->Arguments);
					LowerInlineHLSLWithArgumentsInternal(Code, TConstArrayView<MIR::FValue*>(ExternalCode->Arguments, ExternalCode->NumArguments));
				}
				else
				{
					Printer << Code;
				}
			};

		if (ExternalCode->HasFlags(MIR::EValueFlags::HasDynamicHLSLCode))
		{
			check(ExternalCode->Code != nullptr);

			// Substitute placeholder tokens now unless disabled for custom nodes
			if (ExternalCode->HasFlags(MIR::EValueFlags::SubstituteTagsInInlineHLSL))
			{
				FString Code = ExternalCode->Code;

				// @todo-laura.hermanns - Must be replaced as soon as this state is available in MIR
				constexpr bool bCompilingPreviousFrame = false;
				Code.ReplaceInline(TEXT("<PREV>"), bCompilingPreviousFrame ? TEXT("Prev") : TEXT(""));
	
				PrintHLSLCode(Code);
			}
			else
			{
				PrintHLSLCode(ExternalCode->Code);
			}
		}
		else
		{
			check(ExternalCode->ExternalCodeDeclaration != nullptr);
			if (ExternalCode->HasFlags(MIR::EValueFlags::DerivativeDDX))
			{
				PrintHLSLCode(ExternalCode->ExternalCodeDeclaration->DefinitionDDX);
			}
			else if (ExternalCode->HasFlags(MIR::EValueFlags::DerivativeDDY))
			{
				PrintHLSLCode(ExternalCode->ExternalCodeDeclaration->DefinitionDDY);
			}
			else
			{
				PrintHLSLCode(ExternalCode->ExternalCodeDeclaration->Definition);
			}
		}
	}

	void LowerTextureObject(const MIR::FTextureObject* TextureObject)
	{
		LowerTextureReference(TextureObject->Texture->GetMaterialType(), TextureObject->Analysis_UniformParameterIndex);
	}

	void LowerUniformParameter(const MIR::FUniformParameter* UniformParameter)
	{
		if (UniformParameter->Type->IsObjectOfKind(MIR::ObjectKind_Texture2D))
		{
		 	EMaterialValueType TextureType = Module->GetParameterMetadata(UniformParameter->ParameterIdInModule).Value.Texture->GetMaterialType();
			LowerTextureReference(TextureType, UniformParameter->Analysis_UniformParameterIndex);
		}
		else
		{
			LowerPrimitiveUniformParameter(UniformParameter);
		}
	}

	void LowerPrimitiveUniformParameter(const MIR::FUniformParameter* UniformParameter)
	{
		const MIR::FPrimitiveType* PrimitiveType = UniformParameter->Type->AsPrimitive();
		check(PrimitiveType);
		check(PrimitiveType->IsScalar() || PrimitiveType->IsVector()); // no matrices yet

		if (PrimitiveType->ScalarKind == MIR::EScalarKind::ScalarKind_Int)
		{
			Printer << "asint(";
		}

		const FUniformExpressionSet& UniformExpressionSet = Module->GetCompilationOutput().UniformExpressionSet;
		
		// Get the global float4 component index (e.g. if this is 13, it refer to PreshaderBuffer[3].y)
		uint32 GlobalComponentOffset = UniformExpressionSet.GetNumericParameterEvaluationOffset(UniformParameter->Analysis_UniformParameterIndex);
		
		// Index of the float4 slot
		uint32 BufferSlotIndex = GlobalComponentOffset / 4;

		// Starting component of the float4 slot
		uint32 BufferSlotOffset = GlobalComponentOffset % 4;

		Printer << TEXT("Material.PreshaderBuffer[") << BufferSlotIndex << TEXT("]");

		if (PrimitiveType->GetNumComponents() < 4)
		{
			Printer << TEXT(".");

			static const TCHAR* Components = TEXT("xyzw");
			for (int32 i = 0; i < PrimitiveType->GetNumComponents(); ++i)
			{
				check(BufferSlotOffset + i < sizeof(Components));
				Printer.Buffer.AppendChar(Components[BufferSlotOffset + i]);
			}
		}

		if (PrimitiveType->ScalarKind != MIR::EScalarKind::ScalarKind_Float)
		{
			Printer << ")"; // close the "asint(" bracket
		}
	}

	void LowerDimensional(const MIR::FDimensional* Dimensional) 
	{
		TArrayView<MIR::FValue* const> Components = Dimensional->GetComponents();
		const MIR::FPrimitiveType* ArithmeticType = Dimensional->Type->AsPrimitive();
		check(ArithmeticType && !ArithmeticType->IsScalar());
		
		// In order to generate smaller and tidier HLSL, first check whether all components
		// of this dimensional are actually the same. If so, we can simply emit the 
		// component and cast it to the type.
		bool bSameComponents = true;
		for (int32 i = 1; bSameComponents && i < Components.Num(); ++i)
		{
			bSameComponents &= (Components[i] == Components[0]);
		}
		
		if (bSameComponents)
		{
			Printer << TEXT("(") << LowerType(ArithmeticType) << TEXT(")") << LowerValue(Components[0]);
		}
		else
		{
			Printer << LowerType(ArithmeticType) << BeginArgs;
			for (MIR::FValue* Component : Components)
			{
				Printer << ListSeparator << LowerValue(Component);
			}
			Printer << EndArgs;
		}

	}

	void LowerSetMaterialOutput(const MIR::FSetMaterialOutput* Output)
	{
		// Special case MP_SubsurfaceColor as the actual property is a combination of the color and the profile but we don't want to expose the profile
		const FString& PropertyName = (Output->Property == MP_SubsurfaceColor) ? "Subsurface" : FMaterialAttributeDefinitionMap::GetAttributeName(Output->Property);

		if (Output->Property == MP_WorldPositionOffset)
		{
			Printer << TEXT("return ");
		}
		else
		{
			Printer << TEXT("PixelMaterialInputs.") << PropertyName << TEXT(" = ");
		}

		LowerValue(Output->Arg);
	}

	void LowerOperator(const MIR::FOperator* Operator)
	{
		if (IsOperatorInfix(Operator->Op))
		{
			const TCHAR* OpString;
			switch (Operator->Op)
			{
				case MIR::BO_Add: OpString = TEXT("+"); break;
				case MIR::BO_Divide: OpString = TEXT("/"); break;
				case MIR::BO_Modulo: OpString = TEXT("%"); break;
				case MIR::BO_Equals: OpString = TEXT("=="); break;
				case MIR::BO_GreaterThan: OpString = TEXT(">"); break;
				case MIR::BO_GreaterThanOrEquals: OpString = TEXT(">="); break;
				case MIR::BO_LessThan: OpString = TEXT("<"); break;
				case MIR::BO_LessThanOrEquals: OpString = TEXT("<="); break;
				case MIR::BO_Multiply: OpString = TEXT("*"); break;
				case MIR::BO_NotEquals: OpString = TEXT("!="); break;
				case MIR::BO_Subtract: OpString = TEXT("-"); break;
				default: UE_MIR_UNREACHABLE();
			}

			Printer << "(" << LowerValue(Operator->AArg) << " " << OpString << " " << LowerValue(Operator->BArg) << ")";
		}
		else
		{
			const TCHAR* OpString;
			switch (Operator->Op)
			{
				case MIR::UO_Abs: OpString = TEXT("abs"); break;
				case MIR::UO_ACos: OpString = TEXT("acos"); break;
				case MIR::UO_ACosh: OpString = TEXT("acosh"); break;
				case MIR::UO_ASin: OpString = TEXT("asin"); break;
				case MIR::UO_ASinh: OpString = TEXT("asinh"); break;
				case MIR::UO_ATan: OpString = TEXT("atan"); break;
				case MIR::UO_ATanh: OpString = TEXT("atanh"); break;
				case MIR::UO_Ceil: OpString = TEXT("ceil"); break;
				case MIR::UO_Cos: OpString = TEXT("cos"); break;
				case MIR::UO_Cosh: OpString = TEXT("cosh"); break;
				case MIR::UO_Exponential: OpString = TEXT("exp"); break;
				case MIR::UO_Exponential2: OpString = TEXT("exp2"); break;
				case MIR::UO_Floor: OpString = TEXT("floor"); break;
				case MIR::UO_Frac: OpString = TEXT("frac"); break;
				case MIR::UO_IsFinite: OpString = TEXT("isfinite"); break;
				case MIR::UO_IsInf: OpString = TEXT("isinf"); break;
				case MIR::UO_IsNan: OpString = TEXT("isnan"); break;
				case MIR::UO_Length: OpString = TEXT("length"); break;
				case MIR::UO_Logarithm: OpString = TEXT("log"); break;
				case MIR::UO_Logarithm10: OpString = TEXT("log10"); break;
				case MIR::UO_Logarithm2: OpString = TEXT("log2"); break;
				case MIR::UO_Round: OpString = TEXT("round"); break;
				case MIR::UO_Saturate: OpString = TEXT("saturate"); break;
				case MIR::UO_Sign: OpString = TEXT("sign"); break;
				case MIR::UO_Sin: OpString = TEXT("sin"); break;
				case MIR::UO_Sinh: OpString = TEXT("sinh"); break;
				case MIR::UO_Sqrt: OpString = TEXT("sqrt"); break;
				case MIR::UO_Tan: OpString = TEXT("tan"); break;
				case MIR::UO_Tanh: OpString = TEXT("tanh"); break;
				case MIR::UO_Truncate: OpString = TEXT("trunc"); break;

				case MIR::BO_And: OpString = TEXT("and"); break;
				case MIR::BO_Cross: OpString = TEXT("cross"); break;
				case MIR::BO_Distance: OpString = TEXT("distance"); break;
				case MIR::BO_Dot: OpString = TEXT("dot"); break;
				case MIR::BO_Fmod: OpString = TEXT("fmod"); break;
				case MIR::BO_Max: OpString = TEXT("max"); break;
				case MIR::BO_Min: OpString = TEXT("min"); break;
				case MIR::BO_Or: OpString = TEXT("or"); break;
				case MIR::BO_Pow: OpString = TEXT("pow"); break;
				case MIR::BO_Step: OpString = TEXT("step"); break;

				case MIR::TO_Clamp: OpString = TEXT("clamp"); break;
				case MIR::TO_Lerp: OpString = TEXT("lerp"); break;
				case MIR::TO_Select: OpString = TEXT("select"); break;
				case MIR::TO_Smoothstep: OpString = TEXT("smoothstep"); break;
			
				default: UE_MIR_UNREACHABLE();
			}

			// Unary
			Printer << OpString << "(" << LowerValue(Operator->AArg);

			// Binary
			if (Operator->BArg)
			{
				check(MIR::IsBinaryOperator(Operator->Op) || MIR::IsTernaryOperator(Operator->Op));
				Printer << ", " << LowerValue(Operator->BArg);
			}

			// Ternary
			if (Operator->CArg)
			{
				check(MIR::IsTernaryOperator(Operator->Op));
				Printer << ", " << LowerValue(Operator->CArg);
			}

			Printer << ")";
		}
	}

	void LowerBranch(const MIR::FBranch* Branch)
	{
		if (IsFoldable(Branch, CurrentStage))
		{
			Printer << LowerValue(Branch->ConditionArg)
				<< TEXT(" ? ") << LowerValue(Branch->TrueArg)
				<< TEXT(" : ") << LowerValue(Branch->FalseArg);
		}
		else
		{
			Printer << EndOfStatement;
			Printer << TEXT("if (") << LowerValue(Branch->ConditionArg) << TEXT(")") << NewLine << OpenBrace;
			Printer << LowerBlock(Branch->TrueBlock[CurrentStage]);
			Printer << LocalIdentifier[Branch] << " = " << LowerValue(Branch->TrueArg) << EndOfStatement;
			Printer << CloseBrace << NewLine;
			Printer << TEXT("else") << NewLine << OpenBrace;
			Printer << LowerBlock(Branch->FalseBlock[CurrentStage]);
			Printer << LocalIdentifier[Branch] << " = " << LowerValue(Branch->FalseArg) << EndOfStatement;
			Printer << CloseBrace;
		}
	}
			
	void LowerSubscript(const MIR::FSubscript* Subscript)
	{
		LowerValue(Subscript->Arg);

		if (const MIR::FPrimitiveType* ArgArithmeticType = Subscript->Arg->Type->AsVector())
		{
			const TCHAR* ComponentsStr[] = { TEXT(".x"), TEXT(".y"), TEXT(".z"), TEXT(".w") };
			check(Subscript->Index <= ArgArithmeticType->GetNumComponents());

			Printer << ComponentsStr[Subscript->Index];
		}
	}

	void LowerTextureRead(const MIR::FTextureRead* TextureRead)
	{
		bool bSamplerNeedsBrackets = LowerSamplerType(TextureRead->SamplerType);
		if (bSamplerNeedsBrackets)
		{
			Printer << TEXT("(");
		}
		
		switch (TextureRead->TextureObject->Type->AsObject()->ObjectKind)
		{
			case MIR::ObjectKind_Texture2D: Printer << TEXT("Texture2D"); break;
			default: UE_MIR_UNREACHABLE();
		}

		switch (TextureRead->Mode)
		{
			case MIR::ETextureReadMode::GatherRed: Printer << TEXT("GatherRed"); break;
			case MIR::ETextureReadMode::GatherGreen: Printer << TEXT("GatherGreen"); break;
			case MIR::ETextureReadMode::GatherBlue: Printer << TEXT("GatherBlue"); break;
			case MIR::ETextureReadMode::GatherAlpha: Printer << TEXT("GatherAlpha"); break;
			case MIR::ETextureReadMode::MipAuto: Printer << TEXT("Sample"); break;
			case MIR::ETextureReadMode::MipLevel: Printer << TEXT("SampleLevel"); break;
			case MIR::ETextureReadMode::MipBias: Printer << TEXT("SampleBias"); break;
			case MIR::ETextureReadMode::Derivatives: Printer << TEXT("SampleGrad"); break;
			default: UE_MIR_UNREACHABLE();
		}

		Printer << BeginArgs
				<< ListSeparator << LowerValue(TextureRead->TextureObject)
				<< ListSeparator << LowerTextureSamplerReference(TextureRead->TextureObject, TextureRead->SamplerSourceMode)
				<< ListSeparator << LowerValue(TextureRead->TexCoord);

		switch (TextureRead->Mode)
		{
			case MIR::ETextureReadMode::MipLevel: Printer << ListSeparator << LowerValue(TextureRead->MipValue); break;
			case MIR::ETextureReadMode::MipBias: Printer << ListSeparator << LowerValue(TextureRead->MipValue); break;
			case MIR::ETextureReadMode::Derivatives: Printer << ListSeparator << LowerValue(TextureRead->TexCoordDdx) << ListSeparator << LowerValue(TextureRead->TexCoordDdy); break;
			default: break;
		}

		Printer << EndArgs;

		if (bSamplerNeedsBrackets)
		{
			Printer << TEXT(")");
		}
	}

	bool LowerSamplerType(EMaterialSamplerType SamplerType)
	{
		switch (SamplerType)
		{
			case SAMPLERTYPE_External:
				Printer << TEXT("ProcessMaterialExternalTextureLookup");
				break;

			case SAMPLERTYPE_Color:
				Printer << TEXT("ProcessMaterialColorTextureLookup");
				break;
			case SAMPLERTYPE_VirtualColor:
				// has a mobile specific workaround
				Printer << TEXT("ProcessMaterialVirtualColorTextureLookup");
				break;

			case SAMPLERTYPE_LinearColor:
			case SAMPLERTYPE_VirtualLinearColor:
				Printer << TEXT("ProcessMaterialLinearColorTextureLookup");
				break;

			case SAMPLERTYPE_Alpha:
			case SAMPLERTYPE_VirtualAlpha:
			case SAMPLERTYPE_DistanceFieldFont:
				Printer << TEXT("ProcessMaterialAlphaTextureLookup");
				break;

			case SAMPLERTYPE_Grayscale:
			case SAMPLERTYPE_VirtualGrayscale:
				Printer << TEXT("ProcessMaterialGreyscaleTextureLookup");
				break;

			case SAMPLERTYPE_LinearGrayscale:
			case SAMPLERTYPE_VirtualLinearGrayscale:
				Printer <<TEXT("ProcessMaterialLinearGreyscaleTextureLookup");
				break;

			case SAMPLERTYPE_Normal:
			case SAMPLERTYPE_VirtualNormal:
				// Normal maps need to be unpacked in the pixel shader.
				Printer << TEXT("UnpackNormalMap");
				break;

			case SAMPLERTYPE_Masks:
			case SAMPLERTYPE_VirtualMasks:
			case SAMPLERTYPE_Data:
				return false;

			default:
				UE_MIR_UNREACHABLE();
		}

		return true;
	}

	ENoOp LowerTextureSamplerReference(MIR::FValue* TextureValue, ESamplerSourceMode SamplerSource)
	{
		if (SamplerSource != SSM_FromTextureAsset)
		{
			Printer << "GetMaterialSharedSampler(";
		}
		
		Printer << LowerValue(TextureValue) << TEXT("Sampler");
		
		if (SamplerSource == SSM_Wrap_WorldGroupSettings)
		{
			Printer << ", View.MaterialTextureBilinearWrapedSampler)";
		}
		else if (SamplerSource == SSM_Clamp_WorldGroupSettings)
		{
			Printer << ", View.MaterialTextureBilinearClampedSampler)";
		}
		else
		{
			// SSM_TerrainWeightmapGroupSettings unsupported yet
			check(SamplerSource == SSM_FromTextureAsset);
		}

		return NoOp;
	}

	ENoOp LowerTextureReference(EMaterialValueType TextureType, int32 TextureParameterIndex)
	{
		Printer << TEXT("Material.");

		switch (TextureType)
		{
			case MCT_Texture2D: Printer << TEXT("Texture2D_"); break;
			default: UE_MIR_UNREACHABLE();
		}

		Printer << TextureParameterIndex;

		return NoOp;
	}
	
	void LowerStageSwitch(const MIR::FStageSwitch* StageSwitch)
	{
		LowerValue(StageSwitch->Args[CurrentStage]);
	}

	void LowerHardwarePartialDerivative(const MIR::FHardwarePartialDerivative* StageSwitch)
	{
		Printer << (StageSwitch->Axis == MIR::EDerivativeAxis::X ? "ddx(" : "ddy(") << LowerValue(StageSwitch->Arg) << ")";
	}

	/* Finalization */

	void GenerateTemplateStringParameters(TMap<FString, FString>& Params)
	{
		const FMaterialIRModule::FStatistics ModuleStatistics = Module->GetStatistics();

		auto SetParamInt = [&] (const TCHAR* InParamName, int32 InValue)
		{
			Params.Add(InParamName, FString::Printf(TEXT("%d"), InValue));
		};
		
		auto SetParamReturnFloat = [&] (const TCHAR* InParamName, float InValue)
		{
			Params.Add(InParamName, FString::Printf(TEXT(TAB "return %.5f"), InValue));
		};

		Params.Add(TEXT("pixel_material_inputs"), MoveTemp(PixelAttributesHLSL));

		// "Normal" is treated in a special way because the rest of the attributes may lead back to reading it.
		// Therefore, in the way MaterialTemplate.ush is structured, it needs to be evaluated before other attributes.
		Params.Add(TEXT("calc_pixel_material_inputs_analytic_derivatives_normal"), EvaluateNormalMaterialAttributeHLSL[MIR::Stage_Compute]);
		Params.Add(TEXT("calc_pixel_material_inputs_normal"), EvaluateNormalMaterialAttributeHLSL[MIR::Stage_Pixel]);

		// Then the other other attributes.
		Params.Add(TEXT("calc_pixel_material_inputs_analytic_derivatives_other_inputs"), EvaluateOtherMaterialAttributesHLSL[MIR::Stage_Compute]);
		Params.Add(TEXT("calc_pixel_material_inputs_other_inputs"), EvaluateOtherMaterialAttributesHLSL[MIR::Stage_Pixel]);
		
		// MaterialAttributes
		TArray<FGuid> OrderedVisibleAttributes = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
		
		FString MaterialDeclarations;
		MaterialDeclarations.Appendf(TEXT("struct FMaterialAttributes\n{\n"));
		for (const FGuid& AttributeID : OrderedVisibleAttributes)
		{
			const FString& PropertyName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
			const EMaterialValueType PropertyType = FMaterialAttributeDefinitionMap::GetValueType(AttributeID);
			MaterialDeclarations.Appendf(TEXT(TAB "%s %s;\n"), GetHLSLTypeString(PropertyType), *PropertyName);
		}
		MaterialDeclarations.Appendf(TEXT("};"));
		Params.Add(TEXT("material_declarations"), MoveTemp(MaterialDeclarations));
		
		SetParamInt(TEXT("num_material_texcoords_vertex"), ModuleStatistics.NumVertexTexCoords);
		SetParamInt(TEXT("num_material_texcoords"), ModuleStatistics.NumPixelTexCoords);
		SetParamInt(TEXT("num_custom_vertex_interpolators"), 0);
		SetParamInt(TEXT("num_tex_coord_interpolators"), ModuleStatistics.NumPixelTexCoords);

		FString GetMaterialCustomizedUVS;
		for (int32 CustomUVIndex = 0; CustomUVIndex < ModuleStatistics.NumPixelTexCoords; CustomUVIndex++)
		{
			const FString AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName((EMaterialProperty)(MP_CustomizedUVs0 + CustomUVIndex));
			GetMaterialCustomizedUVS.Appendf(TEXT(TAB "OutTexCoords[%u] = Parameters.MaterialAttributes.%s;\n"), CustomUVIndex, *AttributeName);
		}
		Params.Add(TEXT("get_material_customized_u_vs"), MoveTemp(GetMaterialCustomizedUVS));

		SetParamReturnFloat(TEXT("get_material_emissive_for_cs"), 0.f);
		SetParamReturnFloat(TEXT("get_material_translucency_directional_lighting_intensity"), Material->GetTranslucencyDirectionalLightingIntensity());
		SetParamReturnFloat(TEXT("get_material_translucent_shadow_density_scale"), Material->GetTranslucentShadowDensityScale());
		SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_density_scale"), Material->GetTranslucentSelfShadowDensityScale());
		SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_second_density_scale"), Material->GetTranslucentSelfShadowSecondDensityScale());
		SetParamReturnFloat(TEXT("get_material_translucent_self_shadow_second_opacity"), Material->GetTranslucentSelfShadowSecondOpacity());
		SetParamReturnFloat(TEXT("get_material_translucent_backscattering_exponent"), Material->GetTranslucentBackscatteringExponent());

		FLinearColor Extinction = Material->GetTranslucentMultipleScatteringExtinction();
		Params.Add(TEXT("get_material_translucent_multiple_scattering_extinction"), FString::Printf(TEXT(TAB "return MaterialFloat3(%.5f, %.5f, %.5f)"), Extinction.R, Extinction.G, Extinction.B));

		SetParamReturnFloat(TEXT("get_material_opacity_mask_clip_value"), Material->GetOpacityMaskClipValue());

		Params.Add(TEXT("get_material_world_position_offset_raw"), WorldPositionOffsetHLSL);
		Params.Add(TEXT("get_material_previous_world_position_offset_raw"), WorldPositionOffsetHLSL);
	
		FString EvaluateMaterialDeclaration;
		EvaluateMaterialDeclaration.Append(TEXT("void EvaluateVertexMaterialAttributes(in out FMaterialVertexParameters Parameters)\n{\n"));
		for (int32 CustomUVIndex = 0; CustomUVIndex < ModuleStatistics.NumPixelTexCoords; CustomUVIndex++)
		{
			EvaluateMaterialDeclaration.Appendf(TEXT(TAB "Parameters.MaterialAttributes.CustomizedUV%d = Parameters.TexCoords[%d].xy;\n"), CustomUVIndex, CustomUVIndex);
		}
		EvaluateMaterialDeclaration.Append(TEXT("\n}\n"));
		Params.Add(TEXT("evaluate_material_attributes"), MoveTemp(EvaluateMaterialDeclaration));
	}
	
	ENoOp LowerType(const MIR::FType* Type)
	{
		if (auto ArithmeticType = Type->AsPrimitive())
		{
			switch (ArithmeticType->ScalarKind)
			{
				case MIR::ScalarKind_Bool:	Printer << TEXT("bool"); break;
				case MIR::ScalarKind_Int: 	Printer << TEXT("int32"); break;
				case MIR::ScalarKind_Float:	Printer << TEXT("MaterialFloat"); break;
			}

			if (ArithmeticType->NumRows > 1)
			{
				Printer << ArithmeticType->NumRows;
			}
			
			if (ArithmeticType->NumColumns > 1)
			{
				Printer << TEXT("x") << ArithmeticType->NumColumns;
			}
		}
		else
		{
			UE_MIR_UNREACHABLE();
		}

		return NoOp;
	}
	
	void GetShaderCompilerEnvironment(FShaderCompilerEnvironment& OutEnvironment)
	{
		const FMaterialCompilationOutput& CompilationOutput = Module->GetCompilationOutput();
		EShaderPlatform ShaderPlatform = Module->GetShaderPlatform();

		OutEnvironment.TargetPlatform = TargetPlatform;
		OutEnvironment.SetDefine(TEXT("ENABLE_NEW_HLSL_GENERATOR"), 1);
		OutEnvironment.SetDefine(TEXT("MATERIAL_ATMOSPHERIC_FOG"), false);
		OutEnvironment.SetDefine(TEXT("MATERIAL_SKY_ATMOSPHERE"), false);
		OutEnvironment.SetDefine(TEXT("INTERPOLATE_VERTEX_COLOR"), false);
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_COLOR"), false);
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_LOCAL_TO_WORLD"), false);
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_WORLD_TO_LOCAL"), false);
		OutEnvironment.SetDefine(TEXT("NEEDS_PER_INSTANCE_RANDOM_PS"), false);
		OutEnvironment.SetDefine(TEXT("USES_EYE_ADAPTATION"), false);
		OutEnvironment.SetDefine(TEXT("USES_PER_INSTANCE_CUSTOM_DATA"), false);
		OutEnvironment.SetDefine(TEXT("USES_PER_INSTANCE_FADE_AMOUNT"), false);
		OutEnvironment.SetDefine(TEXT("USES_TRANSFORM_VECTOR"), false);
		OutEnvironment.SetDefine(TEXT("WANT_PIXEL_DEPTH_OFFSET"), CompilationOutput.bUsesPixelDepthOffset);
		OutEnvironment.SetDefineAndCompileArgument(TEXT("USES_WORLD_POSITION_OFFSET"), (bool)CompilationOutput.bUsesWorldPositionOffset);
		OutEnvironment.SetDefineAndCompileArgument(TEXT("USES_DISPLACEMENT"), false);
		OutEnvironment.SetDefine(TEXT("USES_EMISSIVE_COLOR"), false);
		OutEnvironment.SetDefine(TEXT("USES_DISTORTION"), Material->IsDistorted());
		OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_FOGGING"), Material->ShouldApplyFogging());
		OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_CLOUD_FOGGING"), Material->ShouldApplyCloudFogging());
		OutEnvironment.SetDefine(TEXT("MATERIAL_IS_SKY"), Material->IsSky());
		OutEnvironment.SetDefine(TEXT("MATERIAL_COMPUTE_FOG_PER_PIXEL"), Material->ComputeFogPerPixel());
		OutEnvironment.SetDefine(TEXT("MATERIAL_FULLY_ROUGH"), false);
		OutEnvironment.SetDefine(TEXT("MATERIAL_USES_ANISOTROPY"), false);
		OutEnvironment.SetDefine(TEXT("MATERIAL_NEURAL_POST_PROCESS"), (CompilationOutput.bUsedWithNeuralNetworks || Material->IsUsedWithNeuralNetworks()) && Material->IsPostProcessMaterial());
		OutEnvironment.SetDefine(TEXT("NUM_VIRTUALTEXTURE_SAMPLES"), 0);
		OutEnvironment.SetDefine(TEXT("MATERIAL_VIRTUALTEXTURE_FEEDBACK"), false);
		OutEnvironment.SetDefine(TEXT("IS_MATERIAL_SHADER"), true);

		// Set all defines that are defined by the module.
		// Any conditional exemption via material properties, such as 'Material->IsUsedWithInstancedStaticMeshes()', are handled during the material IR analysis.
		for (const FName& EnvironmentDefine : Module->GetEnvironmentDefines())
		{
			OutEnvironment.SetDefine(EnvironmentDefine, true);
		}

		FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
		ensure(ShadingModels.IsValid());

		int32 NumActiveShadingModels = 0;
		if (ShadingModels.IsLit())
		{
			// This is to have platforms use the simple single layer water shading similar to mobile: no dynamic lights, only sun and sky, no distortion, no colored transmittance on background, no custom depth read.
			const bool bSingleLayerWaterUsesSimpleShading = FDataDrivenShaderPlatformInfo::GetWaterUsesSimpleForwardShading(ShaderPlatform) && IsForwardShadingEnabled(ShaderPlatform);

			for (int32 i = 0; i < MSM_NUM; ++i)
			{
				EMaterialShadingModel Model = (EMaterialShadingModel)i;
				if (Model == MSM_Strata || !ShadingModels.HasShadingModel(Model))
				{
					continue;
				}

				if (Model == MSM_SingleLayerWater && !FDataDrivenShaderPlatformInfo::GetRequiresDisableForwardLocalLights(ShaderPlatform))
				{
					continue;
				}

				if (Model == MSM_SingleLayerWater && bSingleLayerWaterUsesSimpleShading)
				{
					// Value must match SINGLE_LAYER_WATER_SHADING_QUALITY_MOBILE_WITH_DEPTH_TEXTURE in SingleLayerWaterCommon.ush!
					OutEnvironment.SetDefine(TEXT("SINGLE_LAYER_WATER_SHADING_QUALITY"), true);
				}

				OutEnvironment.SetDefine(GetShadingModelParameterName(Model), true);
				NumActiveShadingModels += 1;
			}
		}
		else
		{
			// Unlit shading model can only exist by itself
			OutEnvironment.SetDefine(GetShadingModelParameterName(MSM_Unlit), true);
			NumActiveShadingModels += 1;
		}

		if (NumActiveShadingModels == 1)
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SINGLE_SHADINGMODEL"), true);
		}
		else if (!ensure(NumActiveShadingModels > 0))
		{
			UE_LOG(LogMaterial, Warning, TEXT("Unknown material shading model(s). Setting to MSM_DefaultLit"));
			OutEnvironment.SetDefine(GetShadingModelParameterName(MSM_DefaultLit), true);
		}

		static IConsoleVariable* CVarLWCIsEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MaterialEditor.LWCEnabled"));
		OutEnvironment.SetDefine(TEXT("MATERIAL_LWC_ENABLED"), CVarLWCIsEnabled->GetInt());
		OutEnvironment.SetDefine(TEXT("WSVECTOR_IS_TILEOFFSET"), true);
		OutEnvironment.SetDefine(TEXT("WSVECTOR_IS_DOUBLEFLOAT"), false);

		if (Material->GetMaterialDomain() == MD_Volume)
		{
			TArray<const UMaterialExpressionVolumetricAdvancedMaterialOutput*> VolumetricAdvancedExpressions;
			Material->GetMaterialInterface()->GetMaterial()->GetAllExpressionsOfType(VolumetricAdvancedExpressions);
			if (VolumetricAdvancedExpressions.Num() > 0)
			{
				if (VolumetricAdvancedExpressions.Num() > 1)
				{
					UE_LOG(LogMaterial, Fatal, TEXT("Only a single UMaterialExpressionVolumetricAdvancedMaterialOutput node is supported."));
				}

				const UMaterialExpressionVolumetricAdvancedMaterialOutput* VolumetricAdvancedNode = VolumetricAdvancedExpressions[0];
				const TCHAR* Param = VolumetricAdvancedNode->GetEvaluatePhaseOncePerSample() ? TEXT("MATERIAL_VOLUMETRIC_ADVANCED_PHASE_PERSAMPLE") : TEXT("MATERIAL_VOLUMETRIC_ADVANCED_PHASE_PERPIXEL");
				OutEnvironment.SetDefine(Param, true);

				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED"), true);
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_GRAYSCALE_MATERIAL"), VolumetricAdvancedNode->bGrayScaleMaterial);
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_RAYMARCH_VOLUME_SHADOW"), VolumetricAdvancedNode->bRayMarchVolumeShadow);
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_CLAMP_MULTISCATTERING_CONTRIBUTION"), VolumetricAdvancedNode->bClampMultiScatteringContribution);
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_MULTISCATTERING_OCTAVE_COUNT"), VolumetricAdvancedNode->GetMultiScatteringApproximationOctaveCount());
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_CONSERVATIVE_DENSITY"), VolumetricAdvancedNode->ConservativeDensity.IsConnected());
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_OVERRIDE_AMBIENT_OCCLUSION"), Material->HasAmbientOcclusionConnected());
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_GROUND_CONTRIBUTION"), VolumetricAdvancedNode->bGroundContribution);
			}
		}

		OutEnvironment.SetDefine(TEXT("MATERIAL_IS_SUBSTRATE"), false);
		OutEnvironment.SetDefine(TEXT("DUAL_SOURCE_COLOR_BLENDING_ENABLED"), false);
		OutEnvironment.SetDefine(TEXT("TEXTURE_SAMPLE_DEBUG"), false);
	}
};

void FMaterialIRToHLSLTranslation::Run(TMap<FString, FString>& OutParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutParameters.Empty();

	FTranslator Translator{ *this };
	Translator.GeneratePixelAttributesHLSL();
	Translator.GenerateVertexStageHLSL();
	Translator.GenerateOtherStageHLSL(MIR::Stage_Pixel);
	Translator.GenerateOtherStageHLSL(MIR::Stage_Compute);
	Translator.GenerateTemplateStringParameters(OutParameters);
	Translator.GetShaderCompilerEnvironment(OutEnvironment);
}

#undef TAB
#endif // #if WITH_EDITOR
