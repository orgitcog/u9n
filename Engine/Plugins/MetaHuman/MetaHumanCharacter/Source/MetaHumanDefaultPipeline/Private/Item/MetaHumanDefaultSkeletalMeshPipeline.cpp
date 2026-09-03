// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanDefaultSkeletalMeshPipeline.h"

#include "Engine/Texture.h"

namespace UE::MetaHuman::DefaultSkeletalMeshPipeline::Private
{
	namespace MetaDataKey
	{
		static const FName MaterialParamName = FName("MaterialParamName");
	}
}

UMetaHumanDefaultSkeletalMeshPipeline::UMetaHumanDefaultSkeletalMeshPipeline()
	: UMetaHumanSkeletalMeshPipeline()
{
	UpdateParameters();
}

#if WITH_EDITOR
void UMetaHumanDefaultSkeletalMeshPipeline::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultSkeletalMeshPipeline, SlotNames)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultSkeletalMeshPipeline, SlotTarget)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDefaultSkeletalMeshPipeline, SlotIndices))
	{
		UpdateParameters();
	}
}

void UMetaHumanDefaultSkeletalMeshPipeline::AddRuntimeParameter(TNotNull<FProperty*> InProperty, const FName& InMaterialParameterName)
{
	using namespace UE::MetaHuman::MaterialUtils;

	FMetaHumanMaterialParameter& Param = RuntimeMaterialParameters.AddDefaulted_GetRef();
	Param.InstanceParameterName = InProperty->GetFName();
	Param.SlotTarget = SlotTarget;
	Param.SlotNames = SlotNames;
	Param.SlotIndices = SlotIndices;
	Param.MaterialParameter.Name = InMaterialParameterName;
	Param.ParameterType = PropertyToParameterType(InProperty);
	Param.PropertyMetadata = CopyMetadataFromProperty(InProperty);
}
#endif

void UMetaHumanDefaultSkeletalMeshPipeline::UpdateParameters()
{
#if WITH_EDITOR
	using namespace UE::MetaHuman::DefaultSkeletalMeshPipeline::Private;
	using namespace UE::MetaHuman::MaterialUtils;

	RuntimeMaterialParameters.Empty();

	for (TFieldIterator<FProperty> PropertyIterator(UMetaHumanDefaultSkeletalMeshPipelineMaterialParameters::StaticClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;

		if (!Property || Property->HasAnyPropertyFlags(CPF_Deprecated))
		{
			continue;
		}

		const FString MaterialParamName = Property->GetMetaData(MetaDataKey::MaterialParamName);

		if (MaterialParamName.IsEmpty())
		{
			continue;
		}

		AddRuntimeParameter(Property, FName(MaterialParamName));
	}
#endif
}
