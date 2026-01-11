// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/ExtensionDataCompilerInterface.h"

#include "StructUtils/InstancedStruct.h"
#include "MuCO/CustomizableObjectStreamedResourceData.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuR/ExtensionData.h"

FExtensionDataCompilerInterface::FExtensionDataCompilerInterface(FMutableGraphGenerationContext& InGenerationContext)
	: GenerationContext(InGenerationContext)
{
}

TSharedPtr<const mu::FExtensionData> FExtensionDataCompilerInterface::MakeStreamedExtensionData(FInstancedStruct&& Data)
{
	TSharedPtr<mu::FExtensionData> Result = MakeShared<mu::FExtensionData>();
	Result->Origin = mu::FExtensionData::EOrigin::ConstantStreamed;
	Result->Index = GenerationContext.StreamedExtensionData.Num();
	
	FCustomizableObjectResourceData* CompileTimeExtensionData = &GenerationContext.StreamedExtensionData.AddDefaulted_GetRef();
	CompileTimeExtensionData->Data = MoveTemp(Data);

	return Result;
}

TSharedPtr<const mu::FExtensionData> FExtensionDataCompilerInterface::MakeAlwaysLoadedExtensionData(FInstancedStruct&& Data)
{
	TSharedPtr<mu::FExtensionData> Result = MakeShared<mu::FExtensionData>();
	Result->Origin = mu::FExtensionData::EOrigin::ConstantAlwaysLoaded;
	Result->Index = GenerationContext.AlwaysLoadedExtensionData.Num();

	FCustomizableObjectResourceData* CompileTimeExtensionData = &GenerationContext.AlwaysLoadedExtensionData.AddDefaulted_GetRef();
	CompileTimeExtensionData->Data = MoveTemp(Data);

	return Result;
}

const UObject* FExtensionDataCompilerInterface::GetOuterForAlwaysLoadedObjects()
{
	check(GenerationContext.CompilationContext.Object);
	return GenerationContext.CompilationContext.Object.Get();
}

void FExtensionDataCompilerInterface::AddGeneratedNode(const UCustomizableObjectNode* InNode)
{
	check(InNode);

	// A const_cast here is required because the new node needs to be added in the GeneratedNodes list so mutable can
	// discover new parameters that can potentially be attached to the extension node, however, this
	// function is called as ICustomizableObjectExtensionNode::GenerateMutableNode(this), so we need to cast the const away here.
	// Decided to do the case here so the use of AddGeneratedNode is as clean as possible
	GenerationContext.GeneratedNodes.Add(const_cast<UCustomizableObjectNode*>(InNode));
}

void FExtensionDataCompilerInterface::CompilerLog(const FText& InLogText, const UCustomizableObjectNode* InNode)
{
	GenerationContext.Log(InLogText, InNode);
}

