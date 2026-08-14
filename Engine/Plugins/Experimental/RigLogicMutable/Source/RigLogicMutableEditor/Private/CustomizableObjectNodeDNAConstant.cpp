// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomizableObjectNodeDNAConstant.h"

#include "DNAAsset.h"
#include "Engine/SkeletalMesh.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/ExtensionDataCompilerInterface.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPins.h"
#include "MuR/ExtensionData.h"
#include "MuT/NodeExtensionDataConstant.h"
#include "RigLogicMutableExtension.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectNodeDNAConstant)

#define LOCTEXT_NAMESPACE "RigLogicMutableEditor"

FText UCustomizableObjectNodeDNAConstant::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("DNA_Constant", "DNA Constant");
}

FLinearColor UCustomizableObjectNodeDNAConstant::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(URigLogicMutableExtension::DNAPinType);
}

FText UCustomizableObjectNodeDNAConstant::GetTooltipText() const
{
	return LOCTEXT("DNA_Constant_Tooltip", "RigLogic DNA");
}

void UCustomizableObjectNodeDNAConstant::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, URigLogicMutableExtension::DNAPinType, URigLogicMutableExtension::DNABaseNodePinName);
	OutputPin->bDefaultValueIsIgnored = true;
}

bool UCustomizableObjectNodeDNAConstant::ShouldAddToContextMenu(FText& OutCategory) const
{
	OutCategory = UEdGraphSchema_CustomizableObject::NC_Experimental;
	return true;
}


bool UCustomizableObjectNodeDNAConstant::IsExperimental() const
{
	return true;
}


mu::Ptr<mu::NodeExtensionData> UCustomizableObjectNodeDNAConstant::GenerateMutableNode(FExtensionDataCompilerInterface& CompilerInterface) const
{
	check(IsInGameThread());
	
	// Create node and extension data container
	mu::Ptr<mu::NodeExtensionDataConstant> Result = new mu::NodeExtensionDataConstant();

	FDNAPinData PinData;
	PinData.ComponentName = ComponentName;
	
	if (SkeletalMesh)
	{
		// Note that this may be nullptr if the mesh doesn't have a DNA asset
		PinData.SetDNAAsset(Cast<UDNAAsset>(SkeletalMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass())));
	}

	// Populate instanced struct
	FInstancedStruct Struct;
	Struct.InitializeAs<FDNAPinData>(MoveTemp(PinData));

	// DNA is usually quite large, so ideally it would be made a streaming constant to allow it to 
	// be loaded on demand.
	//
	// However, streaming constants don't support subobjects properly at the moment, so we use an
	// always-loaded constant instead.
	const bool bShouldStreamDNA = false;

	if (bShouldStreamDNA)
	{
		Result->SetValue(CompilerInterface.MakeStreamedExtensionData(MoveTemp(Struct)));
	}
	else
	{
		Result->SetValue(CompilerInterface.MakeAlwaysLoadedExtensionData(MoveTemp(Struct)));
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
