// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGCustomHLSLKernel.h"

#include "PCGComputeGraphElement.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Graph/PCGGPUGraphCompilationContext.h"
#include "Compute/Elements/PCGCustomHLSL.h"

#include "ComputeFramework/ComputeSource.h"
#include "Containers/StaticArray.h"

#if WITH_EDITOR
#include "Editor/IPCGEditorModule.h"
#include "Framework/Text/SyntaxTokenizer.h"
#include "Internationalization/Regex.h"
#include "Text/HLSLSyntaxTokenizer.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCustomHLSLKernel)

#define LOCTEXT_NAMESPACE "PCGCustomHLSLKernel"

namespace PCGCustomHLSLKernel
{
	constexpr TCHAR AttributeFunctionGetKeyword[] = { TEXT("Get") };
	constexpr TCHAR AttributeFunctionSetKeyword[] = { TEXT("Set") };
	constexpr TCHAR CopyElementFunctionKeyword[] = { TEXT("CopyElementFrom") };
	constexpr TCHAR InitializeFunctionKeyword[] = { TEXT("Initialize") };
	constexpr TCHAR StoreFunctionKeyword[] = { TEXT("Store") };

#if WITH_EDITOR
	enum class EParseState : uint8
	{
		None,
		LookingForDoubleQuotedString,
		LookingForSingleQuotedString,
		LookingForSingleLineComment,
		LookingForMultiLineComment,
	};

	FString GetDataTypeString(EPCGDataType Type)
	{
		const UEnum* DataTypeEnum = StaticEnum<EPCGDataType>();
		check(DataTypeEnum);

		return DataTypeEnum->GetValueOrBitfieldAsString(static_cast<int64>(Type));
	}

	// Reference implementation in FHLSLSyntaxHighlighterMarshaller::ProcessTokenizedLine()
	void ProcessTokenizedLine(const FString& InSourceString, const ISyntaxTokenizer::FTokenizedLine& InTokenizedLine, EParseState& InOutParseState, TArray<FPCGCustomHLSLParsedSource::FToken>& OutTokens)
	{
		for(const ISyntaxTokenizer::FToken& Token : InTokenizedLine.Tokens)
		{
			FPCGCustomHLSLParsedSource::FToken& Run = OutTokens.Emplace_GetRef();
			Run.Range = Token.Range;
	
			const FString TokenText = InSourceString.Mid(Token.Range.BeginIndex, Token.Range.Len());
			const bool bIsWhitespace = TokenText.TrimEnd().IsEmpty();

			if(!bIsWhitespace)
			{
				bool bHasMatchedSyntax = false;
				if(Token.Type == ISyntaxTokenizer::ETokenType::Syntax)
				{
					if(InOutParseState == EParseState::None && TokenText == TEXT("\""))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::DoubleQuotedString;
						InOutParseState = EParseState::LookingForDoubleQuotedString;
						bHasMatchedSyntax = true;
					}
					else if(InOutParseState == EParseState::LookingForDoubleQuotedString && TokenText == TEXT("\""))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Normal;
						InOutParseState = EParseState::None;
					}
					else if(InOutParseState == EParseState::None && TokenText == TEXT("\'"))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::SingleQuotedString;
						InOutParseState = EParseState::LookingForSingleQuotedString;
						bHasMatchedSyntax = true;
					}
					else if(InOutParseState == EParseState::LookingForSingleQuotedString && TokenText == TEXT("\'"))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Normal;
						InOutParseState = EParseState::None;
					}
					else if(InOutParseState == EParseState::None && TokenText.StartsWith(TEXT("#")))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::PreProcessorKeyword;
						InOutParseState = EParseState::None;
					}
					else if(InOutParseState == EParseState::None && TokenText == TEXT("//"))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Comment;
						InOutParseState = EParseState::LookingForSingleLineComment;
					}
					else if(InOutParseState == EParseState::None && TokenText == TEXT("/*"))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Comment;
						InOutParseState = EParseState::LookingForMultiLineComment;
					}
					else if(InOutParseState == EParseState::LookingForMultiLineComment && TokenText == TEXT("*/"))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Comment;
						InOutParseState = EParseState::None;
					}
					else if(InOutParseState == EParseState::None && TChar<TCHAR>::IsIdentifier(TokenText[0]))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Keyword;
						InOutParseState = EParseState::None;
					}
					else if(InOutParseState == EParseState::None && !TChar<TCHAR>::IsIdentifier(TokenText[0]))
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Operator;
						InOutParseState = EParseState::None;
					}
				}
				
				// It's possible that we fail to match a syntax token if we're in a state where it isn't parsed
				// In this case, we treat it as a literal token
				if(Token.Type == ISyntaxTokenizer::ETokenType::Literal || !bHasMatchedSyntax)
				{
					if(InOutParseState == EParseState::LookingForDoubleQuotedString)
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::DoubleQuotedString;
					}
					else if(InOutParseState == EParseState::LookingForSingleQuotedString)
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::SingleQuotedString;
					}
					else if(InOutParseState == EParseState::LookingForSingleLineComment)
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Comment;
					}
					else if(InOutParseState == EParseState::LookingForMultiLineComment)
					{
						Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Comment;
					}
				}
			}
			else
			{
				Run.Type = FPCGCustomHLSLParsedSource::ETokenType::Whitespace;
			}
		}
	
		if (InOutParseState != EParseState::LookingForMultiLineComment)
		{
			InOutParseState = EParseState::None;
		}
	}

	void ParseTokens(const FString& InSourceString, const TArray<ISyntaxTokenizer::FTokenizedLine>& InTokenizedLines, TArray<FPCGCustomHLSLParsedSource::FToken>& OutTokens)
	{
		EParseState ParseState = EParseState::None;

		for(const ISyntaxTokenizer::FTokenizedLine& TokenizedLine : InTokenizedLines)
		{
			ProcessTokenizedLine(InSourceString, TokenizedLine, ParseState, OutTokens);
		}
	}
#endif
}

#if WITH_EDITOR
void UPCGCustomHLSLKernel::InitializeInternal()
{
	Super::InitializeInternal();

	InitEntryPoint();
	PopulateAttributeKeysFromPinSettings();
	ParseShaderSource();
}
#endif // WITH_EDITOR

bool UPCGCustomHLSLKernel::IsKernelDataValid(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCustomHLSLKernel::IsKernelDataValid);

	if (!Super::IsKernelDataValid(InContext))
	{
		return false;
	}

	if (InContext)
	{
		FText* ErrorTextPtr = nullptr;
#if PCG_KERNEL_LOGGING_ENABLED
		FText ErrorText;
		ErrorTextPtr = &ErrorText;
#endif

		if (!AreAttributesValid(InContext, ErrorTextPtr))
		{
			if (ErrorTextPtr)
			{
				PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), *ErrorTextPtr);
			}
			return false;
		}
	}

	return true;
}

bool UPCGCustomHLSLKernel::ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InGraph);
	check(InBinding);
	
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());

	FPCGDataCollectionDesc OutputDataDesc;

	const FPCGPinPropertiesGPU* OutputPinProperties = CustomHLSLSettings->OutputPins.FindByPredicate([InOutputPinLabel](const FPCGPinPropertiesGPU& InProps) {return InProps.Label == InOutputPinLabel; });
	if (!OutputPinProperties)
	{
		OutDataDesc = {};
		return true;
	}

	const FName FirstOutputPinLabel = CustomHLSLSettings->OutputPins[0].Label;

	// The primary output pin follows any rules prescribed by kernel type.
	if (InOutputPinLabel == FirstOutputPinLabel && CustomHLSLSettings->IsProcessorKernel())
	{
		FPCGDataCollectionDesc InputDesc;

		if (const FPCGPinProperties* FirstInputPinProps = GetFirstInputPin())
		{
			const FPCGKernelPin FirstKernelPin(KernelIndex, FirstInputPinProps->Label, /*bIsInput=*/true);

			if (!InBinding->ComputeKernelPinDataDesc(FirstKernelPin, &InputDesc))
			{
				ensure(false);
				return false;
			}
		}

		OutputDataDesc = MoveTemp(InputDesc);
	}
	else if (InOutputPinLabel == FirstOutputPinLabel && CustomHLSLSettings->KernelType == EPCGKernelType::PointGenerator)
	{
		// Generators always produce a single point data with known point count.
		OutputDataDesc.DataDescs.Emplace(EPCGDataType::Point, CustomHLSLSettings->PointCount);
	}
	else if (InOutputPinLabel == FirstOutputPinLabel && CustomHLSLSettings->KernelType == EPCGKernelType::TextureGenerator)
	{
		// Generators always produce a single texture data with known size.
		FPCGDataDesc& NewDataDesc = OutputDataDesc.DataDescs.Emplace_GetRef();
		NewDataDesc.Type = EPCGDataType::BaseTexture;
		NewDataDesc.ElementCount2D = CustomHLSLSettings->NumElements2D;
	}
	else if (ensure(OutputPinProperties))
	{
		ComputeDataDescFromPinProperties(*OutputPinProperties, MakeArrayView(CustomHLSLSettings->InputPins), InBinding, OutputDataDesc);
	}

	// Add attributes that will be created for this pin on the GPU. This will stomp any existing attributes if they collide!
	for (const FPCGKernelAttributeKey& CreatedKey : OutputPinProperties->PropertiesGPU.CreatedKernelAttributeKeys)
	{
		OutputDataDesc.AddAttributeToAllData(CreatedKey, InBinding);
	}

	// Try to propagate string keys across node. Not trivial because there could be one or more string key attributes on input pins and on output pins,
	// and it is in general hard to determine from source which string keys from input are being written to outputs. Try first collecting all string keys
	// from matching attribute names (across all input pins), and then fall back to collecting keys from all string key attributes across all inputs.
	bool bOutputHasStringKeys = false;

	for (const FPCGDataDesc& DataDesc : OutputDataDesc.DataDescs)
	{
		auto HasStringKey = [](const FPCGKernelAttributeDesc& InAttributeDesc)
			{
				return InAttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::StringKey || InAttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::Name;
			};

		if (DataDesc.AttributeDescs.FindByPredicate(HasStringKey))
		{
			bOutputHasStringKeys = true;
			break;
		}
	}

	if (bOutputHasStringKeys)
	{
		TArray<FPCGDataCollectionDesc> RelevantInputDataDescs;

		// Collect descriptions of input data items that have string key attributes.
		for(const FPCGPinProperties& PinProps : CustomHLSLSettings->InputPins)
		{
			FPCGDataCollectionDesc InputPinDesc;
			const FPCGKernelPin InputKernelPin(KernelIndex, PinProps.Label, /*bIsInput=*/true);
			ensure(InBinding->ComputeKernelPinDataDesc(InputKernelPin, &InputPinDesc));

			bool bFoundStringKeyAttribute = false;

			for (FPCGDataDesc& DataDesc : OutputDataDesc.DataDescs)
			{
				for (FPCGKernelAttributeDesc& AttributeDesc : DataDesc.AttributeDescs)
				{
					bFoundStringKeyAttribute |= (AttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::StringKey || AttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::Name);
				}
			}

			if (bFoundStringKeyAttribute)
			{
				RelevantInputDataDescs.Add(InputPinDesc);
			}
		}

		if (!RelevantInputDataDescs.IsEmpty())
		{
			for (FPCGDataDesc& DataDesc : OutputDataDesc.DataDescs)
			{
				for (FPCGKernelAttributeDesc& AttributeDesc : DataDesc.AttributeDescs)
				{
					if (AttributeDesc.AttributeKey.Type != EPCGKernelAttributeType::StringKey && AttributeDesc.AttributeKey.Type != EPCGKernelAttributeType::Name)
					{
						continue;
					}

					bool bFoundMatchingAttribute = false;

					for (const FPCGDataCollectionDesc& InputPinDataDesc : RelevantInputDataDescs)
					{
						// Try to find string keys for matching attributes on inputs. E.g. if we are processing an output attribute named 'MeshPath',
						// look at data on all input pins for an attribute named MeshPath and assume we could use any of its values - copy the string keys.
						for (const FPCGDataDesc& InputDataDesc : InputPinDataDesc.DataDescs)
						{
							for (const FPCGKernelAttributeDesc& InputAttributeDesc : InputDataDesc.AttributeDescs)
							{
								const bool bIsString = InputAttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::StringKey || InputAttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::Name;
								if (bIsString && InputAttributeDesc.AttributeKey.Identifier == AttributeDesc.AttributeKey.Identifier)
								{
									AttributeDesc.AddUniqueStringKeys(InputAttributeDesc.GetUniqueStringKeys());
									bFoundMatchingAttribute = true;
									break;
								}
							}
						}
					}

					if (!bFoundMatchingAttribute)
					{
						// We didn't find an exact attribute. Fall back to finding any and all string keys. This is concerning and perhaps we can
						// have additional hinting mechanisms in the kernel source or in the node UI.
						for (const FPCGDataCollectionDesc& InputPinDataDesc : RelevantInputDataDescs)
						{
							// Try to find string keys for matching attributes on inputs. E.g. if we are processing an output attribute named 'MeshPath',
							// look at data on all input pins for an attribute named MeshPath and assume we could use any of its values - copy the string keys.
							for (const FPCGDataDesc& InputDataDesc : InputPinDataDesc.DataDescs)
							{
								for (const FPCGKernelAttributeDesc& InputAttributeDesc : InputDataDesc.AttributeDescs)
								{
									if (InputAttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::StringKey || InputAttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::Name)
									{
										AttributeDesc.AddUniqueStringKeys(InputAttributeDesc.GetUniqueStringKeys());
									}
								}
							}
						}
					}
				}
			}
		}
		else
		{
			// If there were no string keys found on any input pin then we are in a bad place. String values cannot be built on the GPU, they must
			// come in through an input.
			UE_LOG(LogPCG, Warning, TEXT("No incoming attributes to obtain string keys from."));
		}
	}

	OutDataDesc = MoveTemp(OutputDataDesc);
	return true;
}

#if WITH_EDITOR
FString UPCGCustomHLSLKernel::GetCookedSource(FPCGGPUCompilationContext& InOutContext) const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	FString ShaderPathName = CustomHLSLSettings->GetPathName();
	PCGComputeHelpers::ConvertObjectPathToShaderFilePath(ShaderPathName);

	const FString Source = ProcessShaderSource(InOutContext, ParsedSources[0]);
	const FString AdditionalSources = ProcessAdditionalShaderSources(InOutContext);
	//const bool bHasKernelKeyword = Source.Contains(TEXT("KERNEL"), ESearchCase::CaseSensitive);

	const FIntVector GroupSize(PCGComputeConstants::THREAD_GROUP_SIZE, 1, 1);
	const FString KernelFunc = FString::Printf(
		TEXT("[numthreads(%d, %d, %d)]\nvoid %s(uint3 GroupId : SV_GroupID, uint GroupIndex : SV_GroupIndex)"),
		GroupSize.X, GroupSize.Y, GroupSize.Z, *GetEntryPoint());

	const FString UnWrappedDispatchThreadId = FString::Printf(
		TEXT("GetUnWrappedDispatchThreadId(GroupId, GroupIndex, %d)"),
		GroupSize.X * GroupSize.Y * GroupSize.Z
	);

	// Used to signal that a kernel has executed. Set the most significant bit in NumData.
	FString SetAsExecuted = TEXT("    // Signal kernel executed by setting the most significant bit of NumData.\n");

	for (const FPCGPinPropertiesGPU& PinProps : CustomHLSLSettings->OutputPins)
	{
		if (PCGComputeHelpers::IsTypeAllowedInDataCollection(PinProps.AllowedTypes))
		{
			SetAsExecuted += FString::Format(TEXT("    if (all(GroupId == 0u) && GroupIndex == 0) {0}_SetAsExecutedInternal();\n"), { PinProps.Label.ToString() });
		}
	}

	// Per-kernel-type preamble. Set up shader inputs and initialize output data.
	FString KernelSpecificPreamble = TEXT("    // Kernel preamble\n");

	auto AddThreadInfoForPin = [&KernelSpecificPreamble](const FPCGPinProperties* InPin)
	{
		check(InPin);

		KernelSpecificPreamble += FString::Format(TEXT(
			"    uint {0}_DataIndex;\n"
			"    if (!{0}_GetThreadData(ThreadIndex, {0}_DataIndex, ElementIndex)) return;\n"),
			{ InPin->Label.ToString() });
	};

	if (CustomHLSLSettings->KernelType == EPCGKernelType::PointProcessor)
	{
		const FPCGPinProperties* InputPin = GetFirstInputPin();
		const FPCGPinPropertiesGPU* OutputPin = GetFirstOutputPin();

		if (InputPin && OutputPin)
		{
			KernelSpecificPreamble += TEXT("    uint ElementIndex; // Assumption - element index identical in input and output data.\n");

			AddThreadInfoForPin(InputPin);
			AddThreadInfoForPin(OutputPin);

			// If input point is invalid, mark output point as invalid and abort.
			KernelSpecificPreamble += FString::Format(TEXT(
				"    if ({0}_IsPointRemoved({0}_DataIndex, ElementIndex))\n"
				"    {\n"
				"        {1}_RemovePoint({1}_DataIndex, ElementIndex);\n"
				"        return;\n"
				"    }\n"),
				{ InputPin->Label.ToString(), OutputPin->Label.ToString() });

			// Automatically copy value of all attributes for this element.
			// TODO pass in IDs of attributes that are actually present.
			KernelSpecificPreamble += FString::Format(TEXT(
				"\n"
				"    // Point processor always initializes outputs by copying input data elements.\n"
				"    PCG_COPY_ALL_ATTRIBUTES_TO_OUTPUT({1}, {0}, {1}_DataIndex, ElementIndex, {0}_DataIndex, ElementIndex);\n"),
				{ InputPin->Label.ToString(), OutputPin->Label.ToString() });
		}
	}
	else if (CustomHLSLSettings->KernelType == EPCGKernelType::PointGenerator)
	{
		KernelSpecificPreamble += FString::Format(TEXT(
			"    const uint NumElements = {0};\n"
			"    // NumPoints is deprecated.\n"
			"    const uint NumPoints = NumElements;\n"
		), { CustomHLSLSettings->PointCount });

		if (const FPCGPinPropertiesGPU* OutputPin = GetFirstOutputPin())
		{
			KernelSpecificPreamble += TEXT("uint ElementIndex; // Assumption - element index identical in input and output data.\n");

			AddThreadInfoForPin(OutputPin);

			KernelSpecificPreamble += FString::Format(TEXT(
				"\n"
				"    // Initialize all values to defaults for output pin {0}\n"
				"    {0}_InitializePoint({0}_DataIndex, ElementIndex);\n"),
				{ OutputPin->Label.ToString() });
		}
	}
	else if (CustomHLSLSettings->KernelType == EPCGKernelType::TextureProcessor)
	{
		const FPCGPinProperties* InputPin = GetFirstInputPin();
		const FPCGPinPropertiesGPU* OutputPin = GetFirstOutputPin();

		if (InputPin && OutputPin)
		{
			KernelSpecificPreamble += TEXT("    uint2 ElementIndex; // Assumption - texel index identical in input and output data.\n");

			AddThreadInfoForPin(InputPin);
			AddThreadInfoForPin(OutputPin);

			// Automatically copy the input texture across.
			KernelSpecificPreamble += FString::Format(TEXT(
				"\n"
				"    // Texture processor always initializes outputs by copying the input texture.\n"
				"    {1}_Store({1}_DataIndex, ElementIndex, {0}_Load({0}_DataIndex, ElementIndex));\n"),
				{ InputPin->Label.ToString(), OutputPin->Label.ToString() });
		}
	}
	else if (CustomHLSLSettings->KernelType == EPCGKernelType::TextureGenerator)
	{
		KernelSpecificPreamble += FString::Format(TEXT("    const uint2 NumElements = uint2({0}, {1});\n"), { CustomHLSLSettings->NumElements2D.X, CustomHLSLSettings->NumElements2D.Y });

		if (const FPCGPinPropertiesGPU* OutputPin = GetFirstOutputPin())
		{
			KernelSpecificPreamble += TEXT("    uint2 ElementIndex; // Assumption - texel index identical in input and output data.\n");

			AddThreadInfoForPin(OutputPin);

			KernelSpecificPreamble += FString::Format(TEXT(
				"\n"
				"    // Zero-initialize for output pin {0}\n"
				"    {0}_Store({0}_DataIndex, ElementIndex, (float4)0.0f);\n"),
				{ OutputPin->Label.ToString() });
		}
	}

	FString Result;

	// Note, it would be preferable to have the AdditionalSources included via the kernel CreateAdditionalSources(), but when the HLSL is composed, those additional sources are
	// placed above the data interfaces, so any additional sources would be unable to utilize functions provided by the data interfaces. Therefore we just inject them by hand here.

	// TODO: Support KERNEL keyword in shader source. Could be handy for external source assets and breaking kernels into sections to
	// support pin/attribute declarations, etc.
	/*if (bHasKernelKeyword)
	{
		Source.ReplaceInline(TEXT("KERNEL"), TEXT("void __kernel_func(uint ThreadIndex)"), ESearchCase::CaseSensitive);

		Result = FString::Printf(TEXT(
			"#line 0 \"%s\"\n" // ShaderPathName
			"%s\n" // AdditionalSources
			"%s\n" // Source
			"%s { __kernel_func(%s); }\n"), // KernelFunc, UnWrappedDispatchThreadId
			*ShaderPathName, *AdditionalSources, *Source, *KernelFunc, *UnWrappedDispatchThreadId);
	}
	else*/
	{
		Result = FString::Printf(TEXT(
			"%s\n\n" // AdditionalSources
			"%s\n" // KernelFunc
			"{\n"
			"%s\n" // SetAsExecuted
			"	const uint ThreadIndex = %s;\n" // UnWrappedDispatchThreadId
			"	if (ThreadIndex >= GetNumThreads().x) return;\n"
			"%s\n" // KernelSpecificPreamble
			"#line 0 \"%s\"\n" // ShaderPathName
			"%s\n" // Source
			"}\n"),
			*AdditionalSources, *KernelFunc, *SetAsExecuted, *UnWrappedDispatchThreadId, *KernelSpecificPreamble, *ShaderPathName, *Source);
	}

	return Result;
}
#endif // WITH_EDITOR

int UPCGCustomHLSLKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	int ThreadCount = 0;

	if (CustomHLSLSettings->KernelType == EPCGKernelType::PointGenerator)
	{
		// Point generator has fixed thread count.
		ThreadCount = CustomHLSLSettings->PointCount;
	}
	else if (CustomHLSLSettings->KernelType == EPCGKernelType::TextureGenerator)
	{
		// Texture generator has fixed thread count.
		ThreadCount = CustomHLSLSettings->NumElements2D.X * CustomHLSLSettings->NumElements2D.Y;
	}
	else if (CustomHLSLSettings->IsProcessorKernel())
	{
		// Processing volume depends on data arriving on primary pin.
		if (const FPCGPinProperties* InputPin = GetFirstInputPin())
		{
			ThreadCount = GetElementCountForInputPin(*InputPin, InBinding);
		}
	}
	else if (CustomHLSLSettings->KernelType == EPCGKernelType::Custom)
	{
		if (CustomHLSLSettings->DispatchThreadCount == EPCGDispatchThreadCount::FromFirstOutputPin)
		{
			if (const FPCGPinPropertiesGPU* OutputPin = CustomHLSLSettings->OutputPins.IsEmpty() ? nullptr : &CustomHLSLSettings->OutputPins[0])
			{
				const FPCGDataCollectionDesc* Desc = InBinding->GetCachedKernelPinDataDesc(this, OutputPin->Label, /*bIsInput=*/false);
				ThreadCount = Desc ? OutputPin->GetElementCountMultiplier() * Desc->ComputeDataElementCount(OutputPin->AllowedTypes) : 0;
			}
		}
		else if (CustomHLSLSettings->DispatchThreadCount == EPCGDispatchThreadCount::FromProductOfInputPins)
		{
			for (const FName& PinLabel : CustomHLSLSettings->ThreadCountInputPinLabels)
			{
				if (const FPCGPinProperties* InputPin = CustomHLSLSettings->InputPins.FindByPredicate([PinLabel](const FPCGPinProperties& InProps) { return InProps.Label == PinLabel; }))
				{
					ThreadCount = FMath::Max(ThreadCount, 1) * GetElementCountForInputPin(*InputPin, InBinding);
				}
			}
		}
		else if (CustomHLSLSettings->DispatchThreadCount == EPCGDispatchThreadCount::Fixed)
		{
			ThreadCount = CustomHLSLSettings->FixedThreadCount;
		}
	}
	else
	{
		checkNoEntry();
	}

	if (IsThreadCountMultiplierInUse())
	{
		ThreadCount *= CustomHLSLSettings->ThreadCountMultiplier;
	}

	return ThreadCount;
}

void UPCGCustomHLSLKernel::GetDataLabels(FName InPinLabel, TArray<FString>& OutDataLabels) const
{
	if (const FPCGDataLabels* DataLabels = PinDataLabels.PinToDataLabels.Find(InPinLabel))
	{
		OutDataLabels = DataLabels->Labels;
	}
}

void UPCGCustomHLSLKernel::GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const
{
	OutKeys.Append(KernelAttributeKeys);
}

uint32 UPCGCustomHLSLKernel::GetThreadCountMultiplier() const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	return CustomHLSLSettings->ThreadCountMultiplier;
}

uint32 UPCGCustomHLSLKernel::GetElementCountMultiplier(FName InOutputPinLabel) const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());

	if (const FPCGPinPropertiesGPU* PinProps = CustomHLSLSettings->OutputPins.FindByPredicate([InOutputPinLabel](const FPCGPinPropertiesGPU& InProps) { return InProps.Label == InOutputPinLabel; }))
	{
		return PinProps->GetElementCountMultiplier();
	}
	else
	{
		ensure(false);
		return 1u;
	}
}

void UPCGCustomHLSLKernel::GetInputPins(TArray<FPCGPinProperties>& OutPins) const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	OutPins = CustomHLSLSettings->InputPins;
}

void UPCGCustomHLSLKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	OutPins = CustomHLSLSettings->OutputPins;
}

#if WITH_EDITOR
void UPCGCustomHLSLKernel::InitEntryPoint()
{
	check(Settings);

	// Record the node title - used to name the kernel and shows up in profiling.
	const UPCGNode* Node = Cast<UPCGNode>(Settings->GetOuter());
	if (Node && Node->HasAuthoredTitle())
	{
		EntryPoint = Node->GetAuthoredTitleName().ToString();
	}
	else
	{
		EntryPoint = Settings->GetDefaultNodeTitle().ToString();
	}

	// We append the PCG kernel name here because of a weird issue on Mac where if the last word of the name of a
	// shader parameter matches the kernel name, the value for the parameter is always zero for unknown reasons
	// (e.g. parameter named "XXXWeight" & kernel also named "Weight"). Adding this bit of "random" string after
	// the user provided kernel name should greatly reduce the chance of things like that happening. We don't include
	// the full fname with number as this will change across executions and cause DDC misses.
	EntryPoint += TEXT("_") + GetFName().GetPlainNameString();

	const TCHAR InvalidCharacters[] =
	{
		TCHAR('\"'), TCHAR('\\'), TCHAR('\''), TCHAR('\"'), TCHAR(' '), TCHAR(','), TCHAR('.'), TCHAR('|'), TCHAR('&'), TCHAR('!'),
		TCHAR('~'), TCHAR('\n'), TCHAR('\r'), TCHAR('\t'), TCHAR('@'), TCHAR('#'), TCHAR('/'), TCHAR('('), TCHAR(')'), TCHAR('{'),
		TCHAR('}'), TCHAR('['), TCHAR(']'), TCHAR('='), TCHAR(';'), TCHAR(':'), TCHAR('^'), TCHAR('%'), TCHAR('$'), TCHAR('`'),
		TCHAR('-'), TCHAR('+'), TCHAR('*'), TCHAR('<'), TCHAR('>'), TCHAR('?')
	};

	for (TCHAR Char : InvalidCharacters)
	{
		EntryPoint.ReplaceCharInline(Char, TCHAR('_'));
	}
}

void UPCGCustomHLSLKernel::PopulateAttributeKeysFromPinSettings()
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());

	// Process each output pin for any new attributes they want to create.
	for (const FPCGPinPropertiesGPU& OutputPin : CustomHLSLSettings->OutputPins)
	{
		for (const FPCGKernelAttributeKey& AuthoredKey : OutputPin.PropertiesGPU.CreatedKernelAttributeKeys)
		{
			if (AuthoredKey.IsValid())
			{
				KernelAttributeKeys.AddUnique(AuthoredKey);
			}
		}
	}
}

// Do not merge to main. Brings a "runtime" copy of our tokenizer here so we can use it launching standalone from editor, which is
// an editor environment but does not have the PCGEditor module loaded.
class FPCGRuntimeHLSLSyntaxTokenizer : public FHlslSyntaxTokenizer
{
public:
	FPCGRuntimeHLSLSyntaxTokenizer(const FPCGSyntaxTokenizerParams& InParams = {})
	{
		Keywords.Append(InParams.AdditionalKeywords);
	}
};

void UPCGCustomHLSLKernel::ParseShaderSource()
{
	CreateParsedSources();

	static const TArray<FString> AttributeTypeStrings =
	{
		TEXT("Bool"),
		TEXT("Int"),
		TEXT("Uint"),
		TEXT("Float"),
		TEXT("Float2"),
		TEXT("Float3"),
		TEXT("Float4"),
		TEXT("Rotator"),
		TEXT("Quat"),
		TEXT("Transform"),
		TEXT("StringKey"),
		TEXT("Name"),
	};

	// Collect additional keywords, such as function getters and setters.
	TArray<FString> AdditionalKeywords;
	TArray<FString> AttributeKeywords;
	TArray<FString> CopyElementKeywords;
	TArray<FString> InitializeKeywords;

	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());

	for (const FPCGPinProperties& PinProps : CustomHLSLSettings->InputPins)
	{
		if (!!(PinProps.AllowedTypes & (EPCGDataType::Point | EPCGDataType::Param)))
		{
			for (const FString& AttributeTypeString : AttributeTypeStrings)
			{
				AttributeKeywords.Add(PinProps.Label.ToString() + "_" + PCGCustomHLSLKernel::AttributeFunctionGetKeyword + AttributeTypeString);
			}
		}
	}

	for (const FPCGPinProperties& PinProps : CustomHLSLSettings->OutputPins)
	{
		const FString PinStr = *PinProps.Label.ToString();

		if (!!(PinProps.AllowedTypes & (EPCGDataType::Point | EPCGDataType::Param)))
		{
			for (const FString& AttributeTypeString : AttributeTypeStrings)
			{
				AttributeKeywords.Add(PinStr + "_" + PCGCustomHLSLKernel::AttributeFunctionSetKeyword + AttributeTypeString);
			}

			for (const FPCGPinProperties& InputPinProps : CustomHLSLSettings->InputPins)
			{
				if (InputPinProps.AllowedTypes == PinProps.AllowedTypes)
				{
					CopyElementKeywords.Add(PinStr + "_" + PCGCustomHLSLKernel::CopyElementFunctionKeyword + "_" + InputPinProps.Label.ToString());
				}
			}
			
			InitializeKeywords.Add(PinStr + "_" + PCGCustomHLSLKernel::InitializeFunctionKeyword);
		}
		else if (!!(PinProps.AllowedTypes & EPCGDataType::BaseTexture))
		{
			InitializeKeywords.Add(PinStr + "_" + PCGCustomHLSLKernel::StoreFunctionKeyword);
		}
	}

	AdditionalKeywords.Append(AttributeKeywords);
	AdditionalKeywords.Append(CopyElementKeywords);
	AdditionalKeywords.Append(InitializeKeywords);

	FPCGSyntaxTokenizerParams TokenizerParams;
	TokenizerParams.AdditionalKeywords = MoveTemp(AdditionalKeywords);

	TSharedPtr<ISyntaxTokenizer> Tokenizer = MakeShared<FPCGRuntimeHLSLSyntaxTokenizer>(TokenizerParams);
	check(Tokenizer.IsValid());

	auto ParseHelper = [&](FPCGCustomHLSLParsedSource& InOutParsedSource)
	{
		TArray<ISyntaxTokenizer::FTokenizedLine> TokenizedLines;
		Tokenizer->Process(TokenizedLines, InOutParsedSource.Source);
		PCGCustomHLSLKernel::ParseTokens(InOutParsedSource.Source, TokenizedLines, InOutParsedSource.Tokens);

		// Information about an attribute function looking for a match.
		struct FAttributeFunctionMatch
		{
			FString PinStr;
			FString FuncStr;
			FString TypeStr;
			FString NameStr;
			int32 MatchBeginning = INDEX_NONE;
			int32 EncounteredCommas = 0;
			const int32 RequiredCommas = 2;

			void Reset()
			{
				PinStr = "";
				FuncStr = "";
				TypeStr = "";
				NameStr = "";
				MatchBeginning = INDEX_NONE;
				EncounteredCommas = 0;
			}
		};

		FAttributeFunctionMatch AttributeFunctionMatch;
		bool bLookingForAttributeFunctionMatch = false;

		FString SingleQuoteString = "";
		bool bLookingForSingleQuoteMatch = false;

		auto AddCompletedAttributeFunction = [&KernelAttributeKeys=KernelAttributeKeys, &AttributeFunctions=InOutParsedSource.AttributeFunctions](const FAttributeFunctionMatch& InAttributeFunction)
		{
			if (InAttributeFunction.EncounteredCommas != InAttributeFunction.RequiredCommas)
			{
				return;
			}

			// @todo_pcg: Validate NameStr in [a-zA-Z0-9 -_\/] ?
			const FString& PinStr = InAttributeFunction.PinStr;
			const FString& FuncStr = InAttributeFunction.FuncStr;
			const FString& TypeStr = InAttributeFunction.TypeStr;
			const FString& NameStr = InAttributeFunction.NameStr;
			const FString UsageString = PinStr + "_" + FuncStr + TypeStr;

			if (PinStr.IsEmpty() || FuncStr.IsEmpty() || TypeStr.IsEmpty() || NameStr.IsEmpty())
			{
				UE_LOG(LogPCG, Error, TEXT("Invalid attribute usage in shader source: '%s' on attribute name '%s'."), *UsageString, *NameStr);
				return;
			}

			const UEnum* AttributeTypeEnum = StaticEnum<EPCGKernelAttributeType>();
			check(AttributeTypeEnum);

			const int64 AttributeType = AttributeTypeEnum->GetValueByName(FName(*TypeStr));

			if (AttributeType == INDEX_NONE)
			{
				UE_LOG(LogPCG, Error, TEXT("Invalid attribute type in shader source: '%s' on attribute name '%s'."), *UsageString, *NameStr);
				return;
			}

			// Add the attribute if it hasn't already been referenced.
			FPCGKernelAttributeKey Key;
			Key.Type = static_cast<EPCGKernelAttributeType>(AttributeType);
			Key.SetSelector(FPCGAttributePropertySelector::CreateSelectorFromString(NameStr));

			if (Key.IsValid())
			{
				KernelAttributeKeys.AddUnique(Key);
				AttributeFunctions.Emplace(PinStr, FuncStr, AttributeType, NameStr, InAttributeFunction.MatchBeginning);
			}
		};

		const int32 NumTokens = InOutParsedSource.Tokens.Num();

		for (int32 TokenIndex = 0; TokenIndex < NumTokens; ++TokenIndex)
		{
			const FPCGCustomHLSLParsedSource::FToken& Token = InOutParsedSource.Tokens[TokenIndex];
			const FString TokenString = InOutParsedSource.Source.Mid(Token.Range.BeginIndex, Token.Range.Len());

			if (Token.Type == FPCGCustomHLSLParsedSource::ETokenType::Keyword)
			{
				if (AttributeKeywords.Contains(TokenString))
				{
					AttributeFunctionMatch.Reset();

					const int32 DelimiterIndex = TokenString.Find("_");
					AttributeFunctionMatch.PinStr = TokenString.Left(DelimiterIndex);
					AttributeFunctionMatch.FuncStr = TokenString.Mid(DelimiterIndex + 1, 3);
					AttributeFunctionMatch.TypeStr = TokenString.Mid(DelimiterIndex + 4, Token.Range.Len() - DelimiterIndex + 1);
					AttributeFunctionMatch.MatchBeginning = Token.Range.BeginIndex;
					bLookingForAttributeFunctionMatch = true;

					if (AttributeFunctionMatch.FuncStr == PCGCustomHLSLKernel::AttributeFunctionSetKeyword)
					{
						InOutParsedSource.InitializedOutputPins.Add(AttributeFunctionMatch.PinStr);
					}
				}
				else if (CopyElementKeywords.Contains(TokenString))
				{
					const int32 FirstDelimiterIndex = TokenString.Find("_");
					const int32 SecondDelimiterIndex = TokenString.Find("_", ESearchCase::IgnoreCase, ESearchDir::FromEnd);
					
					FString TargetPin = TokenString.Left(FirstDelimiterIndex);
					FString SourcePin = TokenString.RightChop(SecondDelimiterIndex + 1);

					InOutParsedSource.InitializedOutputPins.Add(TargetPin);
					InOutParsedSource.CopyElementFunctions.Emplace(MoveTemp(SourcePin), MoveTemp(TargetPin));
				}
				else if (InitializeKeywords.Contains(TokenString))
				{
					const int32 DelimiterIndex = TokenString.Find("_");
					FString PinStr = TokenString.Left(DelimiterIndex);

					InOutParsedSource.InitializedOutputPins.Add(MoveTemp(PinStr));
				}
			}
			else if (Token.Type == FPCGCustomHLSLParsedSource::ETokenType::Normal)
			{
				if (bLookingForSingleQuoteMatch && TokenString == "\'")
				{
					if (bLookingForAttributeFunctionMatch && AttributeFunctionMatch.EncounteredCommas == AttributeFunctionMatch.RequiredCommas)
					{
						AttributeFunctionMatch.NameStr = SingleQuoteString;
						AddCompletedAttributeFunction(AttributeFunctionMatch);
						bLookingForAttributeFunctionMatch = false;
					}

					SingleQuoteString = "";
					bLookingForSingleQuoteMatch = false;
				}
				else if (bLookingForAttributeFunctionMatch && TokenString == ",")
				{
					++AttributeFunctionMatch.EncounteredCommas;

					if (AttributeFunctionMatch.EncounteredCommas > AttributeFunctionMatch.RequiredCommas)
					{
						AttributeFunctionMatch.Reset();
						bLookingForAttributeFunctionMatch = false;
					}
				}
			}
			else if (Token.Type == FPCGCustomHLSLParsedSource::ETokenType::SingleQuotedString)
			{
				if (!bLookingForSingleQuoteMatch)
				{
					bLookingForSingleQuoteMatch = true;

					// Chop the leading single quote.
					SingleQuoteString = TokenString.RightChop(1);
				}
				else
				{
					SingleQuoteString += TokenString;
				}
			}
		}

		// @todo_pcg: Maybe this should also be parsed instead of regex'd, but it's not as trivial to create tokens to detect the data labels pattern.
		CollectDataLabels(InOutParsedSource);

		// Capture any pin inititialization that won't have been caught by parsing.
		// @todo_pcg: We should just add the point property setters as keywords to the parsing.
		for (const FPCGPinPropertiesGPU& Pin : CustomHLSLSettings->OutputPins)
		{
			if (Pin.Label == NAME_None)
			{
				continue;
			}

			const FString PinStr = Pin.Label.ToString();

			if (InOutParsedSource.Source.Contains(PinStr + TEXT("_Set")))
			{
				InOutParsedSource.InitializedOutputPins.Add(PinStr);
			}
		}
	};

	for (FPCGCustomHLSLParsedSource& ParsedSource : ParsedSources)
	{
		ParseHelper(ParsedSource);
	}
}

void UPCGCustomHLSLKernel::CreateParsedSources()
{
	TArray<TObjectPtr<UComputeSource>> AdditionalSources;
	TSet<TObjectPtr<UComputeSource>> VisitedAdditionalSources;

	auto TraverseAdditionalSources = [&AdditionalSources, &VisitedAdditionalSources](TObjectPtr<UComputeSource> AdditionalSource, auto&& RecursiveCall)
	{
		if (!AdditionalSource || VisitedAdditionalSources.Contains(AdditionalSource))
		{
			return;
		}

		VisitedAdditionalSources.Add(AdditionalSource);

		// We do a postfix traversal of the nested additional sources because we need them to be pasted higher in the resulting HLSL, since presumably a source depends on its additional sources.
		for (TObjectPtr<UComputeSource> NestedSource : AdditionalSource->AdditionalSources)
		{
			RecursiveCall(NestedSource, RecursiveCall);
		}

		AdditionalSources.Add(AdditionalSource);
	};

	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	TArray<TObjectPtr<UComputeSource>> AdditionalSourcesToProcess = CustomHLSLSettings->AdditionalSources;

	if (TObjectPtr<UComputeSource> KernelSourceOverride = CustomHLSLSettings->KernelSourceOverride)
	{
		ParsedSources.Emplace(KernelSourceOverride->GetSource());
		VisitedAdditionalSources.Add(KernelSourceOverride);
		AdditionalSourcesToProcess.Append(KernelSourceOverride->AdditionalSources);
	}
	else
	{
		ParsedSources.Emplace(CustomHLSLSettings->ShaderSource);
		ParsedSources.Emplace(CustomHLSLSettings->ShaderFunctions);
	}
 
	for (TObjectPtr<UComputeSource> RootAdditionalSource : AdditionalSourcesToProcess)
	{
		TraverseAdditionalSources(RootAdditionalSource, TraverseAdditionalSources);
	}

	// Now that the additional sources are in post-fix order, we can begin to parse them.
	for (TObjectPtr<UComputeSource> AdditionalSource : AdditionalSources)
	{
		check(AdditionalSource);
		ParsedSources.Emplace(AdditionalSource->GetSource());
	}
}

void UPCGCustomHLSLKernel::CollectDataLabels(const FPCGCustomHLSLParsedSource& InParsedSource)
{
	auto CollectDataLabelsForPin = [&InParsedSource=InParsedSource, &PinDataLabels=PinDataLabels](FName InPinLabel)
	{
		FPCGDataLabels& DataLabels = PinDataLabels.PinToDataLabels.FindOrAdd(InPinLabel);

		// Matches against {PinName}_AnyFunction('{DataLabel}'...
		const FString Pattern = FString::Format(TEXT("{0}_.*?[\\s]*?\\([\\s]*?'([a-zA-Z0-9_].*?)'"), { *InPinLabel.ToString() });

		// First capture: Data label (supports a - z, A - Z, 0 - 9, and underscores).
		constexpr int LabelCaptureGroup = 1;

		FRegexMatcher ModuleMatcher(FRegexPattern(Pattern), InParsedSource.Source);
		while (ModuleMatcher.FindNext())
		{
			DataLabels.Labels.AddUnique(ModuleMatcher.GetCaptureGroup(LabelCaptureGroup));
		}
	};

	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());

	for (const FPCGPinProperties& PinProps : CustomHLSLSettings->InputPins)
	{
		CollectDataLabelsForPin(PinProps.Label);
	}

	for (const FPCGPinProperties& PinProps : CustomHLSLSettings->OutputPins)
	{
		CollectDataLabelsForPin(PinProps.Label);
	}
}

bool UPCGCustomHLSLKernel::PerformStaticValidation()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCustomHLSLKernel::PerformStaticValidation);

	if (!Super::PerformStaticValidation())
	{
		return false;
	}

	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());

	if (CustomHLSLSettings->OutputPins.IsEmpty())
	{
#if PCG_KERNEL_LOGGING_ENABLED
		StaticLogEntries.Emplace(LOCTEXT("NoOutputs", "Custom HLSL nodes must have at least one output."), EPCGKernelLogVerbosity::Error);
#endif
		return false;
	}

	auto CheckPinLabel = [Settings=CustomHLSLSettings, &StaticLogEntries=StaticLogEntries](FName PinLabel)
	{
		if (PinLabel == NAME_None)
		{
#if PCG_KERNEL_LOGGING_ENABLED
			StaticLogEntries.Emplace(LOCTEXT("InvalidPinLabelNone", "Pin label 'None' is not a valid pin label."), EPCGKernelLogVerbosity::Error);
#endif
			return false;
		}

		bool bFoundPinLabel = false;

		auto IsAlreadyFound = [Settings, &StaticLogEntries, PinLabel, &bFoundPinLabel](const FPCGPinProperties PinProps)
		{
			if (PinProps.Label == PinLabel)
			{
				if (bFoundPinLabel)
				{
#if PCG_KERNEL_LOGGING_ENABLED
					StaticLogEntries.Emplace(FText::Format(LOCTEXT("DuplicatedPinLabels", "Duplicate pin label '{0}', all labels must be unique."), FText::FromName(PinLabel)), EPCGKernelLogVerbosity::Error);
#endif
					return true;
				}

				bFoundPinLabel = true;
			}

			return false;
		};

		for (const FPCGPinProperties& PinProps : Settings->InputPins)
		{
			if (IsAlreadyFound(PinProps))
			{
				return false;
			}
		}

		for (const FPCGPinProperties& PinProps : Settings->OutputPins)
		{
			if (IsAlreadyFound(PinProps))
			{
				return false;
			}
		}

		return true;
	};

	// Validate input pins
	bool bIsFirstInputPin = true;
	for (const FPCGPinProperties& Properties : CustomHLSLSettings->InputPins)
	{
		if (!CheckPinLabel(Properties.Label))
		{
			return false;
		}

		if (bIsFirstInputPin && CustomHLSLSettings->KernelType == EPCGKernelType::PointProcessor)
		{
			if (Properties.AllowedTypes != EPCGDataType::Point)
			{
#if PCG_KERNEL_LOGGING_ENABLED
				StaticLogEntries.Emplace(FText::Format(
					LOCTEXT("InvalidNonPointPrimaryInput", "'Point Processor' nodes require primary input pin to be of type 'Point', but found '{0}'."),
					FText::FromString(PCGCustomHLSLKernel::GetDataTypeString(Properties.AllowedTypes))),
					EPCGKernelLogVerbosity::Error);
#endif
				return false;
			}
		}
		else if (bIsFirstInputPin && CustomHLSLSettings->KernelType == EPCGKernelType::TextureProcessor)
		{
			if (!(Properties.AllowedTypes & EPCGDataType::BaseTexture))
			{
#if PCG_KERNEL_LOGGING_ENABLED
				StaticLogEntries.Emplace(FText::Format(
					LOCTEXT("InvalidNonTexturePrimaryInput", "'Texture Processor' nodes require primary input pin to be of type 'Base Texture', but found '{0}'."),
					FText::FromString(PCGCustomHLSLKernel::GetDataTypeString(Properties.AllowedTypes))),
					EPCGKernelLogVerbosity::Error);
#endif
				return false;
			}
		}

		if (!PCGComputeHelpers::IsTypeAllowedAsInput(Properties.AllowedTypes))
		{
#if PCG_KERNEL_LOGGING_ENABLED
			StaticLogEntries.Emplace(FText::Format(
				LOCTEXT("InvalidInputType", "Unsupported input type '{0}', found on pin '{1}'."),
				FText::FromString(PCGCustomHLSLKernel::GetDataTypeString(Properties.AllowedTypes)),
				FText::FromName(Properties.Label)),
				EPCGKernelLogVerbosity::Error);
#endif
			return false;
		}

		bIsFirstInputPin = false;
	}

	// Validate output pins
	bool bIsFirstOutputPin = true;
	for (const FPCGPinPropertiesGPU& Properties : CustomHLSLSettings->OutputPins)
	{
		if (!CheckPinLabel(Properties.Label))
		{
			return false;
		}

		const bool bPinIsDefinedByKernel = bIsFirstOutputPin && (CustomHLSLSettings->IsPointKernel() || CustomHLSLSettings->IsTextureKernel());

		if (bIsFirstOutputPin && CustomHLSLSettings->IsPointKernel())
		{
			if (Properties.AllowedTypes != EPCGDataType::Point)
			{
#if PCG_KERNEL_LOGGING_ENABLED
				StaticLogEntries.Emplace(FText::Format(
					LOCTEXT("InvalidNonPointPrimaryOutput", "'Point Processor' and 'Point Generator' nodes require primary output pin to be of type 'Point', but found '{0}'."),
					FText::FromString(PCGCustomHLSLKernel::GetDataTypeString(Properties.AllowedTypes))),
					EPCGKernelLogVerbosity::Error);
#endif
				return false;
			}
		}
		else if (bIsFirstInputPin && CustomHLSLSettings->IsTextureKernel())
		{
			if (!(Properties.AllowedTypes & EPCGDataType::BaseTexture))
			{
#if PCG_KERNEL_LOGGING_ENABLED
				StaticLogEntries.Emplace(FText::Format(
					LOCTEXT("InvalidNonTexturePrimaryOutput", "'Texture Processor' and 'Texture Generator' nodes require primary output pin to be of type 'Base Texture', but found '{0}'."),
					FText::FromString(PCGCustomHLSLKernel::GetDataTypeString(Properties.AllowedTypes))),
					EPCGKernelLogVerbosity::Error);
#endif
				return false;
			}
		}

		if (!PCGComputeHelpers::IsTypeAllowedAsOutput(Properties.AllowedTypes))
		{
#if PCG_KERNEL_LOGGING_ENABLED
			StaticLogEntries.Emplace(FText::Format(
				LOCTEXT("InvalidOutputType", "Unsupported output type '{0}', found on pin '{1}'."),
				FText::FromString(PCGCustomHLSLKernel::GetDataTypeString(Properties.AllowedTypes)),
				FText::FromName(Properties.Label)),
				EPCGKernelLogVerbosity::Error);
#endif
			return false;
		}

		if (!bPinIsDefinedByKernel)
		{
			const FPCGPinPropertiesGPUStruct& Props = Properties.PropertiesGPU;

			if (Props.InitializationMode == EPCGPinInitMode::FromInputPins)
			{
				if (Props.PinsToInititalizeFrom.IsEmpty())
				{
#if PCG_KERNEL_LOGGING_ENABLED
					StaticLogEntries.Emplace(FText::Format(
						LOCTEXT("InitFromEmptyPins", "Output pin '{0}' tried to initialize from input pins, but no pins were specified."),
						FText::FromName(Properties.Label)),
						EPCGKernelLogVerbosity::Error);
#endif
					return false;
				}

				for (const FName InitPinName : Props.PinsToInititalizeFrom)
				{
					const FPCGPinProperties* InitPinProps = CustomHLSLSettings->InputPins.FindByPredicate([InitPinName](const FPCGPinProperties& InPinProps)
					{
						return InPinProps.Label == InitPinName;
					});

					if (InitPinProps)
					{
						if (!PCGComputeHelpers::IsTypeAllowedAsOutput(InitPinProps->AllowedTypes))
						{
#if PCG_KERNEL_LOGGING_ENABLED
							StaticLogEntries.Emplace(FText::Format(
								LOCTEXT("InitFromInvalidPinType", "Output pin '{0}' tried to initialize from input pin '{1}', but pin '{1}' has an invalid type."),
								FText::FromName(Properties.Label),
								FText::FromName(InitPinName)),
								EPCGKernelLogVerbosity::Error);
#endif
							return false;
						}
					}
					else
					{
#if PCG_KERNEL_LOGGING_ENABLED
						StaticLogEntries.Emplace(FText::Format(
							LOCTEXT("InitFromNonExistentPin", "Output pin '{0}' tried to initialize from non-existent input pin '{1}'."),
							FText::FromName(Properties.Label),
							FText::FromName(InitPinName)),
							EPCGKernelLogVerbosity::Error);
#endif
						return false;
					}
				}

				// TODO: Could do validation on data multiplicity for Pairwise, checking that data counts are 1 or N, but maybe that should be a runtime error instead.
			}

			const bool bUsingFixedDataCount = Props.InitializationMode == EPCGPinInitMode::Custom || Props.DataCountMode == EPCGDataCountMode::Fixed;

			if (bUsingFixedDataCount)
			{
				if (Props.DataCount < 1)
				{
#if PCG_KERNEL_LOGGING_ENABLED
					StaticLogEntries.Emplace(FText::Format(
						LOCTEXT("InvalidDataCount", "Invalid fixed data count {0} on output pin '{1}'. Must be greater than 0."),
						FText::AsNumber(Props.DataCount),
						FText::FromName(Properties.Label)),
						EPCGKernelLogVerbosity::Error);
#endif
					return false;
				}
			}

			const bool bUsingFixedElemCount = Props.InitializationMode == EPCGPinInitMode::Custom || Props.ElementCountMode == EPCGElementCountMode::Fixed;

			if (bUsingFixedElemCount)
			{
				if (Props.ElementCount < 1)
				{
#if PCG_KERNEL_LOGGING_ENABLED
					StaticLogEntries.Emplace(FText::Format(
						LOCTEXT("InvalidElementCount", "Invalid fixed num elements {0} on output pin '{1}'. Must be greater than 0."),
						FText::AsNumber(Props.ElementCount),
						FText::FromName(Properties.Label)),
						EPCGKernelLogVerbosity::Error);
#endif
					return false;
				}

				if (Props.NumElements2D.GetMin() < 1)
				{
#if PCG_KERNEL_LOGGING_ENABLED
					StaticLogEntries.Emplace(FText::Format(
						LOCTEXT("InvalidElementCount2D", "Invalid fixed num elements ({0}, {1}) on output pin '{2}'. Must be greater than 0."),
						FText::AsNumber(Props.NumElements2D.X),
						FText::AsNumber(Props.NumElements2D.Y),
						FText::FromName(Properties.Label)),
						EPCGKernelLogVerbosity::Error);
#endif
					return false;
				}
			}

			if (Props.ElementCountMultiplier < 1)
			{
#if PCG_KERNEL_LOGGING_ENABLED
				StaticLogEntries.Emplace(FText::Format(
					LOCTEXT("InvalidElementCountMultiplier", "Invalid element count multiplier {0} on output pin '{1}'. Must be greater than 0."),
					FText::AsNumber(Props.ElementCountMultiplier),
					FText::FromName(Properties.Label)),
					EPCGKernelLogVerbosity::Error);
#endif
				return false;
			}
		}

		bIsFirstOutputPin = false;
	}

	if (CustomHLSLSettings->KernelType == EPCGKernelType::Custom && CustomHLSLSettings->DispatchThreadCount == EPCGDispatchThreadCount::FromProductOfInputPins)
	{
		if (CustomHLSLSettings->ThreadCountInputPinLabels.IsEmpty())
		{
#if PCG_KERNEL_LOGGING_ENABLED
			StaticLogEntries.Emplace(LOCTEXT("MissingThreadCountPins", "Dispatch thread count is based on input pins but no labels have been set in Input Pins array."), EPCGKernelLogVerbosity::Error);
#endif
			return false;
		}

		for (const FName& Label : CustomHLSLSettings->ThreadCountInputPinLabels)
		{
			if (!CustomHLSLSettings->InputPins.FindByPredicate([Label](const FPCGPinProperties& InProps) { return InProps.Label == Label; }))
			{
#if PCG_KERNEL_LOGGING_ENABLED
				StaticLogEntries.Emplace(FText::Format(LOCTEXT("MissingThreadCountPin", "Invalid pin specified in Input Pins array: '{0}'."), FText::FromName(Label)), EPCGKernelLogVerbosity::Error);
#endif
				return false;
			}
		}
	}

	if (IsThreadCountMultiplierInUse())
	{
		if (CustomHLSLSettings->ThreadCountMultiplier < 1)
		{
#if PCG_KERNEL_LOGGING_ENABLED
			StaticLogEntries.Emplace(FText::Format(LOCTEXT("InvalidThreadCountMultiplier", "Thread Count Multiplier has invalid value ({0}). Must be greater than 0."), CustomHLSLSettings->ThreadCountMultiplier), EPCGKernelLogVerbosity::Error);
#endif
			return false;
		}
	}

	for (const FPCGKernelAttributeKey& AttributeKey : KernelAttributeKeys)
	{
		if (AttributeKey.Type == EPCGKernelAttributeType::Invalid)
		{
			const UEnum* AttributeTypeEnum = StaticEnum<EPCGKernelAttributeType>();
			check(AttributeTypeEnum);

#if PCG_KERNEL_LOGGING_ENABLED
			StaticLogEntries.Emplace(FText::Format(
				LOCTEXT("InvalidAttributeTypeNone", "Attribute '{0}' has invalid GPU attribute type '{1}', check the 'Attributes to Create' array on your pins."),
				FText::FromName(AttributeKey.Identifier.Name),
				FText::FromString(AttributeTypeEnum->GetNameStringByValue(static_cast<int64>(AttributeKey.Type)))),
				EPCGKernelLogVerbosity::Error);
#endif
			return false;
		}
	}

	if (!ValidateShaderSource(StaticLogEntries))
	{
		return false;
	}

	return true;
}

bool UPCGCustomHLSLKernel::ValidateShaderSource(TArray<FPCGKernelLogEntry>& OutLogEntries) const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());

	if (!CustomHLSLSettings->bMuteUnwrittenPinDataErrors && !AreAllOutputPinsWritten(OutLogEntries))
	{
		return false;
	}

	// @todo_pcg: Validation of parsed attribute functions could be done here instead of during parsing?

	return true;
}

FString UPCGCustomHLSLKernel::ProcessShaderSource(FPCGGPUCompilationContext& InOutContext, const FPCGCustomHLSLParsedSource& InParsedSource) const
{
	FString OutShaderSource = InParsedSource.Source;

	const FPCGKernelAttributeTable* StaticAttributeTable = InOutContext.GetStaticAttributeTable();

	if (!ensure(StaticAttributeTable))
	{
		return OutShaderSource;
	}

	// Replacement relies on precomputed indices into the source strings, therefore the replacement must take place
	// before any other modifications. Otherwise, the indices will be incorrect and the source will become gibberish.
	using FReplacement = TTuple</*ReplacementString=*/FString, /*ReplaceStartIndex=*/int32, /*ReplaceEndIndex=*/int32>;
	TArray<FReplacement> Replacements;

	const UEnum* AttributeTypeEnum = StaticEnum<EPCGKernelAttributeType>();
	check(AttributeTypeEnum);

	// We inject attribute IDs directly into the source. This is most efficient and saves us passing them into the kernel. However
	// the trade off is that a shader with a shared source will generate multiple variants if used in different compute graphs with
	// different attribute IDs.
	for (const FPCGParsedAttributeFunction& ParsedFunction : InParsedSource.AttributeFunctions)
	{
		const int64 AttrType = ParsedFunction.AttributeType;

		if (!ensure(AttrType != INDEX_NONE))
		{
			break;
		}

		const FPCGKernelAttributeKey AttributeKey(FPCGAttributePropertySelector::CreateSelectorFromString(*ParsedFunction.AttributeName), static_cast<EPCGKernelAttributeType>(AttrType));
		const FString SourceDefinition = FString::Format(TEXT("'{0}'"), { ParsedFunction.AttributeName });

		const int32 AttributeIndex = StaticAttributeTable->GetAttributeId(AttributeKey);
		const int32 ReplaceStartIndex = OutShaderSource.Find(SourceDefinition, ESearchCase::CaseSensitive, ESearchDir::FromStart, ParsedFunction.MatchBeginning);
		const int32 ReplaceEndIndex = ReplaceStartIndex + SourceDefinition.Len();

		Replacements.Emplace(FString::FromInt(AttributeIndex), ReplaceStartIndex, ReplaceEndIndex);
	}

	// We inject data IDs directly into the source. They will get remapped to data indices using a label resolver data interface.
	for (const TPair<FName, FPCGDataLabels>& Pair : PinDataLabels.PinToDataLabels)
	{
		const FName PinLabel = Pair.Key;
		const TArray<FString>& DataLabels = Pair.Value.Labels;

		for (int DataId = 0; DataId < DataLabels.Num(); ++DataId)
		{
			const FString& DataLabel = DataLabels[DataId];
			const FString ReplacementStr = FString::Format(TEXT("{0}_GetDataIndexFromIdInternal(/*DataId=*/{1}u)"),
			{
				PCGComputeHelpers::GetDataLabelResolverName(PinLabel),
				FString::FromInt(DataId)
			});

			// Matches against {PinName}_AnyFunction('{DataLabel}'...
			// First capture group is the data label, so that we can find & replace it by index.
			const FString Pattern = FString::Format(TEXT("{0}_.*?[\\s]*?\\([\\s]*?('{1}')"), { *PinLabel.ToString(), DataLabel });
			FRegexMatcher ModuleMatcher(FRegexPattern(Pattern), OutShaderSource);

			while (ModuleMatcher.FindNext())
			{
				const int32 DataLabelCharIndexStart = ModuleMatcher.GetCaptureGroupBeginning(1);
				const int32 DataLabelCharIndexEnd = ModuleMatcher.GetCaptureGroupEnding(1);

				Replacements.Emplace(ReplacementStr, DataLabelCharIndexStart, DataLabelCharIndexEnd);
			}
		}
	}

	// Sort the replacements by replacement index and apply them to the source in reverse order.
	// Note: Assumes that two replacements do not overlap.
	Algo::Sort(Replacements, [](const FReplacement& A, const FReplacement& B) { return A.Get<1>() < B.Get<1>(); });

	for (int I = Replacements.Num() - 1; I >= 0; --I)
	{
		const FString& ReplacementString = Replacements[I].Get<0>();
		const int32 ReplaceStartIndex = Replacements[I].Get<1>();
		const int32 ReplaceEndIndex = Replacements[I].Get<2>();

		OutShaderSource = OutShaderSource.Left(ReplaceStartIndex) + ReplacementString + OutShaderSource.RightChop(ReplaceEndIndex);
	}

	// Remove old-school stuff.
	OutShaderSource.ReplaceInline(TEXT("\r"), TEXT(""));

	// @todo_pcg: Replace using token ranges instead of find/replace, similar to what we do with the parsed attribute functions.
	// Replace function calls like Out_CopyElementFrom_In(...) with macro PCG_COPY_ALL_ATTRIBUTES_TO_OUTPUT(Out, In, ...).
	for (const FPCGParsedCopyElementFunction& ParsedFunction : InParsedSource.CopyElementFunctions)
	{
		OutShaderSource.ReplaceInline(
			*FString::Format(TEXT("{2}_{0}_{1}("), { PCGCustomHLSLKernel::CopyElementFunctionKeyword, ParsedFunction.SourcePin, ParsedFunction.TargetPin }),
			*FString::Format(TEXT("PCG_COPY_ALL_ATTRIBUTES_TO_OUTPUT({1}, {0}, "), { ParsedFunction.SourcePin, ParsedFunction.TargetPin })
		);
	}

	return OutShaderSource;
}

FString UPCGCustomHLSLKernel::ProcessAdditionalShaderSources(FPCGGPUCompilationContext& InOutContext) const
{
	// @todo_pcg: We should pivot to a stringbuilder here for perf.
	FString OutShaderSource;

	// The first parsed source is reserved for the kernel source.
	for (int SourceIndex = 1; SourceIndex < ParsedSources.Num(); ++SourceIndex)
	{
		OutShaderSource += ProcessShaderSource(InOutContext, ParsedSources[SourceIndex]) + "\n\n";
	}

	return OutShaderSource;
}

bool UPCGCustomHLSLKernel::AreAllOutputPinsWritten(TArray<FPCGKernelLogEntry>& OutLogEntries) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCustomHLSLKernel::AreAllOutputPinsWritten);

	auto IsPinInitializedBySource = [](const FPCGCustomHLSLParsedSource& InParsedSource, const FString& PinStr)
	{
		return InParsedSource.InitializedOutputPins.Contains(PinStr);
	};

	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());

	// Processor/Generator kernels initialize the first output pin data automatically.
	const bool bSkipFirstPin = CustomHLSLSettings->IsProcessorKernel() || CustomHLSLSettings->IsGeneratorKernel();

	for (int32 I = 0; I < CustomHLSLSettings->OutputPins.Num(); ++I)
	{
		if (I == 0 && bSkipFirstPin)
		{
			continue;
		}

		const FPCGPinPropertiesGPU& PinProps = CustomHLSLSettings->OutputPins[I];
		const FString& PinStr = PinProps.Label.ToString();
		bool bInitializedByAnySource = false;

		for (const FPCGCustomHLSLParsedSource& ParsedSource : ParsedSources)
		{
			if (IsPinInitializedBySource(ParsedSource, PinStr))
			{
				bInitializedByAnySource = true;
				break;
			}
		}

		if (!bInitializedByAnySource)
		{
#if PCG_KERNEL_LOGGING_ENABLED
			OutLogEntries.Emplace(FText::Format(
				LOCTEXT("PinMayNotBeWritten", "Data on pin '{0}' may be uninitialized. Add code to write to this data, or mute this error in the node settings."),
				{ FText::FromString(PinStr) }),
				EPCGKernelLogVerbosity::Error);
#endif
			return false;
		}
	}

	return true;
}
#endif // WITH_EDITOR

bool UPCGCustomHLSLKernel::IsThreadCountMultiplierInUse() const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	return CustomHLSLSettings->KernelType == EPCGKernelType::Custom && CustomHLSLSettings->DispatchThreadCount != EPCGDispatchThreadCount::Fixed;
}

bool UPCGCustomHLSLKernel::AreAttributesValid(FPCGContext* InContext, FText* OutErrorText) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCustomHLSLKernel::AreAttributesValid);

	// The context can either be a compute graph element context (if the compute graph was successfully created), otherwise
	// it will be the original CPU node context. We need the former to run the following validation.
	if (!InContext || !InContext->IsComputeContext())
	{
		return true;
	}

	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	const FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);
	const UPCGDataBinding* DataBinding = Context->DataBinding.Get();

	if (DataBinding)
	{
		TMap<FName, const FPCGDataCollectionDesc*> InputPinDescs;
		TMap<FName, const FPCGDataCollectionDesc*> OutputPinDescs;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCustomHLSLKernel::AreAttributesValid::GetCachedPinDataDescs);

			for (const FPCGPinProperties& Pin : CustomHLSLSettings->InputPins)
			{
				const FPCGDataCollectionDesc* DataDesc = DataBinding->GetCachedKernelPinDataDesc(this, Pin.Label, /*bIsInputPin=*/true);

				if (ensure(DataDesc))
				{
					InputPinDescs.Add(Pin.Label, DataDesc);
				}
			}

			for (const FPCGPinProperties& Pin : CustomHLSLSettings->OutputPins)
			{
				const FPCGDataCollectionDesc* DataDesc = DataBinding->GetCachedKernelPinDataDesc(this, Pin.Label, /*bIsInputPin=*/false);

				if (ensure(DataDesc))
				{
					OutputPinDescs.Add(Pin.Label, DataDesc);
				}
			}
		}

		auto ValidateParsedAttributeFunctions = [&InputPinDescs, &OutputPinDescs, OutErrorText](const TArray<FPCGParsedAttributeFunction>& ParsedAttributeFunctions)
		{
			for (const auto& ParsedFunction : ParsedAttributeFunctions)
			{
				const UEnum* AttributeTypeEnum = StaticEnum<EPCGKernelAttributeType>();
				check(AttributeTypeEnum);

				const FString& PinLabelStr = ParsedFunction.PinLabel;
				const FString& FunctionName = ParsedFunction.FunctionName;
				const FString& AttributeName = ParsedFunction.AttributeName;
				const FString TypeStr = AttributeTypeEnum->GetNameStringByValue(ParsedFunction.AttributeType);

				const FName PinLabel = FName(*PinLabelStr);
				const FPCGDataCollectionDesc* PinDesc = nullptr;

				auto ConstructFunctionText = [&ParsedFunction, &TypeStr]()
				{
					return FText::FromString(ParsedFunction.PinLabel + TEXT("_") + ParsedFunction.FunctionName + TypeStr);
				};

				if (FunctionName == PCGCustomHLSLKernel::AttributeFunctionSetKeyword)
				{
					if (const FPCGDataCollectionDesc** PinDescPtr = OutputPinDescs.Find(PinLabel))
					{
						PinDesc = *PinDescPtr;
					}

					if (!PinDesc && InputPinDescs.Find(PinLabel))
					{
#if PCG_KERNEL_LOGGING_ENABLED
						if (OutErrorText)
						{
							*OutErrorText = FText::Format(
								LOCTEXT("InvalidSetAttributeUsage", "Tried to call attribute function '{0}' on read-only input pin '{1}'."),
								ConstructFunctionText(),
								FText::FromName(PinLabel));
						}
#endif

						return false;
					}
				}
				else if (ensure(FunctionName == PCGCustomHLSLKernel::AttributeFunctionGetKeyword))
				{
					if (const FPCGDataCollectionDesc** PinDescPtr = InputPinDescs.Find(PinLabel))
					{
						PinDesc = *PinDescPtr;
					}
				}

				if (!PinDesc)
				{
#if PCG_KERNEL_LOGGING_ENABLED
					if (OutErrorText)
					{
						*OutErrorText = FText::Format(
							LOCTEXT("InvalidAttributePinName", "Tried to call attribute function '{0}' on non-existent pin '{1}'."),
							ConstructFunctionText(),
							FText::FromName(PinLabel));
					}
#endif

					return false;
				}


				const int64 AttrType = AttributeTypeEnum->GetValueByName(FName(*TypeStr));

				if (AttrType == INDEX_NONE)
				{
#if PCG_KERNEL_LOGGING_ENABLED
					if (OutErrorText)
					{
						*OutErrorText = FText::Format(
							LOCTEXT("InvalidAttributePinType", "Tried to call attribute function '{0}' on non-existent type '{1}'."),
							ConstructFunctionText(),
							FText::FromString(TypeStr));
					}
#endif

					return false;
				}

				const FPCGKernelAttributeDesc* AttrDesc = nullptr;
				const FPCGKernelAttributeKey AttrKey(FPCGAttributePropertySelector::CreateSelectorFromString(AttributeName), static_cast<EPCGKernelAttributeType>(AttrType));
				bool bFoundMatchingAttributeName = false;

				// Verify that the attribute exists on at least one data in pin data collection.
				for (const FPCGDataDesc& DataDesc : PinDesc->DataDescs)
				{
					AttrDesc = DataDesc.AttributeDescs.FindByPredicate([&AttrKey, &bFoundMatchingAttributeName](const FPCGKernelAttributeDesc& Desc)
					{
						const bool bAttributeNameMatches = Desc.AttributeKey.Identifier == AttrKey.Identifier;
						bFoundMatchingAttributeName |= bAttributeNameMatches;

						return bAttributeNameMatches && Desc.AttributeKey.Type == AttrKey.Type;
					});

					if (AttrDesc)
					{
						break;
					}
				}

				if (!AttrDesc)
				{
#if PCG_KERNEL_LOGGING_ENABLED
					if (OutErrorText)
					{
						if (bFoundMatchingAttributeName)
						{
							*OutErrorText = FText::Format(
								LOCTEXT("InvalidAttributeType", "Tried to call attribute function '{0}' on attribute '{1}' which is not of type '{2}'."),
								ConstructFunctionText(),
								FText::FromString(AttributeName),
								FText::FromString(TypeStr));
						}
						else
						{
							*OutErrorText = FText::Format(
								LOCTEXT("InvalidAttributeDNE", "Tried to call attribute function '{0}' on attribute '{1}' which does not exist."),
								ConstructFunctionText(),
								FText::FromString(AttributeName),
								FText::FromString(TypeStr));
						}
					}
#endif
					return false;
				}
			}

			return true;
		};

		for (const FPCGCustomHLSLParsedSource& ParsedSource : ParsedSources)
		{
			if (!ValidateParsedAttributeFunctions(ParsedSource.AttributeFunctions))
			{
				return false;
			}
		}
	}

	return true;
}

const FPCGPinProperties* UPCGCustomHLSLKernel::GetFirstInputPin() const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	return !CustomHLSLSettings->InputPins.IsEmpty() ? &CustomHLSLSettings->InputPins[0] : nullptr;
}

const FPCGPinPropertiesGPU* UPCGCustomHLSLKernel::GetFirstOutputPin() const
{
	const UPCGCustomHLSLSettings* CustomHLSLSettings = CastChecked<UPCGCustomHLSLSettings>(GetSettings());
	return !CustomHLSLSettings->OutputPins.IsEmpty() ? &CustomHLSLSettings->OutputPins[0] : nullptr;
}

int UPCGCustomHLSLKernel::GetElementCountForInputPin(const FPCGPinProperties& InInputPinProps, const UPCGDataBinding* InBinding) const
{
	check(InBinding);
	check(InBinding->Graph);

	const FPCGDataCollectionDesc* InputDesc = InBinding->GetCachedKernelPinDataDesc(this, InInputPinProps.Label, /*bIsInputPin=*/true);

	return ensure(InputDesc) ? InputDesc->ComputeDataElementCount(InInputPinProps.AllowedTypes) : 0;
}

#undef LOCTEXT_NAMESPACE
