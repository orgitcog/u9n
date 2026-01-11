// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateFramework/SkeletonBinding.h"

#include "SkeletonTemplateFramework/NamedElementTypes.h"
#include "Animation/Skeleton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletonBinding)

USkeletonBinding::USkeletonBinding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USkeletonBinding::InitializeFrom(const TObjectPtr<USkeletonTemplate> InTemplate, const TObjectPtr<USkeleton> InSkeleton)
{
	if (!ensure(InTemplate))
	{
		return false;
	}

	Template = InTemplate;
	Skeleton = InSkeleton;
	AttributeBindings.Reset();
	CachedSkeletonGuid = InSkeleton->GetGuid();
	
	NamedAttributes.Reset();
	{
		const FReferenceSkeleton& ReferenceSkeleton = InSkeleton->GetReferenceSkeleton();
		const int32 BoneNum = ReferenceSkeleton.GetNum();

		for (int32 BoneIndex = 0; BoneIndex < BoneNum; ++BoneIndex)
		{
			const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
			const FName ParentName = ParentIndex != INDEX_NONE ? ReferenceSkeleton.GetBoneName(ParentIndex) : NAME_None;
			
			FSkeletonNamedAttribute NewNamedAttribute;
			NewNamedAttribute.Name = ReferenceSkeleton.GetBoneName(BoneIndex);
			NewNamedAttribute.Type = USkeletonTemplateTransform::StaticClass();
			NewNamedAttribute.ParentName = ParentName;
			
			NamedAttributes.Add(NewNamedAttribute);
		}
	}
	
	for (const FSkeletonNamedAttributeSet& TemplateAttributeSet : InTemplate->GetNamedAttributeSets())
	{
		FSkeletonNamedAttributeSet BindingAttributeSet;
		BindingAttributeSet.Name = TemplateAttributeSet.Name;
		
		NamedAttributeSets.Add(BindingAttributeSet);
	}
	
	for (const FSkeletonNamedAttributeMapping& TemplateAttributeMapping : InTemplate->GetNamedAttributeMappings())
	{
		FSkeletonNamedAttributeMapping BindingAttributeMapping;
		BindingAttributeMapping.Name = TemplateAttributeMapping.Name;
		BindingAttributeMapping.SourceAttributeSet = TemplateAttributeMapping.SourceAttributeSet;
		BindingAttributeMapping.MappingTypes = TemplateAttributeMapping.MappingTypes;
		
		NamedAttributeMappings.Add(BindingAttributeMapping);
	}
	
	// TODO: Set CachedParentTemplateHash

	return true;
}

const FSkeletonNamedAttribute* const USkeletonBinding::FindNamedAttribute(const FName AttributeName) const
{
	return NamedAttributes.FindByPredicate([AttributeName](const FSkeletonNamedAttribute& Candidate)
		{
			return Candidate.Name == AttributeName;
		});
}
	

TObjectPtr<USkeleton> USkeletonBinding::GetSkeleton() const
{
	return Skeleton;
}

TObjectPtr<USkeletonTemplate> USkeletonBinding::GetSkeletonTemplate() const
{
	return Template;
}

const TArray<FSkeletonNamedAttribute>& USkeletonBinding::GetNamedAttributes() const
{
	return NamedAttributes;
}

const TArray<FSkeletonNamedAttributeSet>& USkeletonBinding::GetNamedAttributeSets() const
{
	return NamedAttributeSets;
}

const TArray<FSkeletonNamedAttributeMapping>& USkeletonBinding::GetNamedAttributeMappings() const
{
	return NamedAttributeMappings;
}

TArray<FSkeletonNamedAttribute> USkeletonBinding::GetUnboundSchemaNamedAttributes() const
{
	return Template->GetNamedAttributes().FilterByPredicate([this](const FSkeletonNamedAttribute& SchemaAttribute)
	{
		const int32 BoundAttributeIndex = AttributeBindings.IndexOfByPredicate([SchemaAttribute](const FSkeletonAttributeBinding& AttributeBinding)
		{
			return AttributeBinding.AttributeName == SchemaAttribute.Name;
		});

		return BoundAttributeIndex == INDEX_NONE;
	});
}

const FSkeletonAttributeBinding* const USkeletonBinding::FindAttributeBinding(const FName BindingAttributeName)  const
{
	const int32 AttributeIndex = FindAttributeBindingIndex(BindingAttributeName);
	if (AttributeIndex == INDEX_NONE)
	{
		return nullptr;
	}

	return &AttributeBindings[AttributeIndex];
}

int32 USkeletonBinding::FindAttributeBindingIndex(const FName BindingAttributeName) const
{
	return AttributeBindings.IndexOfByPredicate([BindingAttributeName](const FSkeletonAttributeBinding& AttributeBinding)
	{
		return AttributeBinding.BoneName == BindingAttributeName;
	});
}

FSkeletonNamedAttributeMappingEntry USkeletonBinding::CreateDefaultMappingEntry(const FSkeletonNamedAttributeMapping& AttributeMapping, const FSkeletonNamedAttribute& NamedAttribute)
{
	FSkeletonNamedAttributeMappingEntry NewMappingEntry;
	NewMappingEntry.AttributeName = NamedAttribute.Name;

	const FSkeletonNamedAttributeMappingType* const MappingType = AttributeMapping.MappingTypes.FindByPredicate([NamedAttribute](const FSkeletonNamedAttributeMappingType& MappingType)
	{
		return MappingType.SourceType == NamedAttribute.Type;
	});

	if (MappingType)
	{
		NewMappingEntry.Value = NewObject<USkeletonTemplateBaseType>(this, MappingType->TargetType);
	}

	return NewMappingEntry;
}

bool USkeletonBinding::BindAttribute(const FName BindingAttributeName, const FName TemplateAttributeName)
{
	if (TemplateAttributeName == NAME_None || BindingAttributeName == NAME_None)
	{
		return false;
	}
	
	const FSkeletonAttributeBinding* const ExistingAttributeBinding = FindAttributeBinding(BindingAttributeName);
	if (ExistingAttributeBinding != nullptr)
	{
		return false; // Template attribute is already bound
	}

	FSkeletonAttributeBinding NewBinding;
	NewBinding.AttributeName = TemplateAttributeName;
	NewBinding.BoneName = BindingAttributeName;

	AttributeBindings.Add(NewBinding);
	return true;
}

bool USkeletonBinding::UnbindTemplateNamedAttribute(const FName TemplateAttributeName)
{
	const int32 ExistingAttributeBindingIndex = AttributeBindings.IndexOfByPredicate([TemplateAttributeName](const FSkeletonAttributeBinding& AttributeBinding)
	{
		return AttributeBinding.AttributeName == TemplateAttributeName;
	});
	
	if (ExistingAttributeBindingIndex == INDEX_NONE)
	{
		return false; // Template attribute is not bound
	}

	AttributeBindings.RemoveAt(ExistingAttributeBindingIndex);	
	return true;
}

bool USkeletonBinding::UnbindBindingNamedAttribute(const FName BindingAttributeName)
{
	const int32 ExistingAttributeBindingIndex = FindAttributeBindingIndex(BindingAttributeName);
	if (ExistingAttributeBindingIndex == INDEX_NONE)
	{
		return false; // Template attribute is not bound
	}

	AttributeBindings.RemoveAt(ExistingAttributeBindingIndex);	
	return true;
}

void USkeletonBinding::GetChildNamedAttributes(const FName BindingAttributeName, TArray<FName>& OutChildren)
{
	for (const FSkeletonNamedAttribute& NamedAttribute : NamedAttributes)
	{
		if (NamedAttribute.ParentName == BindingAttributeName)
		{
			OutChildren.Add(NamedAttribute.Name);
			GetChildNamedAttributes(NamedAttribute.Name, OutChildren);
		}
	}
}

bool USkeletonBinding::AddNamedAttributeToSet(const FName AttributeSetName, const FName AttributeName)
{
	const FSkeletonNamedAttribute* const NamedAttribute = FindNamedAttribute(AttributeName);
	if (!NamedAttribute)
	{
		return false;
	}
	
	FSkeletonNamedAttributeSet* AttributeSet = FindMutableNamedAttributeSet(AttributeSetName);
	if (!AttributeSet)
	{
		return false;
	}

	if (AttributeSet->NamedAttributes.Contains(AttributeName))
	{
		return false;
	}

	AttributeSet->NamedAttributes.Add(AttributeName);

	// Add this attribute to all mappings with this set as a source set
	for (FSkeletonNamedAttributeMapping& AttributeMapping : NamedAttributeMappings)
	{
		if (AttributeMapping.SourceAttributeSet == AttributeSetName)
		{
			FSkeletonNamedAttributeMappingEntry NewMappingEntry = CreateDefaultMappingEntry(AttributeMapping, *NamedAttribute);
			AttributeMapping.TableData.Add(NewMappingEntry);
		}
	}
	
	return true;
}

bool USkeletonBinding::RemoveNamedAttributeFromSet(const FName AttributeSetName, const FName AttributeName)
{
	FSkeletonNamedAttributeSet* AttributeSet = FindMutableNamedAttributeSet(AttributeSetName);
	if (!AttributeSet)
	{
		return false;
	}

	return AttributeSet->NamedAttributes.Remove(AttributeName) > 0;
}

FSkeletonNamedAttributeSet* USkeletonBinding::FindMutableNamedAttributeSet(const FName AttributeSetName)
{
	return NamedAttributeSets.FindByPredicate([AttributeSetName](const FSkeletonNamedAttributeSet& Candidate)
		{
			return Candidate.Name == AttributeSetName;
		});
}

const FSkeletonNamedAttributeMapping* const USkeletonBinding::FindNamedAttributeMapping(const FName AttributeMappingName) const
{
	return NamedAttributeMappings.FindByPredicate([AttributeMappingName](const FSkeletonNamedAttributeMapping& Candidate)
		{
			return Candidate.Name == AttributeMappingName;
		});
}