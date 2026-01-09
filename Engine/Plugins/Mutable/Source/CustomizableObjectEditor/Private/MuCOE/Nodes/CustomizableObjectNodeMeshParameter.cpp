// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshParameter.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Engine/SkeletalMesh.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeMeshParameter::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	USkeletalMesh* SkeletalMesh = DefaultValue.LoadSynchronous();

	if (!SkeletalMesh)
	{
		return;
	}

	NamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Name"));

	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	if (const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel())
	{
		int32 LODIndex = 0;
		{
			const int32 NumSections = ImportedModel->LODModels[LODIndex].Sections.Num();
			for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
			{
				// Ignore disabled sections.
				if (ImportedModel->LODModels[LODIndex].Sections[SectionIndex].bDisabled)
				{
					continue;
				}

				UMaterialInterface* MaterialInterface = GetMaterialInterfaceFor( SectionIndex);

				FString Section = FString::Printf(TEXT("Section %i"), SectionIndex);
				if (MaterialInterface)
				{
					Section.Append(FString::Printf(TEXT(" : %s"), *MaterialInterface->GetName()));
				}

				// Mesh
				{
					UCustomizableObjectNodeMeshParameterPinDataSection* PinData = NewObject<UCustomizableObjectNodeMeshParameterPinDataSection>(this);
					PinData->Init( SectionIndex, ImportedModel->LODModels[LODIndex].NumTexCoords );

					const FString MeshPinName = FString::Printf(TEXT("Section %i - Mesh"), SectionIndex);

					UEdGraphPin* Pin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*MeshPinName), PinData);
					Pin->PinFriendlyName = FText::FromString(FString::Printf(TEXT("%s"), *Section));
				}
			}
		}
	}

}


void UCustomizableObjectNodeMeshParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeMeshParameter, DefaultValue))
	{
		ReconstructNode();
	}
}


bool UCustomizableObjectNodeMeshParameter::IsExperimental() const
{
	return true;
}


void UCustomizableObjectNodeMeshParameter::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::EnableMutableMacrosNewVersion)
	{
		if (!NamePin.Get())
		{
			NamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Name"));
		}
	}
}


FText UCustomizableObjectNodeMeshParameter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UEdGraphPin* ParamNamePin = NamePin.Get();

	if (TitleType == ENodeTitleType::ListView || (ParamNamePin && ParamNamePin->LinkedTo.Num()))
	{
		return LOCTEXT("Mesh_Parameter", "Mesh Parameter");
	}
	else if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText::Format(LOCTEXT("Mesh_Parameter_EditableTitle", "{0}"), FText::FromString(ParameterName));
	}
	else
	{
		return FText::Format(LOCTEXT("Mesh_Parameter_Title", "{0}\nMesh Parameter"), FText::FromString(ParameterName));
	}
}


FLinearColor UCustomizableObjectNodeMeshParameter::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


FText UCustomizableObjectNodeMeshParameter::GetTooltipText() const
{
	return LOCTEXT("Mesh_Parameter_Tooltip", "Expose a runtime modifiable Mesh parameter from the Customizable Object.");
}


void UCustomizableObjectNodeMeshParameter::OnRenameNode(const FString& NewName)
{
	if (!NewName.IsEmpty())
	{
		ParameterName = NewName;
	}
}


bool UCustomizableObjectNodeMeshParameter::GetCanRenameNode() const
{
	const UEdGraphPin* ParamNamePin = NamePin.Get();

	if (ParamNamePin && ParamNamePin->LinkedTo.Num())
	{
		return false;
	}

	return true;
}


void UCustomizableObjectNodeMeshParameter::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin == NamePin)
	{
		GetGraph()->NotifyGraphChanged();
	}
}


UTexture2D* UCustomizableObjectNodeMeshParameter::FindTextureForPin(const UEdGraphPin* Pin) const
{
	return nullptr;
}


TArray<UCustomizableObjectLayout*> UCustomizableObjectNodeMeshParameter::GetLayouts(const UEdGraphPin& MeshPin) const
{
	const UCustomizableObjectNodeMeshParameterPinDataSection& MeshPinData = GetPinData<UCustomizableObjectNodeMeshParameterPinDataSection>(MeshPin);
	return MeshPinData.Layouts;
}


TSoftObjectPtr<UObject> UCustomizableObjectNodeMeshParameter::GetMesh() const
{
	return DefaultValue;
}


UEdGraphPin* UCustomizableObjectNodeMeshParameter::GetMeshPin(const int32 LODIndex, const int32 SectionIndex) const
{
	for (UEdGraphPin* Pin : GetAllNonOrphanPins())
	{
		if (const UCustomizableObjectNodeMeshParameterPinDataSection* PinData = Cast<UCustomizableObjectNodeMeshParameterPinDataSection>(GetPinData(*Pin)))
		{
			if (PinData->GetSectionIndex() == SectionIndex)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}


void UCustomizableObjectNodeMeshParameter::GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const
{
	OutLODIndex = -1;
	OutSectionIndex = -1;
	OutLayoutIndex = -1;

	if (const UCustomizableObjectNodeMeshParameterPinDataSection* PinData = Cast<UCustomizableObjectNodeMeshParameterPinDataSection>(GetPinData(Pin)))
	{
		OutLODIndex = 0;
		OutSectionIndex = PinData->GetSectionIndex();
	}
}


void UCustomizableObjectNodeMeshParameter::GetPinSection(const UEdGraphPin& Pin, int32& OutSectionIndex) const
{
	if (const UCustomizableObjectNodeMeshParameterPinDataSection* PinData = Cast<UCustomizableObjectNodeMeshParameterPinDataSection>(GetPinData(Pin)))
	{
		OutSectionIndex = PinData->GetSectionIndex();
		return;
	}

	OutSectionIndex = -1;
}


UMaterialInterface* UCustomizableObjectNodeMeshParameter::GetMaterialInterfaceFor(const int32 SectionIndex) const
{
	if (FSkeletalMaterial* SkeletalMaterial = GetSkeletalMaterialFor(SectionIndex))
	{
		return SkeletalMaterial->MaterialInterface;
	}

	return nullptr;
}


FSkeletalMaterial* UCustomizableObjectNodeMeshParameter::GetSkeletalMaterialFor(const int32 SectionIndex) const
{
	USkeletalMesh* SkeletalMesh = DefaultValue.LoadSynchronous();

	if (!SkeletalMesh)
	{
		return nullptr;
	}

	const int32 SkeletalMeshMaterialIndex = GetSkeletalMaterialIndexFor(SectionIndex);
	if (SkeletalMesh->GetMaterials().IsValidIndex(SkeletalMeshMaterialIndex))
	{
		return &SkeletalMesh->GetMaterials()[SkeletalMeshMaterialIndex];
	}

	return nullptr;
}

int32 UCustomizableObjectNodeMeshParameter::GetSkeletalMaterialIndexFor(const int32 SectionIndex) const
{
	USkeletalMesh* SkeletalMesh = DefaultValue.LoadSynchronous();

	if (!SkeletalMesh)
	{
		return INDEX_NONE;
	}

	int32 LODIndex = 0;

	// We assume that LODIndex and MaterialIndex are valid for the imported model
	int32 SkeletalMeshMaterialIndex = INDEX_NONE;

	// Check if we have lod info map to get the correct material index
	if (const FSkeletalMeshLODInfo* LodInfo = SkeletalMesh->GetLODInfo(LODIndex))
	{
		if (LodInfo->LODMaterialMap.IsValidIndex(SectionIndex))
		{
			SkeletalMeshMaterialIndex = LodInfo->LODMaterialMap[SectionIndex];
		}
	}

	// Only deduce index when the explicit mapping is not found or there is no remap
	if (SkeletalMeshMaterialIndex == INDEX_NONE)
	{
		FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		if (ImportedModel && ImportedModel->LODModels.IsValidIndex(LODIndex) && ImportedModel->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex))
		{
			SkeletalMeshMaterialIndex = ImportedModel->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
		}
	}

	return SkeletalMeshMaterialIndex;
}


int32 UCustomizableObjectNodeMeshParameter::GetSkeletalMaterialIndexFor(const UEdGraphPin& Pin) const
{
	int32 SectionIndex;
	GetPinSection(Pin, SectionIndex);

	return GetSkeletalMaterialIndexFor(SectionIndex);
}


void UCustomizableObjectNodeMeshParameterPinDataSection::Init(int32 InSectionIndex, int32 NumTexCoords)
{
	SectionIndex = InSectionIndex;

	if (NumTexCoords > 0)
	{
		UObject* Outer = GetOuter();

		Layouts.SetNum(NumTexCoords);

		const int LODIndex = 0;

		for (int32 Index = 0; Index < NumTexCoords; ++Index)
		{
			Layouts[Index] = NewObject<UCustomizableObjectLayout>(Outer);
			Layouts[Index]->SetLayout(LODIndex, InSectionIndex, Index);
		}
	}
}


int32 UCustomizableObjectNodeMeshParameterPinDataSection::GetSectionIndex() const
{
	return SectionIndex;
}


bool UCustomizableObjectNodeMeshParameterPinDataSection::Equals(const UCustomizableObjectNodePinData& Other) const
{
	if (GetClass() != Other.GetClass())
	{
		return false;
	}

	const UCustomizableObjectNodeMeshParameterPinDataSection& OtherTyped = static_cast<const UCustomizableObjectNodeMeshParameterPinDataSection&>(Other);
	if (SectionIndex != OtherTyped.SectionIndex)
	{
		return false;
	}

	return Super::Equals(Other);
}


#undef LOCTEXT_NAMESPACE

