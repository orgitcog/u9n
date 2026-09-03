// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateFramework/SkeletonTemplate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletonTemplate)

USkeletonTemplate::USkeletonTemplate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const TArray<FSkeletonNamedAttribute>& USkeletonTemplate::GetNamedAttributes() const
{
	return NamedAttributes;
}

bool USkeletonTemplate::AddNamedAttribute(const FSkeletonNamedAttribute& NewNamedAttribute)
{
	if (NewNamedAttribute.Name == NAME_None)
	{
		return false; // Reserved name for denoting unrooted attributes
	}

	const int32 ExistingAttributeIndex = FindNamedAttributeIndex(NewNamedAttribute.Name);
	if (ExistingAttributeIndex == INDEX_NONE)
	{
		NamedAttributes.Add(NewNamedAttribute);
		return true;
	}

	return false;
}

bool USkeletonTemplate::RenameNamedAttribute(const FName OldAttributeName, const FName NewAttributeName)
{
	if (NewAttributeName == NAME_None)
	{
		return false; // Reserved name for denoting unrooted attributes
	}

	int32 AttributeToRenameIndex = FindNamedAttributeIndex(OldAttributeName);
	if (AttributeToRenameIndex == INDEX_NONE)
	{
		return false; // Attribute with old name does not exist
	}

	int32 ExistingAttributeIndex = FindNamedAttributeIndex(NewAttributeName);
	if (ExistingAttributeIndex != INDEX_NONE)
	{
		return false; // Attribute with this name already exists
	}

	for (FSkeletonNamedAttribute& NamedAttribute : NamedAttributes)
	{
		if (NamedAttribute.ParentName == OldAttributeName)
		{
			NamedAttribute.ParentName = NewAttributeName;
		}
	}
	
	FSkeletonNamedAttribute& NamedAttribute = NamedAttributes[AttributeToRenameIndex];
	NamedAttribute.Name = NewAttributeName;

	return true;
}

bool USkeletonTemplate::ReparentNamedAttribute(const FName AttributeName, const FName NewParentName)
{
	if (AttributeName == NewParentName)
	{
		return false; // Cannot set parent of self
	}

	FSkeletonNamedAttribute* AttributeToReparent = FindMutableNamedAttribute(AttributeName);
	if (!AttributeToReparent)
	{
		return false; // Attribute does not exist
	}

	FSkeletonNamedAttribute* NewParentAttribute = FindMutableNamedAttribute(NewParentName);
	if (!NewParentAttribute)
	{
		return false; // Parent does not exist
	}

	// Update hierarchy to prevent cycles
	{
		FSkeletonNamedAttribute* AncestorAttribute = NewParentAttribute;
		while (AncestorAttribute)
		{
			if (AncestorAttribute->ParentName == AttributeName)
			{
				AncestorAttribute->ParentName = AttributeToReparent->ParentName;
				break;
			}
			AncestorAttribute = FindMutableNamedAttribute(AncestorAttribute->ParentName);
		}
	}

	AttributeToReparent->ParentName = NewParentName;
	SortNamedAttributes();
	return true;
}

bool USkeletonTemplate::RemoveNamedAttribute(const FName AttributeToRemoveName, const ERemoveNamedAttributeBehavior Behavior)
{
	FName ParentOfAttributeToRemove = NAME_None;
	
	if (Behavior == ERemoveNamedAttributeBehavior::MoveChildrenToParent)
	{
		int32 AttributeToRemoveIndex = FindNamedAttributeIndex(AttributeToRemoveName);

		if (AttributeToRemoveIndex != INDEX_NONE)
		{
			ParentOfAttributeToRemove = NamedAttributes[AttributeToRemoveIndex].ParentName;
			NamedAttributes.RemoveAtSwap(AttributeToRemoveIndex, EAllowShrinking::No);
		}
		else
		{
			return false;
		}
	}

	const int32 NumNamedAttributes = NamedAttributes.Num();
	for (int32 Index = NumNamedAttributes - 1; Index >= 0; --Index)
	{
		FSkeletonNamedAttribute& NamedAttribute = NamedAttributes[Index];

		if (NamedAttribute.ParentName == AttributeToRemoveName)
		{
			if (Behavior == ERemoveNamedAttributeBehavior::RemoveChildren)
			{
				NamedAttributes.RemoveAtSwap(Index, EAllowShrinking::No);
			}
			else
			{
				NamedAttribute.ParentName = ParentOfAttributeToRemove;
			}
		}
	}

	SortNamedAttributes();
	return true;
}

bool USkeletonTemplate::SetNamedAttributeType(const FName AttributeName, const TObjectPtr<const UClass> NewType)
{
	FSkeletonNamedAttribute* NamedAttribute = FindMutableNamedAttribute(AttributeName);

	if (NamedAttribute)
	{
		NamedAttribute->Type = NewType;
		return true;
	}

	return false;
}

const TArray<FSkeletonNamedAttributeSet>& USkeletonTemplate::GetNamedAttributeSets() const
{
	return NamedAttributeSets;
}

const FSkeletonNamedAttributeSet* const USkeletonTemplate::FindNamedAttributeSet(const FName AttributeSetName) const
{
	return NamedAttributeSets.FindByPredicate([AttributeSetName](const FSkeletonNamedAttributeSet& Candidate)
		{
			return Candidate.Name == AttributeSetName;
		});
}

const FSkeletonNamedAttribute* const USkeletonTemplate::FindNamedAttribute(const FName AttributeName) const
{
	return NamedAttributes.FindByPredicate([AttributeName](const FSkeletonNamedAttribute& Candidate)
			{
				return Candidate.Name == AttributeName;
			});
}

bool USkeletonTemplate::AddAttributeSet(const FName AttributeSetName)
{
	FSkeletonNamedAttributeSet* AttributeSet = FindMutableNamedAttributeSet(AttributeSetName);
	if (AttributeSet)
	{
		return false; // Set already exists with that name
	}

	FSkeletonNamedAttributeSet NewSet;
	NewSet.Name = AttributeSetName;

	NamedAttributeSets.Add(NewSet);
	return true;
}

bool USkeletonTemplate::RemoveAttributeSet(const FName AttributeSetName)
{
	int32 AttributeSetIndex = FindNamedAttributeSetIndex(AttributeSetName);
	if (AttributeSetIndex == INDEX_NONE)
	{
		return false; // Cannot find set with that name
	}

	NamedAttributeSets.RemoveAt(AttributeSetIndex);
	return true;
}

bool USkeletonTemplate::AddAttributeToSet(const FName AttributeSetName, const FName AttributeName)
{
	FSkeletonNamedAttributeSet* AttributeSet = FindMutableNamedAttributeSet(AttributeSetName);
	if (!AttributeSet)
	{
		return false;
	}

	const int32 Index = AttributeSet->NamedAttributes.AddUnique(AttributeName);

	return Index != INDEX_NONE;
}

bool USkeletonTemplate::RemoveAttributeFromSet(const FName AttributeSetName, const FName AttributeName)
{
	FSkeletonNamedAttributeSet* AttributeSet = FindMutableNamedAttributeSet(AttributeSetName);
	if (!AttributeSet)
	{
		return false;
	}

	const int32 RemoveCount = AttributeSet->NamedAttributes.Remove(AttributeName);

	return RemoveCount > 0;
}

FSkeletonNamedAttributeMapping* USkeletonTemplate::FindMutableNamedAttributeMapping(const FName AttributeMappingName)
{
	return NamedAttributeMappings.FindByPredicate([AttributeMappingName](const FSkeletonNamedAttributeMapping& Candidate)
		{
			return Candidate.Name == AttributeMappingName;
		});
}

FSkeletonNamedAttributeSet* USkeletonTemplate::FindMutableNamedAttributeSet(const FName AttributeSetName)
{
	return NamedAttributeSets.FindByPredicate([AttributeSetName](const FSkeletonNamedAttributeSet& Candidate)
		{
			return Candidate.Name == AttributeSetName;
		});
}

bool USkeletonTemplate::RenameNamedAttributeSet(const FName AttributeSetName, const FName NewAttributeSetName)
{
	FSkeletonNamedAttributeSet* AttributeSetToRename = FindMutableNamedAttributeSet(AttributeSetName);
	if (!AttributeSetToRename)
	{
		return false; // No attribute set with old name
	}

	FSkeletonNamedAttributeSet* ExistingAttributeSet = FindMutableNamedAttributeSet(NewAttributeSetName);
	if (ExistingAttributeSet)
	{
		return false; // Attribute set with new name already exists
	}

	AttributeSetToRename->Name = NewAttributeSetName;
	return true;
}

const TArray<FSkeletonNamedAttributeMapping>& USkeletonTemplate::GetNamedAttributeMappings() const
{
	return NamedAttributeMappings;
}

const FSkeletonNamedAttributeMapping* const USkeletonTemplate::FindNamedAttributeMapping(const FName AttributeMappingName) const
{
	return NamedAttributeMappings.FindByPredicate([AttributeMappingName](const FSkeletonNamedAttributeMapping& Candidate)
		{
			return Candidate.Name == AttributeMappingName;
		});
}

bool USkeletonTemplate::AddAttributeMapping(const FName AttributeMappingName)
{
	if (const FSkeletonNamedAttributeMapping* const AttributeMapping = FindNamedAttributeMapping(AttributeMappingName))
	{
		return false; // Set already exists with that name
	}

	FSkeletonNamedAttributeMapping NewMapping;
	NewMapping.Name = AttributeMappingName;

	NamedAttributeMappings.Add(NewMapping);
	return true;	
}

bool USkeletonTemplate::RemoveAttributeMapping(const FName AttributeMappingName)
{
	const int32 AttributeMappingIndex = FindNamedAttributeMappingIndex(AttributeMappingName);
	if (AttributeMappingIndex == INDEX_NONE)
	{
		return false; // Cannot find set with that name
	}

	NamedAttributeSets.RemoveAt(AttributeMappingIndex);
	return true;
}

bool USkeletonTemplate::RenameNamedAttributeMapping(const FName AttributeMappingName, const FName NewAttributeMappingName)
{
	FSkeletonNamedAttributeMapping* AttributeMappingToRename = FindMutableNamedAttributeMapping(AttributeMappingName);
	if (!AttributeMappingToRename)
	{
		return false; // No attribute mapping with old name
	}

	const FSkeletonNamedAttributeMapping* const ExistingAttributeMapping = FindNamedAttributeMapping(NewAttributeMappingName);
	if (ExistingAttributeMapping)
	{
		return false; // Attribute mapping with new name already exists
	}

	AttributeMappingToRename->Name = NewAttributeMappingName;
	return true;
}

bool USkeletonTemplate::SetNamedAttributeMappingSourceSet(const FName AttributeMappingName, const FName NewSourceSet)
{
	FSkeletonNamedAttributeMapping* AttributeMappingToUpdate = FindMutableNamedAttributeMapping(AttributeMappingName);
	if (!AttributeMappingToUpdate)
	{
		return false; // No attribute mapping found
	}

	if (NewSourceSet != NAME_None)
	{
		const FSkeletonNamedAttributeSet* const AttributeSet = FindNamedAttributeSet(NewSourceSet);
		if (!AttributeSet)
		{
			return false; // No attribute set found
		}

		AttributeMappingToUpdate->SourceAttributeSet = NewSourceSet;
		AttributeMappingToUpdate->TableData.Reset(); // TODO: Keep table entries for attributes from the old source set that also exist in the new set

		for (const FName& AttributeName : AttributeSet->NamedAttributes)
		{
			FSkeletonNamedAttributeMappingEntry NewMappingEntry;
			NewMappingEntry.AttributeName = AttributeName;

			if (const FSkeletonNamedAttribute* const NamedAttribute = FindNamedAttribute(AttributeName))
			{
				const FSkeletonNamedAttributeMappingType* const MappingType = AttributeMappingToUpdate->MappingTypes.FindByPredicate([NamedAttribute](const FSkeletonNamedAttributeMappingType& MappingType)
				{
					return MappingType.SourceType == NamedAttribute->Type;
				});

				if (MappingType)
				{
					NewMappingEntry.Value = NewObject<USkeletonTemplateBaseType>(this, MappingType->TargetType);
				}
			}
			
			AttributeMappingToUpdate->TableData.Add(NewMappingEntry);
		}
	}
	else
	{
		AttributeMappingToUpdate->SourceAttributeSet = NAME_None;
		AttributeMappingToUpdate->TableData.Reset(); // TODO: Keep table entries for attributes from the old source set that also exist in the new set
	}

	return true;
}

FSkeletonNamedAttribute* USkeletonTemplate::FindMutableNamedAttribute(const FName AttributeName)
{
	return NamedAttributes.FindByPredicate([AttributeName](const FSkeletonNamedAttribute& Candidate)
		{
			return Candidate.Name == AttributeName;
		});
}

int32 USkeletonTemplate::FindNamedAttributeSetIndex(const FName AttributeSetName)
{
	return NamedAttributeSets.IndexOfByPredicate([AttributeSetName](const FSkeletonNamedAttributeSet& Candidate)
		{
			return Candidate.Name == AttributeSetName;
		});
}

int32 USkeletonTemplate::FindNamedAttributeMappingIndex(const FName AttributeMappingName)
{
	return NamedAttributeMappings.IndexOfByPredicate([AttributeMappingName](const FSkeletonNamedAttributeMapping& Candidate)
		{
			return Candidate.Name == AttributeMappingName;
		});
}

int32 USkeletonTemplate::FindNamedAttributeIndex(const FName AttributeName)
{
	return NamedAttributes.IndexOfByPredicate([AttributeName](const FSkeletonNamedAttribute& Candidate)
		{
			return Candidate.Name == AttributeName;
		});
}

// Util function to sort an array topologically for any type with unique FName identifiers and parent identifiers
// TODO: Will be used if needed to sort entries in sets and mappings similarly to how named mappings are sorted
template <typename ArrayType>
void SortByProperty(TArray<ArrayType>& Array, FName ArrayType::*NameProperty, FName ArrayType::*ParentNameProperty, TFunctionRef<ArrayType*(FName)> FindArrayItem)
{
	struct Node
	{
		FName AttributeName;
		TArray<FName> Children;
		int32 InDegree;

		Node(const FName InAttributeName)
			: AttributeName(InAttributeName)
			, InDegree(0)
		{
		}
	};

	TMap<FName, Node> Graph;

	for (const ArrayType& ArrayItem : Array)
	{
		Graph.Add(ArrayItem.*NameProperty, Node(ArrayItem.*NameProperty));
		Graph.Add(ArrayItem.*ParentNameProperty, Node(ArrayItem.*ParentNameProperty));
	}

	for (const ArrayType& ArrayItem : Array)
	{
		Graph[ArrayItem.*ParentNameProperty].Children.Add(ArrayItem.*NameProperty);
		++Graph[ArrayItem.*NameProperty].InDegree;
	}

	TArray<FName> NodeQueue;
	for (const TPair<FName, Node>& AttributeDegree : Graph)
	{
		if (AttributeDegree.Value.InDegree == 0)
		{
			NodeQueue.Add(AttributeDegree.Key);
		}
	}

	TArray<ArrayType> SortedArray;

	while (!NodeQueue.IsEmpty())
	{
		const FName AttributeName = NodeQueue.Pop();
		if (AttributeName != NAME_None)
		{
			SortedArray.Add(*FindArrayItem(AttributeName));
		}

		for (const FName& Child : Graph[AttributeName].Children)
		{
			--Graph[Child].InDegree;
			if (Graph[Child].InDegree == 0)
			{
				NodeQueue.Add(Child);
			}
		}
	}

	ensureMsgf(Array.Num() == SortedArray.Num(), TEXT("Cycle found in the named attribute DAG"));
	Array = SortedArray;
}

void USkeletonTemplate::SortNamedAttributes()
{
	TFunction<FSkeletonNamedAttribute*(FName)> Functor = [this](FName InAttributeName)
	{
		return FindMutableNamedAttribute(InAttributeName);
	};

	SortByProperty<FSkeletonNamedAttribute>(NamedAttributes, &FSkeletonNamedAttribute::Name, &FSkeletonNamedAttribute::ParentName, Functor);
}