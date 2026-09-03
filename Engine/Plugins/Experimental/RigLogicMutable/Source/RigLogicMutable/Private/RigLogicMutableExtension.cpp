// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigLogicMutableExtension.h"

#include "DNAAsset.h"
#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObject.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/UObjectGlobals.h"

const FName URigLogicMutableExtension::DNAPinType(TEXT("DNA"));
const FName URigLogicMutableExtension::DNABaseNodePinName(TEXT("DNA"));
const FText URigLogicMutableExtension::DNANodeCategory(FText::FromString(TEXT("DNA")));

FDNAPinData::FDNAPinData(FDNAPinData&& Source)
{
	// Invoke assignment operator
	*this = MoveTemp(Source);
}

FDNAPinData& FDNAPinData::operator=(FDNAPinData&& Source)
{
	ComponentName = Source.ComponentName;

	DNAAsset = Source.DNAAsset;
	Source.DNAAsset = nullptr;

	return *this;
}

void FDNAPinData::SetDNAAsset(UDNAAsset* SourceAsset)
{
	DNAAsset = SourceAsset;
	DNAAsset->bKeepDNAAfterInitialization = SourceAsset != nullptr;
}

TArray<FCustomizableObjectPinType> URigLogicMutableExtension::GetPinTypes() const
{
	TArray<FCustomizableObjectPinType> Result;
	
	FCustomizableObjectPinType& DNAType = Result.AddDefaulted_GetRef();
	DNAType.Name = DNAPinType;
	DNAType.DisplayName = FText::FromString(TEXT("RigLogic DNA"));
	DNAType.Color = FLinearColor::Red;

	return Result;
}

TArray<FObjectNodeInputPin> URigLogicMutableExtension::GetAdditionalObjectNodePins() const
{
	TArray<FObjectNodeInputPin> Result;

	FObjectNodeInputPin& DNAInputPin = Result.AddDefaulted_GetRef();
	DNAInputPin.PinType = DNAPinType;
	DNAInputPin.PinName = DNABaseNodePinName;
	DNAInputPin.DisplayName = FText::FromString(TEXT("RigLogic DNA"));
	DNAInputPin.bIsArray = false;

	return Result;
}

void URigLogicMutableExtension::OnSkeletalMeshCreated(
	const TArray<FInputPinDataContainer>& InputPinData,
	FName ComponentName,
	USkeletalMesh* SkeletalMesh) const
{
	// Find the DNA produced by the Customizable Object, if any, and assign it to the Skeletal Mesh

	for (const FInputPinDataContainer& Container : InputPinData)
	{
		if (Container.Pin.PinName == DNABaseNodePinName)
		{
			const FDNAPinData* Data = Container.Data.GetPtr<FDNAPinData>();
			if (Data
				&& Data->ComponentName == ComponentName
				&& Data->GetDNAAsset() != nullptr)
			{
				UDNAAsset* NewDNA = CopyDNAAsset(Data->GetDNAAsset(), SkeletalMesh);
				SkeletalMesh->AddAssetUserData(NewDNA);

				// A mesh can only have one DNA at a time, so if the CO produced multiple DNA
				// Assets, all but the first will be discarded.
				break;
			}
		}
	}
}

#if WITH_EDITOR
void URigLogicMutableExtension::MovePrivateReferencesToContainer(FInstancedStruct& Struct, UObject* Container) const
{
	FDNAPinData* Data = Struct.GetMutablePtr<FDNAPinData>();
	if (Data
		&& Data->GetDNAAsset() != nullptr)
	{
		Data->SetDNAAsset(CopyDNAAsset(Data->GetDNAAsset(), Container));
	}
}
#endif

UDNAAsset* URigLogicMutableExtension::CopyDNAAsset(const UDNAAsset* Source, UObject* OuterForCopy)
{
	check(IsInGameThread());

	return DuplicateObject(Source, OuterForCopy);
}
