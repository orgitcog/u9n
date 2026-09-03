// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsActions.h"

#include "LearningAgentsManagerListener.h"
#include "LearningAgentsDebug.h"

#include "LearningArray.h"
#include "LearningLog.h"

#include "Containers/StaticArray.h"

bool operator==(const FLearningAgentsActionSchemaElement& Lhs, const FLearningAgentsActionSchemaElement& Rhs)
{
	return Lhs.SchemaElement.Index == Rhs.SchemaElement.Index;
}

bool operator==(const FLearningAgentsActionObjectElement& Lhs, const FLearningAgentsActionObjectElement& Rhs)
{
	return Lhs.ObjectElement.Index == Rhs.ObjectElement.Index;
}

bool operator==(const FLearningAgentsActionModifierElement& Lhs, const FLearningAgentsActionModifierElement& Rhs)
{
	return Lhs.ModifierElement.Index == Rhs.ModifierElement.Index;
}

uint32 GetTypeHash(const FLearningAgentsActionSchemaElement& Element)
{
	return (uint32)Element.SchemaElement.Index;
}

uint32 GetTypeHash(const FLearningAgentsActionObjectElement & Element)
{
	return (uint32)Element.ObjectElement.Index;
}

uint32 GetTypeHash(const FLearningAgentsActionModifierElement& Element)
{
	return (uint32)Element.ModifierElement.Index;
}

namespace UE::Learning::Agents::Action::Private
{
	// We declare this here instead of in LearningArray.h because FName is part of the UObject system and right now the 
	// Learning core module does not have a dependency on the UObject part of UE which is a nice thing to preserve.	
	static inline FString FormatNames(const TLearningArrayView<1, const FName> Array, const int32 MaxItemNum = 16)
	{
		return Array::Format<FName>(Array, [](const FName& Value) { return FString::Printf(TEXT("%s"), *Value.ToString()); }, MaxItemNum);
	}

	static inline bool ContainsDuplicates(const TArrayView<const int32> Indices)
	{
		TSet<int32, DefaultKeyFuncs<int32>, TInlineSetAllocator<32>> IndicesSet;
		IndicesSet.Append(Indices);
		return Indices.Num() != IndicesSet.Num();
	}

	static inline bool ContainsDuplicates(const TArrayView<const FName> ElementNames)
	{
		TSet<FName, DefaultKeyFuncs<FName>, TInlineSetAllocator<32>> ElementNameSet;
		ElementNameSet.Append(ElementNames);
		return ElementNames.Num() != ElementNameSet.Num();
	}

	static inline const TCHAR* GetActionTypeString(const Learning::Action::EType ActionType)
	{
		switch (ActionType)
		{
		case Learning::Action::EType::Null: return TEXT("Null");
		case Learning::Action::EType::Continuous: return TEXT("Continuous");
		case Learning::Action::EType::DiscreteExclusive: return TEXT("DiscreteExclusive");
		case Learning::Action::EType::DiscreteInclusive: return TEXT("DiscreteInclusive");
		case Learning::Action::EType::NamedDiscreteExclusive: return TEXT("NamedDiscreteExclusive");
		case Learning::Action::EType::NamedDiscreteInclusive: return TEXT("NamedDiscreteInclusive");
		case Learning::Action::EType::And: return TEXT("Struct");
		case Learning::Action::EType::OrExclusive: return TEXT("ExclusiveUnion");
		case Learning::Action::EType::OrInclusive: return TEXT("InclusiveUnion");
		case Learning::Action::EType::Array: return TEXT("Array");
		case Learning::Action::EType::Encoding: return TEXT("Encoding");
		default:
			checkNoEntry();
			return TEXT("Unimplemented");
		}
	}

	static bool ValidateActionObjectMatchesSchema(
		const Learning::Action::FSchema& Schema,
		const Learning::Action::FSchemaElement SchemaElement,
		const Learning::Action::FObject& Object,
		const Learning::Action::FObjectElement ObjectElement)
	{
		// Check Elements are Valid

		if (!Schema.IsValid(SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Invalid Action Schema Element."));
			return false;
		}

		if (!Object.IsValid(ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Invalid Action Object Element."));
			return false;
		}

		// Check Names Match

		const FName ActionSchemaElementTag = Schema.GetTag(SchemaElement);
		const FName ActionObjectElementTag = Object.GetTag(ObjectElement);

		if (ActionSchemaElementTag != ActionObjectElementTag)
		{
			UE_LOG(LogLearning, Warning, TEXT("ValidateActionObjectMatchesSchema: Action tag does not match Schema. Expected '%s', got '%s'."),
				*ActionSchemaElementTag.ToString(), *ActionObjectElementTag.ToString());
		}

		// Check Types Match

		const Learning::Action::EType ActionSchemaElementType = Schema.GetType(SchemaElement);
		const Learning::Action::EType ActionObjectElementType = Object.GetType(ObjectElement);

		if (ActionSchemaElementType != ActionObjectElementType)
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' type does not match Schema. Expected type '%s', got type '%s'."),
				*ActionSchemaElementTag.ToString(),
				GetActionTypeString(ActionSchemaElementType),
				GetActionTypeString(ActionObjectElementType));
			return false;
		}

		// Type Specific Checks

		switch (ActionSchemaElementType)
		{
		case Learning::Action::EType::Null: return true;

		case Learning::Action::EType::Continuous:
		{
			const int32 SchemaElementSize = Schema.GetContinuous(SchemaElement).Num;
			const int32 ObjectElementSize = Object.GetContinuous(ObjectElement).Values.Num();

			if (SchemaElementSize != ObjectElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' size does not match Schema. Expected '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaElementSize,
					ObjectElementSize);
				return false;
			}

			return true;
		}

		case Learning::Action::EType::DiscreteExclusive:
		{
			const int32 SchemaElementSize = Schema.GetDiscreteExclusive(SchemaElement).Num;
			const int32 ObjectElementIndex = Object.GetDiscreteExclusive(ObjectElement).DiscreteIndex;

			if (ObjectElementIndex < 0 || ObjectElementIndex >= SchemaElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' index out of range for Schema. Expected '<%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaElementSize,
					ObjectElementIndex);
				return false;
			}

			return true;
		}

		case Learning::Action::EType::DiscreteInclusive:
		{
			const int32 SchemaElementSize = Schema.GetDiscreteInclusive(SchemaElement).Num;
			const TArrayView<const int32> ObjectElementIndices = Object.GetDiscreteInclusive(ObjectElement).DiscreteIndices;

			if (ObjectElementIndices.Num() > SchemaElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' too many indices provided. Expected at most '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaElementSize,
					ObjectElementIndices.Num());
				return false;
			}

			for (int32 SubElementIdx = 0; SubElementIdx < ObjectElementIndices.Num(); SubElementIdx++)
			{
				if (ObjectElementIndices[SubElementIdx] < 0 || ObjectElementIndices[SubElementIdx] >= SchemaElementSize)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' index out of range for Schema. Expected '<%i', got '%i'."),
						*ActionSchemaElementTag.ToString(),
						SchemaElementSize,
						ObjectElementIndices[SubElementIdx]);
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::NamedDiscreteExclusive:
		{
			const Learning::Action::FSchemaNamedDiscreteExclusiveParameters SchemaParameters = Schema.GetNamedDiscreteExclusive(SchemaElement);
			const Learning::Action::FObjectNamedDiscreteExclusiveParameters ObjectParameters = Object.GetNamedDiscreteExclusive(ObjectElement);
			check(SchemaParameters.ElementNames.Num() == SchemaParameters.ElementNames.Num());

			const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementName);

			if (SchemaSubElementIdx == INDEX_NONE)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' Schema does not include '%s' action."),
					*ActionSchemaElementTag.ToString(),
					*ObjectParameters.ElementName.ToString());
				return false;
			}

			return true;
		}

		case Learning::Action::EType::NamedDiscreteInclusive:
		{
			const Learning::Action::FSchemaNamedDiscreteInclusiveParameters SchemaParameters = Schema.GetNamedDiscreteInclusive(SchemaElement);
			const Learning::Action::FObjectNamedDiscreteInclusiveParameters ObjectParameters = Object.GetNamedDiscreteInclusive(ObjectElement);

			if (ObjectParameters.ElementNames.Num() > SchemaParameters.ElementNames.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' too many sub-actions provided. Expected at most '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.ElementNames.Num(),
					ObjectParameters.ElementNames.Num());
				return false;
			}

			for (int32 ObjectSubElementIdx = 0; ObjectSubElementIdx < ObjectParameters.ElementNames.Num(); ObjectSubElementIdx++)
			{
				const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectSubElementIdx]);

				if (SchemaSubElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' Schema does not include '%s' Action."),
						*ActionSchemaElementTag.ToString(),
						*ObjectParameters.ElementNames[ObjectSubElementIdx].ToString());
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::And:
		{
			const Learning::Action::FSchemaAndParameters SchemaParameters = Schema.GetAnd(SchemaElement);
			const Learning::Action::FObjectAndParameters ObjectParameters = Object.GetAnd(ObjectElement);
			check(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());
			check(ObjectParameters.Elements.Num() == ObjectParameters.ElementNames.Num());

			if (SchemaParameters.Elements.Num() != ObjectParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' number of sub-elements does not match Schema. Expected '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.Elements.Num(),
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 ObjectElementIdx = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);

				if (ObjectElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' does not include '%s' action required by Schema."),
						*ActionSchemaElementTag.ToString(),
						*SchemaParameters.ElementNames[SchemaElementIdx].ToString());
					return false;
				}

				if (!ValidateActionObjectMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Object,
					ObjectParameters.Elements[ObjectElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::OrExclusive:
		{
			const Learning::Action::FSchemaOrExclusiveParameters SchemaParameters = Schema.GetOrExclusive(SchemaElement);
			const Learning::Action::FObjectOrExclusiveParameters ObjectParameters = Object.GetOrExclusive(ObjectElement);
			check(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());

			const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementName);

			if (SchemaSubElementIdx == INDEX_NONE)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' Schema does not include '%s' action."),
					*ActionSchemaElementTag.ToString(),
					*ObjectParameters.ElementName.ToString());
				return false;
			}

			return ValidateActionObjectMatchesSchema(
				Schema,
				SchemaParameters.Elements[SchemaSubElementIdx],
				Object,
				ObjectParameters.Element);
		}

		case Learning::Action::EType::OrInclusive:
		{
			const Learning::Action::FSchemaOrInclusiveParameters SchemaParameters = Schema.GetOrInclusive(SchemaElement);
			const Learning::Action::FObjectOrInclusiveParameters ObjectParameters = Object.GetOrInclusive(ObjectElement);

			if (ObjectParameters.Elements.Num() > SchemaParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' too many sub-actions provided. Expected at most '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.Elements.Num(),
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ObjectSubElementIdx = 0; ObjectSubElementIdx < ObjectParameters.Elements.Num(); ObjectSubElementIdx++)
			{
				const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectSubElementIdx]);

				if (SchemaSubElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' Schema does not include '%s' action."),
						*ActionSchemaElementTag.ToString(),
						*ObjectParameters.ElementNames[ObjectSubElementIdx].ToString());
					return false;
				}

				if (!ValidateActionObjectMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaSubElementIdx],
					Object,
					ObjectParameters.Elements[ObjectSubElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::Array:
		{
			const Learning::Action::FSchemaArrayParameters SchemaParameters = Schema.GetArray(SchemaElement);
			const Learning::Action::FObjectArrayParameters ObjectParameters = Object.GetArray(ObjectElement);

			if (ObjectParameters.Elements.Num() != SchemaParameters.Num)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Action '%s' array incorrect size. Expected '%i' elements, got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.Num,
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ElementIdx = 0; ElementIdx < ObjectParameters.Elements.Num(); ElementIdx++)
			{
				if (!ValidateActionObjectMatchesSchema(
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::Encoding:
		{
			const Learning::Action::FSchemaEncodingParameters SchemaParameters = Schema.GetEncoding(SchemaElement);
			const Learning::Action::FObjectEncodingParameters ObjectParameters = Object.GetEncoding(ObjectElement);

			return ValidateActionObjectMatchesSchema(
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Element);
		}

		default:
		{
			checkNoEntry();
			return true;
		}
		}
	}


	static bool ValidateActionModifierMatchesSchema(
		const Learning::Action::FSchema& Schema,
		const Learning::Action::FSchemaElement SchemaElement,
		const Learning::Action::FModifier& Modifier,
		const Learning::Action::FModifierElement ModifierElement)
	{
		// Check Elements are Valid

		if (!Schema.IsValid(SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Invalid Action Schema Element."));
			return false;
		}

		if (!Modifier.IsValid(ModifierElement))
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Invalid Action Modifier Element."));
			return false;
		}

		// Null Modifiers Match Everything

		const Learning::Action::EType ActionSchemaElementType = Schema.GetType(SchemaElement);
		const Learning::Action::EType ActionModifierElementType = Modifier.GetType(ModifierElement);

		if (ActionModifierElementType == Learning::Action::EType::Null)
		{
			return true;
		}

		// Check Names Match

		const FName ActionSchemaElementTag = Schema.GetTag(SchemaElement);
		const FName ActionModifierElementTag = Modifier.GetTag(ModifierElement);

		if (ActionSchemaElementTag != ActionModifierElementTag)
		{
			UE_LOG(LogLearning, Warning, TEXT("ValidateActionModifierMatchesSchema: Action Modifier tag does not match Schema. Expected '%s', got '%s'."),
				*ActionSchemaElementTag.ToString(), *ActionModifierElementTag.ToString());
		}

		// Check Types Match

		if (ActionSchemaElementType != ActionModifierElementType)
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' type does not match Schema. Expected type '%s', got type '%s'."),
				*ActionSchemaElementTag.ToString(),
				GetActionTypeString(ActionSchemaElementType),
				GetActionTypeString(ActionModifierElementType));
			return false;
		}

		// Type Specific Checks

		switch (ActionSchemaElementType)
		{
		case Learning::Action::EType::Null: return true;

		case Learning::Action::EType::Continuous:
		{
			const int32 SchemaElementSize = Schema.GetContinuous(SchemaElement).Num;
			const int32 ModifierMaskedElementSize = Modifier.GetContinuous(ModifierElement).Masked.Num();
			const int32 ModifierValueElementSize = Modifier.GetContinuous(ModifierElement).MaskedValues.Num();

			if (SchemaElementSize != ModifierMaskedElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' size does not match Schema. Expected '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaElementSize,
					ModifierMaskedElementSize);
				return false;
			}

			if (SchemaElementSize != ModifierValueElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' size does not match Schema. Expected '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaElementSize,
					ModifierValueElementSize);
				return false;
			}

			return true;
		}

		case Learning::Action::EType::DiscreteExclusive:
		{
			const int32 SchemaElementSize = Schema.GetDiscreteExclusive(SchemaElement).Num;
			const TArrayView<const int32> ModifierMaskedIndices = Modifier.GetDiscreteExclusive(ModifierElement).MaskedIndices;

			// For exclusive action we must have one less than the schema size or it means all are masked.
			if (ModifierMaskedIndices.Num() > SchemaElementSize - 1)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' too many indices provided. Expected at most '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaElementSize - 1,
					ModifierMaskedIndices.Num());
				return false;
			}

			for (int32 MaskIdx = 0; MaskIdx < ModifierMaskedIndices.Num(); MaskIdx++)
			{
				if (ModifierMaskedIndices[MaskIdx] < 0 || ModifierMaskedIndices[MaskIdx] >= SchemaElementSize)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' masked index out of range for Schema. Expected '<%i', got '%i'."),
						*ActionSchemaElementTag.ToString(),
						SchemaElementSize,
						ModifierMaskedIndices[MaskIdx]);
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::DiscreteInclusive:
		{
			const int32 SchemaElementSize = Schema.GetDiscreteInclusive(SchemaElement).Num;
			const TArrayView<const int32> ModifierMaskedIndices = Modifier.GetDiscreteInclusive(ModifierElement).MaskedIndices;

			if (ModifierMaskedIndices.Num() > SchemaElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' too many indices provided. Expected at most '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaElementSize,
					ModifierMaskedIndices.Num());
				return false;
			}

			for (int32 MaskIdx = 0; MaskIdx < ModifierMaskedIndices.Num(); MaskIdx++)
			{
				if (ModifierMaskedIndices[MaskIdx] < 0 || ModifierMaskedIndices[MaskIdx] >= SchemaElementSize)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' masked index out of range for Schema. Expected '<%i', got '%i'."),
						*ActionSchemaElementTag.ToString(),
						SchemaElementSize,
						ModifierMaskedIndices[MaskIdx]);
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::NamedDiscreteExclusive:
		{
			const TArrayView<const FName> SchemaNames = Schema.GetNamedDiscreteExclusive(SchemaElement).ElementNames;
			const TArrayView<const FName> ModifierMaskedNames = Modifier.GetNamedDiscreteExclusive(ModifierElement).MaskedElementNames;

			// For exclusive action we must have one less than the schema size or it means all are masked.
			if (ModifierMaskedNames.Num() > SchemaNames.Num() - 1)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' too many indices provided. Expected at most '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaNames.Num() - 1,
					ModifierMaskedNames.Num());
				return false;
			}

			for (int32 MaskIdx = 0; MaskIdx < ModifierMaskedNames.Num(); MaskIdx++)
			{
				if (!SchemaNames.Contains(ModifierMaskedNames[MaskIdx]))
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' name '%s' not found in Schema."),
						*ActionSchemaElementTag.ToString(),
						*ModifierMaskedNames[MaskIdx].ToString());
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::NamedDiscreteInclusive:
		{
			const TArrayView<const FName> SchemaNames = Schema.GetNamedDiscreteInclusive(SchemaElement).ElementNames;
			const TArrayView<const FName> ModifierMaskedNames = Modifier.GetNamedDiscreteInclusive(ModifierElement).MaskedElementNames;

			for (int32 MaskIdx = 0; MaskIdx < ModifierMaskedNames.Num(); MaskIdx++)
			{
				if (!SchemaNames.Contains(ModifierMaskedNames[MaskIdx]))
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' name '%s' not found in Schema."),
						*ActionSchemaElementTag.ToString(),
						*ModifierMaskedNames[MaskIdx].ToString());
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::And:
		{
			const Learning::Action::FSchemaAndParameters SchemaParameters = Schema.GetAnd(SchemaElement);
			const Learning::Action::FModifierAndParameters ModifierParameters = Modifier.GetAnd(ModifierElement);
			check(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());
			check(ModifierParameters.Elements.Num() == ModifierParameters.ElementNames.Num());

			if (SchemaParameters.Elements.Num() < ModifierParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' too many sub-elements. Expected '<=%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.Elements.Num(),
					ModifierParameters.Elements.Num());
				return false;
			}

			for (int32 ModifierElementIdx = 0; ModifierElementIdx < ModifierParameters.Elements.Num(); ModifierElementIdx++)
			{
				const int32 SchemaElementIdx = SchemaParameters.ElementNames.Find(ModifierParameters.ElementNames[ModifierElementIdx]);

				if (SchemaElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action '%s' does not include '%s' action given by Modifier."),
						*ActionSchemaElementTag.ToString(),
						*ModifierParameters.ElementNames[ModifierElementIdx].ToString());
					return false;
				}

				if (!ValidateActionModifierMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Modifier,
					ModifierParameters.Elements[ModifierElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::OrExclusive:
		{
			const Learning::Action::FSchemaOrExclusiveParameters SchemaParameters = Schema.GetOrExclusive(SchemaElement);
			const Learning::Action::FModifierOrExclusiveParameters ModifierParameters = Modifier.GetOrExclusive(ModifierElement);
			check(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());

			// For exclusive action we must have one less than the schema size or it means all are masked.
			if (ModifierParameters.MaskedElements.Num() > SchemaParameters.Elements.Num() - 1)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' too many masked elements. Expected at most '%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.Elements.Num() - 1,
					ModifierParameters.MaskedElements.Num());
				return false;
			}

			for (int32 ModifierMaskIdx = 0; ModifierMaskIdx < ModifierParameters.MaskedElements.Num(); ModifierMaskIdx++)
			{
				const FName MaskedElement = ModifierParameters.MaskedElements[ModifierMaskIdx];

				if (SchemaParameters.ElementNames.Find(MaskedElement) == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action '%s' does not include '%s' action given by Modifier."),
						*ActionSchemaElementTag.ToString(),
						*MaskedElement.ToString());
					return false;
				}
			}

			// TODO: Check Mask does not contain all elements

			if (SchemaParameters.Elements.Num() < ModifierParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' too many sub-elements. Expected '<=%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.Elements.Num(),
					ModifierParameters.Elements.Num());
				return false;
			}

			for (int32 ModifierElementIdx = 0; ModifierElementIdx < ModifierParameters.Elements.Num(); ModifierElementIdx++)
			{
				const int32 SchemaElementIdx = SchemaParameters.ElementNames.Find(ModifierParameters.ElementNames[ModifierElementIdx]);

				if (SchemaElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action '%s' does not include '%s' action given by Modifier."),
						*ActionSchemaElementTag.ToString(),
						*ModifierParameters.ElementNames[ModifierElementIdx].ToString());
					return false;
				}

				if (!ValidateActionModifierMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Modifier,
					ModifierParameters.Elements[ModifierElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::OrInclusive:
		{
			const Learning::Action::FSchemaOrInclusiveParameters SchemaParameters = Schema.GetOrInclusive(SchemaElement);
			const Learning::Action::FModifierOrInclusiveParameters ModifierParameters = Modifier.GetOrInclusive(ModifierElement);

			for (int32 ModifierMaskIdx = 0; ModifierMaskIdx < ModifierParameters.MaskedElements.Num(); ModifierMaskIdx++)
			{
				const FName MaskedElement = ModifierParameters.MaskedElements[ModifierMaskIdx];

				if (SchemaParameters.ElementNames.Find(MaskedElement) == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action '%s' does not include '%s' action given by Modifier."),
						*ActionSchemaElementTag.ToString(),
						*MaskedElement.ToString());
					return false;
				}
			}

			if (SchemaParameters.Elements.Num() < ModifierParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action Modifier '%s' too many sub-elements. Expected '<=%i', got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.Elements.Num(),
					ModifierParameters.Elements.Num());
				return false;
			}

			for (int32 ModifierElementIdx = 0; ModifierElementIdx < ModifierParameters.Elements.Num(); ModifierElementIdx++)
			{
				const int32 SchemaElementIdx = SchemaParameters.ElementNames.Find(ModifierParameters.ElementNames[ModifierElementIdx]);

				if (SchemaElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action '%s' does not include '%s' action given by Modifier."),
						*ActionSchemaElementTag.ToString(),
						*ModifierParameters.ElementNames[ModifierElementIdx].ToString());
					return false;
				}

				if (!ValidateActionModifierMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Modifier,
					ModifierParameters.Elements[ModifierElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::Array:
		{
			const Learning::Action::FSchemaArrayParameters SchemaParameters = Schema.GetArray(SchemaElement);
			const Learning::Action::FModifierArrayParameters ModifierParameters = Modifier.GetArray(ModifierElement);

			if (ModifierParameters.Elements.Num() != SchemaParameters.Num)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Action '%s' array incorrect size. Expected '%i' elements, got '%i'."),
					*ActionSchemaElementTag.ToString(),
					SchemaParameters.Num,
					ModifierParameters.Elements.Num());
				return false;
			}

			for (int32 ElementIdx = 0; ElementIdx < ModifierParameters.Elements.Num(); ElementIdx++)
			{
				if (!ValidateActionModifierMatchesSchema(
					Schema,
					SchemaParameters.Element,
					Modifier,
					ModifierParameters.Elements[ElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::Encoding:
		{
			const Learning::Action::FSchemaEncodingParameters SchemaParameters = Schema.GetEncoding(SchemaElement);
			const Learning::Action::FModifierEncodingParameters ModifierParameters = Modifier.GetEncoding(ModifierElement);

			return ValidateActionModifierMatchesSchema(
				Schema,
				SchemaParameters.Element,
				Modifier,
				ModifierParameters.Element);
		}

		default:
		{
			checkNoEntry();
			return false;
		}
		}
	}

	static void LogAction(
		const UE::Learning::Action::FObject& Object,
		const UE::Learning::Action::FObjectElement ObjectElement,
		const FString& Indentation,
		const FString& Prefix)
	{
		if (!Object.IsValid(ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("LogAction: Invalid Action Object Element."));
			return;
		}

		const UE::Learning::Action::EType Type = Object.GetType(ObjectElement);
		const FName Tag = Object.GetTag(ObjectElement);

		switch (Type)
		{
		case UE::Learning::Action::EType::Null:
		{
			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type));
			return;
		}

		case UE::Learning::Action::EType::Continuous:
		{
			const UE::Learning::Action::FObjectContinuousParameters Parameters = Object.GetContinuous(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %s"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type), *UE::Learning::Array::FormatFloat(Parameters.Values));
			return;
		}

		case UE::Learning::Action::EType::DiscreteExclusive:
		{
			const UE::Learning::Action::FObjectDiscreteExclusiveParameters Parameters = Object.GetDiscreteExclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %i"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type), Parameters.DiscreteIndex);
			return;
		}

		case UE::Learning::Action::EType::DiscreteInclusive:
		{
			const UE::Learning::Action::FObjectDiscreteInclusiveParameters Parameters = Object.GetDiscreteInclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %s"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type), *UE::Learning::Array::FormatInt32(Parameters.DiscreteIndices));
			return;
		}

		case UE::Learning::Action::EType::NamedDiscreteExclusive:
		{
			const UE::Learning::Action::FObjectNamedDiscreteExclusiveParameters Parameters = Object.GetNamedDiscreteExclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %s"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type), *Parameters.ElementName.ToString());
			return;
		}

		case UE::Learning::Action::EType::NamedDiscreteInclusive:
		{
			const UE::Learning::Action::FObjectNamedDiscreteInclusiveParameters Parameters = Object.GetNamedDiscreteInclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %s"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type), *UE::Learning::Agents::Action::Private::FormatNames(Parameters.ElementNames));
			return;
		}

		case UE::Learning::Action::EType::And:
		{
			const UE::Learning::Action::FObjectAndParameters Parameters = Object.GetAnd(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogAction(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementNames[SubElementIdx].ToString()));
			}

			return;
		}

		case UE::Learning::Action::EType::OrExclusive:
		{
			const UE::Learning::Action::FObjectOrExclusiveParameters Parameters = Object.GetOrExclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type));
			LogAction(Object, Parameters.Element, *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementName.ToString()));

			return;
		}

		case UE::Learning::Action::EType::OrInclusive:
		{
			const UE::Learning::Action::FObjectOrInclusiveParameters Parameters = Object.GetOrInclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogAction(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementNames[SubElementIdx].ToString()));
			}

			return;
		}

		case UE::Learning::Action::EType::Array:
		{
			const UE::Learning::Action::FObjectArrayParameters Parameters = Object.GetArray(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogAction(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| %3i:"), SubElementIdx));
			}

			return;
		}

		case UE::Learning::Action::EType::Encoding:
		{
			const UE::Learning::Action::FObjectEncodingParameters Parameters = Object.GetEncoding(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetActionTypeString(Type));
			LogAction(Object, Parameters.Element, *(Indentation + TEXT("    ")), TEXT("|"));

			return;
		}
		}
	}

	static inline FVector VectorLogSafe(const FVector V, const float Epsilon = UE_SMALL_NUMBER)
	{
		return FVector(
			FMath::Loge(FMath::Max(V.X, Epsilon)),
			FMath::Loge(FMath::Max(V.Y, Epsilon)),
			FMath::Loge(FMath::Max(V.Z, Epsilon)));
	}

	static inline FVector VectorExp(const FVector V)
	{
		return FVector(
			FMath::Exp(V.X),
			FMath::Exp(V.Y),
			FMath::Exp(V.Z));
	}

	static inline void NormalizeProbabilitiesExclusive(TArrayView<float> PriorProbabilities)
	{
		float Total = 0.0f;
		for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
		{
			if (PriorProbabilities[Idx] < 0.0f || PriorProbabilities[Idx] > 1.0f)
			{
				UE_LOG(LogLearning, Warning, TEXT("NormalizeProbabilitiesExclusive: Invalid Prior Probability Given (%f), must be in range 0 to 1."), PriorProbabilities[Idx]);
			}

			PriorProbabilities[Idx] = FMath::Clamp(PriorProbabilities[Idx], 0.0f, 1.0f);
			Total += PriorProbabilities[Idx];
		}

		if (PriorProbabilities.Num() > 0 && FMath::Abs(Total) < UE_SMALL_NUMBER)
		{
			UE_LOG(LogLearning, Warning, TEXT("NormalizeProbabilitiesExclusive: Prior Probabilities are too small. Should sum to 1."));

			for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
			{
				PriorProbabilities[Idx] = 1.0f / PriorProbabilities.Num();
			}
		}
		else
		{
			for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
			{
				PriorProbabilities[Idx] /= Total;
			}
		}
	}

	static inline void NormalizeProbabilitiesInclusive(TArrayView<float> PriorProbabilities)
	{
		for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
		{
			if (PriorProbabilities[Idx] < 0.0f || PriorProbabilities[Idx] > 1.0f)
			{
				UE_LOG(LogLearning, Warning, TEXT("NormalizeProbabilitiesInclusive: Invalid Prior Probability Given (%f), must be in range 0 to 1."), PriorProbabilities[Idx]);
			}

			PriorProbabilities[Idx] = FMath::Clamp(PriorProbabilities[Idx], 0.0f, 1.0f);
		}
	}

	static inline Learning::Action::EEncodingActivationFunction GetEncodingActivationFunction(const ELearningAgentsActivationFunction ActivationFunction)
	{
		switch (ActivationFunction)
		{
		case ELearningAgentsActivationFunction::ReLU: return Learning::Action::EEncodingActivationFunction::ReLU;
		case ELearningAgentsActivationFunction::ELU: return Learning::Action::EEncodingActivationFunction::ELU;
		case ELearningAgentsActivationFunction::TanH: return Learning::Action::EEncodingActivationFunction::TanH;
		default: checkNoEntry(); return Learning::Action::EEncodingActivationFunction::ReLU;
		}
	}
}

bool ULearningAgentsActions::ValidateActionObjectMatchesSchema(
	const ULearningAgentsActionSchema* Schema,
	const FLearningAgentsActionSchemaElement SchemaElement,
	const ULearningAgentsActionObject* Object,
	const FLearningAgentsActionObjectElement ObjectElement)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Schema is nullptr."));
		return false;
	}

	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("ValidateActionObjectMatchesSchema: Object is nullptr."));
		return false;
	}

	return UE::Learning::Agents::Action::Private::ValidateActionObjectMatchesSchema(
		Schema->ActionSchema,
		SchemaElement.SchemaElement,
		Object->ActionObject,
		ObjectElement.ObjectElement);
}

bool ULearningAgentsActions::ValidateActionModifierMatchesSchema(
	const ULearningAgentsActionSchema* Schema,
	const FLearningAgentsActionSchemaElement SchemaElement,
	const ULearningAgentsActionModifier* Modifier,
	const FLearningAgentsActionModifierElement ModifierElement)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Schema is nullptr."));
		return false;
	}

	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("ValidateActionModifierMatchesSchema: Modifier is nullptr."));
		return false;
	}

	return UE::Learning::Agents::Action::Private::ValidateActionModifierMatchesSchema(
		Schema->ActionSchema,
		SchemaElement.SchemaElement,
		Modifier->ActionModifier,
		ModifierElement.ModifierElement);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyNullAction(ULearningAgentsActionSchema* Schema, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyNullAction: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	return { Schema->ActionSchema.CreateNull(Tag) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyContinuousAction(ULearningAgentsActionSchema* Schema, const int32 Size, const float Scale, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyContinuousAction: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Size < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyContinuousAction: Invalid Continuous Action Size '%i'."), Size);
		return FLearningAgentsActionSchemaElement();
	}

	if (Scale < UE_SMALL_NUMBER)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyContinuousAction: Invalid Scale for Continuous Action '%f', must be greater than '%f'."), Scale, UE_SMALL_NUMBER);
		return FLearningAgentsActionSchemaElement();
	}

	if (Size == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyContinuousAction: Specifying zero-sized Continuous Action."));
	}

	return { Schema->ActionSchema.CreateContinuous({ Size, Scale }, Tag) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyExclusiveDiscreteAction(ULearningAgentsActionSchema* Schema, const int32 Size, const TArray<float>& PriorProbabilities, const FName Tag)
{
	return SpecifyExclusiveDiscreteActionFromArrayView(Schema, Size, PriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyExclusiveDiscreteActionFromArrayView(ULearningAgentsActionSchema* Schema, const int32 Size, const TArrayView<const float> PriorProbabilities, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveDiscreteActionFromArrayView: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Size < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveDiscreteActionFromArrayView: Invalid DiscreteExclusive Action Size '%i'."), Size);
		return FLearningAgentsActionSchemaElement();
	}

	if (Size == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyExclusiveDiscreteActionFromArrayView: Specifying zero-sized Exclusive Discrete Action."));
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(1.0f / Size, Size);
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesExclusive(NormalizedPriorProbabilities);

	return { Schema->ActionSchema.CreateDiscreteExclusive({ Size, MakeArrayView(NormalizedPriorProbabilities) }, Tag)};
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyNamedExclusiveDiscreteAction(ULearningAgentsActionSchema* Schema, const TArray<FName>& Names, const TMap<FName, float>& PriorProbabilities, const FName Tag)
{
	if (Names.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyNamedExclusiveDiscreteAction: Specifying zero-sized Named Exclusive Discrete Action."));
	}

	const int32 NameNum = Names.Num();

	TArray<int32, TInlineAllocator<16>> NameIndices;
	TArray<float, TInlineAllocator<16>> NamePriorProbabilities;
	NameIndices.Empty(NameNum);
	NamePriorProbabilities.Empty(NameNum);

	int32 Index = 0;
	for (const FName& Name : Names)
	{
		NameIndices.Add(Index);
		const float* PriorProb = PriorProbabilities.Find(Name);
		NamePriorProbabilities.Add(PriorProb ? *PriorProb : 1.0f / NameNum);
		Index++;
	}

	// Sort Names According to FName

	NameIndices.Sort([Names](const int32 Lhs, const int32 Rhs)
		{
			return Names[Lhs].ToString().ToLower() < Names[Rhs].ToString().ToLower();
		});

	TArray<FName, TInlineAllocator<16>> SortedNames;
	TArray<float, TInlineAllocator<16>> SortedPriorProbabilities;
	SortedNames.SetNumUninitialized(NameNum);
	SortedPriorProbabilities.SetNumUninitialized(NameNum);
	for (int32 Idx = 0; Idx < NameNum; Idx++)
	{
		SortedNames[Idx] = Names[NameIndices[Idx]];
		SortedPriorProbabilities[Idx] = NamePriorProbabilities[NameIndices[Idx]];
	}

	return SpecifyNamedExclusiveDiscreteActionFromArrayViews(Schema, SortedNames, SortedPriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyNamedExclusiveDiscreteActionFromArrayViews(ULearningAgentsActionSchema* Schema, const TArrayView<const FName> Names, const TArrayView<const float> PriorProbabilities, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyNamedExclusiveDiscreteActionFromArrayViews: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Names.Num() == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyNamedExclusiveDiscreteActionFromArrayViews: Specifying zero-sized Named Exclusive Discrete Action."));
		return FLearningAgentsActionSchemaElement();
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(1.0f / Names.Num(), Names.Num());
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesExclusive(NormalizedPriorProbabilities);

	return { Schema->ActionSchema.CreateNamedDiscreteExclusive({ Names, MakeArrayView(NormalizedPriorProbabilities) }, Tag) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyInclusiveDiscreteAction(ULearningAgentsActionSchema* Schema, const int32 Size, const TArray<float>& PriorProbabilities, const FName Tag)
{
	return SpecifyInclusiveDiscreteActionFromArrayView(Schema, Size, PriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyInclusiveDiscreteActionFromArrayView(ULearningAgentsActionSchema* Schema, const int32 Size, const TArrayView<const float> PriorProbabilities, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveDiscreteActionFromArrayView: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Size < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveDiscreteActionFromArrayView: Invalid DiscreteInclusive Action Size '%i'."), Size);
		return FLearningAgentsActionSchemaElement();
	}

	if (Size == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyInclusiveDiscreteActionFromArrayView: Specifying zero-sized Inclusive Discrete Action."));
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(0.5f, Size);
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesInclusive(NormalizedPriorProbabilities);

	return { Schema->ActionSchema.CreateDiscreteInclusive({ Size, MakeArrayView(NormalizedPriorProbabilities) }, Tag) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyNamedInclusiveDiscreteAction(ULearningAgentsActionSchema* Schema, const TArray<FName> Names, const TMap<FName, float>& PriorProbabilities, const FName Tag)
{
	if (Names.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyNamedInclusiveDiscreteAction: Specifying zero-sized Named Inclusive Discrete Action."));
	}

	const int32 NameNum = Names.Num();

	TArray<int32, TInlineAllocator<16>> NameIndices;
	TArray<float, TInlineAllocator<16>> NamePriorProbabilities;
	NameIndices.Empty(NameNum);
	NamePriorProbabilities.Empty(NameNum);

	int32 Index = 0;
	for (const FName& Name : Names)
	{
		NameIndices.Add(Index);
		const float* PriorProb = PriorProbabilities.Find(Name);
		NamePriorProbabilities.Add(PriorProb ? *PriorProb : 1.0f / NameNum);
		Index++;
	}

	// Sort Names According to FName

	NameIndices.Sort([Names](const int32 Lhs, const int32 Rhs)
		{
			return Names[Lhs].ToString().ToLower() < Names[Rhs].ToString().ToLower();
		});

	TArray<FName, TInlineAllocator<16>> SortedNames;
	TArray<float, TInlineAllocator<16>> SortedPriorProbabilities;
	SortedNames.SetNumUninitialized(NameNum);
	SortedPriorProbabilities.SetNumUninitialized(NameNum);
	for (int32 Idx = 0; Idx < NameNum; Idx++)
	{
		SortedNames[Idx] = Names[NameIndices[Idx]];
		SortedPriorProbabilities[Idx] = NamePriorProbabilities[NameIndices[Idx]];
	}

	return SpecifyNamedInclusiveDiscreteActionFromArrayViews(Schema, SortedNames, SortedPriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyNamedInclusiveDiscreteActionFromArrayViews(ULearningAgentsActionSchema* Schema, const TArrayView<const FName> Names, const TArrayView<const float> PriorProbabilities, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyNamedInclusiveDiscreteActionFromArrayViews: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Names.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyNamedInclusiveDiscreteActionFromArrayViews: Specifying zero-sized Named Inclusive Discrete Action."));
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(1.0f / Names.Num(), Names.Num());
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesInclusive(NormalizedPriorProbabilities);

	return { Schema->ActionSchema.CreateNamedDiscreteInclusive({ Names, MakeArrayView(NormalizedPriorProbabilities) }, Tag) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyStructAction(ULearningAgentsActionSchema* Schema, const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const FName Tag)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyStructAction: Specifying zero-sized Struct Action."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SubElements;
	SubElementIndices.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsActionSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SortedSubElements;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
	}

	return SpecifyStructActionFromArrayViews(Schema, SortedSubElementNames, SortedSubElements, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyStructActionFromArrays(ULearningAgentsActionSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const FName Tag)
{
	return SpecifyStructActionFromArrayViews(Schema, ElementNames, Elements, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyStructActionFromArrayViews(ULearningAgentsActionSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStructActionFromArrayViews: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyStructActionFromArrayViews: Specifying zero-sized Struct Action."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStructActionFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionSchemaElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStructActionFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsActionSchemaElement();
	}

	TArray<UE::Learning::Action::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionSchemaElement& Element : Elements)
	{
		if (!Schema->ActionSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("SpecifyStructActionFromArrayViews: Invalid Action Object."));
			return FLearningAgentsActionSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	return { Schema->ActionSchema.CreateAnd({ ElementNames, SubElements }, Tag) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyExclusiveUnionAction(ULearningAgentsActionSchema* Schema, const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const TMap<FName, float>& PriorProbabilities, const FName Tag)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyExclusiveUnionAction: Specifying zero-sized Exclusive Union Action."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SubElements;
	TArray<float, TInlineAllocator<16>> SubElementPriorProbabilities;
	SubElementIndices.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);
	SubElementPriorProbabilities.Empty(SubElementNum);

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsActionSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		const float* PriorProb = PriorProbabilities.Find(Element.Key);
		SubElementPriorProbabilities.Add(PriorProb ? *PriorProb : 1.0f / SubElementNum);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SortedSubElements;
	TArray<float, TInlineAllocator<16>> SortedSubElementPriorProbabilities;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	SortedSubElementPriorProbabilities.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
		SortedSubElementPriorProbabilities[Idx] = SubElementPriorProbabilities[SubElementIndices[Idx]];
	}

	return SpecifyExclusiveUnionActionFromArrayViews(Schema, SortedSubElementNames, SortedSubElements, SortedSubElementPriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyExclusiveUnionActionFromArrays(ULearningAgentsActionSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const TArray<float>& PriorProbabilities, const FName Tag)
{
	return SpecifyExclusiveUnionActionFromArrayViews(Schema, ElementNames, Elements, PriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyExclusiveUnionActionFromArrayViews(ULearningAgentsActionSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const TArrayView<const float> PriorProbabilities, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionActionFromArrayViews: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyExclusiveUnionActionFromArrayViews: Specifying zero-sized Exclusive Union Action."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionActionFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionSchemaElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionActionFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsActionSchemaElement();
	}

	TArray<UE::Learning::Action::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionSchemaElement& Element : Elements)
	{
		if (!Schema->ActionSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionActionFromArrayViews: Invalid Action Object."));
			return FLearningAgentsActionSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(1.0f / Elements.Num(), Elements.Num());
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesExclusive(NormalizedPriorProbabilities);

	return { Schema->ActionSchema.CreateOrExclusive({ ElementNames, SubElements, MakeArrayView(NormalizedPriorProbabilities) }, Tag)};
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyInclusiveUnionAction(ULearningAgentsActionSchema* Schema, const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const TMap<FName, float>& PriorProbabilities, const FName Tag)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyInclusiveUnionAction: Specifying zero-sized Inclusive Union Action."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SubElements;
	TArray<float, TInlineAllocator<16>> SubElementPriorProbabilities;
	SubElementIndices.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);
	SubElementPriorProbabilities.Empty(SubElementNum);

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsActionSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		const float* PriorProb = PriorProbabilities.Find(Element.Key);
		SubElementPriorProbabilities.Add(PriorProb ? *PriorProb : 1.0f / SubElementNum);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SortedSubElements;
	TArray<float, TInlineAllocator<16>> SortedSubElementPriorProbabilities;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	SortedSubElementPriorProbabilities.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
		SortedSubElementPriorProbabilities[Idx] = SubElementPriorProbabilities[SubElementIndices[Idx]];
	}

	return SpecifyInclusiveUnionActionFromArrayViews(Schema, SortedSubElementNames, SortedSubElements, SortedSubElementPriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyInclusiveUnionActionFromArrays(ULearningAgentsActionSchema* Schema, const TArray<FName> ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const TArray<float>& PriorProbabilities, const FName Tag)
{
	return SpecifyInclusiveUnionActionFromArrayViews(Schema, ElementNames, Elements, PriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyInclusiveUnionActionFromArrayViews(ULearningAgentsActionSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const TArrayView<const float> PriorProbabilities, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionActionFromArrayViews: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyInclusiveUnionActionFromArrayViews: Specifying zero-sized Inclusive Union Action."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionActionFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionSchemaElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionActionFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsActionSchemaElement();
	}

	TArray<UE::Learning::Action::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionSchemaElement& Element : Elements)
	{
		if (!Schema->ActionSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionActionFromArrayViews: Invalid Action Object."));
			return FLearningAgentsActionSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(0.5f, Elements.Num());
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesInclusive(NormalizedPriorProbabilities);

	return { Schema->ActionSchema.CreateOrInclusive({ ElementNames, SubElements, MakeArrayView(NormalizedPriorProbabilities) }, Tag)};
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyStaticArrayAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement Element, const int32 Num, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStaticArrayAction: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Num < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStaticArrayAction: Invalid Action Static Array Num %i."), Num);
		return FLearningAgentsActionSchemaElement();
	}

	if (Num == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyStaticArrayAction: Specifying zero-sized Static Array Action."));
	}

	if (!Schema->ActionSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStaticArrayAction: Invalid Action Object."));
		return FLearningAgentsActionSchemaElement();
	}

	return { Schema->ActionSchema.CreateArray({ Element.SchemaElement, Num }, Tag) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyPairAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement Key, const FLearningAgentsActionSchemaElement Value, const FName Tag)
{
	return SpecifyStructActionFromArrayViews(Schema, { TEXT("Key"), TEXT("Value") }, { Key, Value }, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyEnumAction(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TMap<uint8, float>& PriorProbabilities, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEnumAction: Enum is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	TArray<float, TInlineAllocator<16>> PriorProbabilitiesArray;
	PriorProbabilitiesArray.Init(1.0f / (Enum->NumEnums() - 1), Enum->NumEnums() - 1);
	for (const TPair<uint8, float> Prior : PriorProbabilities)
	{
		const int32 EnumIndex = Enum->GetIndexByValue(Prior.Key);
		if (EnumIndex != INDEX_NONE)
		{
			PriorProbabilitiesArray[EnumIndex] = Prior.Value;
		}
	}

	return SpecifyEnumActionFromArrayView(Schema, Enum, PriorProbabilitiesArray, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyEnumActionFromArray(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TArray<float>& PriorProbabilities, const FName Tag)
{
	return SpecifyEnumActionFromArrayView(Schema, Enum, PriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyEnumActionFromArrayView(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TArrayView<const float> PriorProbabilities, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEnumActionFromArrayView: Enum is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	return SpecifyExclusiveDiscreteActionFromArrayView(Schema, Enum->NumEnums() - 1, PriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyBitmaskAction(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TMap<uint8, float>& PriorProbabilities, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyBitmaskAction: Enum is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyBitmaskAction: Too many values in Enum to use as Bitmask (%i)."), Enum->NumEnums() - 1);
		return FLearningAgentsActionSchemaElement();
	}

	TArray<float, TInlineAllocator<16>> PriorProbabilitiesArray;
	PriorProbabilitiesArray.Init(0.5f, Enum->NumEnums() - 1);
	for (const TPair<uint8, float> Prior : PriorProbabilities)
	{
		const int32 EnumIndex = Enum->GetIndexByValue(Prior.Key);
		if (EnumIndex != INDEX_NONE)
		{
			PriorProbabilitiesArray[EnumIndex] = Prior.Value;
		}
	}

	return SpecifyBitmaskActionFromArrayView(Schema, Enum, PriorProbabilitiesArray, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyBitmaskActionFromArray(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TArray<float>& PriorProbabilities, const FName Tag)
{
	return SpecifyBitmaskActionFromArrayView(Schema, Enum, PriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyBitmaskActionFromArrayView(ULearningAgentsActionSchema* Schema, const UEnum* Enum, const TArrayView<const float> PriorProbabilities, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyBitmaskActionFromArrayView: Enum is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyBitmaskActionFromArrayView: Too many values in Enum to use as Bitmask (%i)."), Enum->NumEnums() - 1);
		return FLearningAgentsActionSchemaElement();
	}

	return SpecifyInclusiveDiscreteActionFromArrayView(Schema, Enum->NumEnums() - 1, PriorProbabilities, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyOptionalAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement Element, const float PriorProbability, const FName Tag)
{
	return SpecifyExclusiveUnionActionFromArrayViews(Schema, { TEXT("Null"), TEXT("Valid") }, { SpecifyNullAction(Schema), Element }, { 1.0f - PriorProbability, PriorProbability }, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyEitherAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement A, const FLearningAgentsActionSchemaElement B, const float PriorProbabilityOfA, const FName Tag)
{
	return SpecifyExclusiveUnionActionFromArrayViews(Schema, { TEXT("A"), TEXT("B") }, { A, B }, { 1.0f - PriorProbabilityOfA, PriorProbabilityOfA }, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyEncodingAction(ULearningAgentsActionSchema* Schema, const FLearningAgentsActionSchemaElement Element, const int32 EncodingSize, const int32 HiddenLayerNum, const ELearningAgentsActivationFunction ActivationFunction, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEncodingAction: Schema is nullptr."));
		return FLearningAgentsActionSchemaElement();
	}

	if (EncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEncodingAction: Invalid Action EncodingSize '%i' - must be greater than zero."), EncodingSize);
		return FLearningAgentsActionSchemaElement();
	}

	if (HiddenLayerNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEncodingAction: Invalid Action HiddenLayerNum '%i' - must be greater than zero."), HiddenLayerNum);
		return FLearningAgentsActionSchemaElement();
	}

	if (!Schema->ActionSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEncodingAction: Invalid Action Object."));
		return FLearningAgentsActionSchemaElement();
	}

	return { Schema->ActionSchema.CreateEncoding({ Element.SchemaElement, EncodingSize, HiddenLayerNum, UE::Learning::Agents::Action::Private::GetEncodingActivationFunction(ActivationFunction) }, Tag)};
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyBoolAction(ULearningAgentsActionSchema* Schema, const float PriorProbability, const FName Tag)
{
	return SpecifyExclusiveDiscreteActionFromArrayView(Schema, 2, { 1.0f - PriorProbability, PriorProbability }, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyFloatAction(ULearningAgentsActionSchema* Schema, const float FloatScale, const FName Tag)
{
	return SpecifyContinuousAction(Schema, 1, FloatScale, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyLocationAction(ULearningAgentsActionSchema* Schema, const float LocationScale, const FName Tag)
{
	return SpecifyContinuousAction(Schema, 3, LocationScale, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyRotationAction(ULearningAgentsActionSchema* Schema, const float RotationScale, const FName Tag)
{
	return SpecifyContinuousAction(Schema, 3, RotationScale, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyScaleAction(ULearningAgentsActionSchema* Schema, const float ScaleScale, const FName Tag)
{
	return SpecifyContinuousAction(Schema, 3, ScaleScale, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyTransformAction(
	ULearningAgentsActionSchema* Schema, 
	const float LocationScale ,
	const float RotationScale,
	const float ScaleScale,
	const FName Tag)
{
	return SpecifyStructActionFromArrayViews(Schema,
		{
			TEXT("Location"),
			TEXT("Rotation"),
			TEXT("Scale")
		},
		{
			SpecifyLocationAction(Schema, LocationScale),
			SpecifyRotationAction(Schema, RotationScale),
			SpecifyScaleAction(Schema, ScaleScale)
		}, 
		Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyAngleAction(ULearningAgentsActionSchema* Schema, const float AngleScale, const FName Tag)
{
	return SpecifyContinuousAction(Schema, 1, AngleScale, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyVelocityAction(ULearningAgentsActionSchema* Schema, const float VelocityScale, const FName Tag)
{
	return SpecifyContinuousAction(Schema, 3, VelocityScale, Tag);
}

FLearningAgentsActionSchemaElement ULearningAgentsActions::SpecifyDirectionAction(ULearningAgentsActionSchema* Schema, const FName Tag)
{
	return SpecifyContinuousAction(Schema, 3, 1.0f, Tag);
}

void ULearningAgentsActions::LogAction(const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("LogAction: Object is nullptr."));
		return;
	}

	UE::Learning::Agents::Action::Private::LogAction(Object->ActionObject, Element.ObjectElement, TEXT(""), TEXT(""));
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeNullAction(ULearningAgentsActionObject* Object, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNullAction: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	return { Object->ActionObject.CreateNull(Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeContinuousAction(
	ULearningAgentsActionObject* Object,
	const TArray<float>& Values,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	return MakeContinuousActionFromArrayView(Object, Values, Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerLocation, VisualLoggerColor);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeContinuousActionFromArrayView(
	ULearningAgentsActionObject* Object,
	const TArrayView<const float> Values,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeContinuousActionFromArrayView: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (Values.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeContinuousActionFromArrayView: Creating zero-sized Continuous Action."));
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEncoded: %s\nValues: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*UE::Learning::Array::FormatFloat(Values),
			*UE::Learning::Array::FormatFloat(Values)); // Encoded is identical to provided values
	}
#endif

	return { Object->ActionObject.CreateContinuous({ Values }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeExclusiveDiscreteAction(
	ULearningAgentsActionObject* Object,
	const int32 Index,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveDiscreteAction: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (Index < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveDiscreteAction: Invalid Action Index %i."), Index);
		return FLearningAgentsActionObjectElement();
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nIndex: [%i]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Index);
	}
#endif

	return { Object->ActionObject.CreateDiscreteExclusive({ Index }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeNamedExclusiveDiscreteAction(
	ULearningAgentsActionObject* Object,
	const FName Name,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNamedExclusiveDiscreteAction: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nName: [%s]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*Name.ToString());
	}
#endif

	return { Object->ActionObject.CreateNamedDiscreteExclusive({ Name }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeInclusiveDiscreteAction(
	ULearningAgentsActionObject* Object,
	const TArray<int32>& Indices,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	return MakeInclusiveDiscreteActionFromArrayView(Object, Indices, Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerLocation, VisualLoggerColor);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeInclusiveDiscreteActionFromArrayView(
	ULearningAgentsActionObject* Object,
	const TArrayView<const int32> Indices,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveDiscreteActionFromArrayView: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(Indices))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveDiscreteActionFromArrayView: Indices contain duplicates."));
		return FLearningAgentsActionObjectElement();
	}

	const int32 IndexNum = Indices.Num();

	for (int32 IndexIdx = 0; IndexIdx < IndexNum; IndexIdx++)
	{
		if (Indices[IndexIdx] < 0)
		{
			UE_LOG(LogLearning, Error, TEXT("MakeInclusiveDiscreteActionFromArrayView: Invalid Action Index %i."), Indices[IndexIdx]);
			return FLearningAgentsActionObjectElement();
		}
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nIndices: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*UE::Learning::Array::FormatInt32(Indices, 256));
	}
#endif

	return { Object->ActionObject.CreateDiscreteInclusive({ Indices }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeNamedInclusiveDiscreteAction(
	ULearningAgentsActionObject* Object,
	const TArray<FName>& Names,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	return MakeNamedInclusiveDiscreteActionFromArrayView(Object, Names, Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerLocation, VisualLoggerColor);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeNamedInclusiveDiscreteActionFromArrayView(
	ULearningAgentsActionObject* Object,
	const TArrayView<const FName> Names,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNamedInclusiveDiscreteActionFromArrayView: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(Names))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNamedInclusiveDiscreteActionFromArrayView: Names contain duplicates."));
		return FLearningAgentsActionObjectElement();
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nNames: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*UE::Learning::Agents::Action::Private::FormatNames(Names, 256));
	}
#endif

	return { Object->ActionObject.CreateNamedDiscreteInclusive({ Names }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeStructAction(ULearningAgentsActionObject* Object, const TMap<FName, FLearningAgentsActionObjectElement>& Elements, const FName Tag)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStructAction: Creating zero-sized Struct Action."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<FLearningAgentsActionObjectElement, TInlineAllocator<16>> SubElements;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	SubElements.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsActionObjectElement>& Element : Elements)
	{
		SubElements.Add(Element.Value);
		SubElementNames.Add(Element.Key);
	}

	return MakeStructActionFromArrayViews(Object, SubElementNames, SubElements, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeStructActionFromArrays(ULearningAgentsActionObject* Object, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Tag)
{
	return MakeStructActionFromArrayViews(Object, ElementNames, Elements, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeStructActionFromArrayViews(ULearningAgentsActionObject* Object, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructActionFromArrayViews: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStructActionFromArrayViews: Creating zero-sized Struct Action."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructActionFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionObjectElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructActionFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsActionObjectElement();
	}

	TArray<UE::Learning::Action::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionObjectElement& Element : Elements)
	{
		if (!Object->ActionObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeStructActionFromArrayViews: Invalid Action Object."));
			return FLearningAgentsActionObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { Object->ActionObject.CreateAnd({ ElementNames, SubElements }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeExclusiveUnionAction(ULearningAgentsActionObject* Object, const FName ElementName, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveUnionAction: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveUnionAction: Invalid Action Object."));
		return FLearningAgentsActionObjectElement();
	}

	return { Object->ActionObject.CreateOrExclusive({ ElementName, Element.ObjectElement }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeInclusiveUnionAction(ULearningAgentsActionObject* Object, const TMap<FName, FLearningAgentsActionObjectElement>& Elements, const FName Tag)
{
	const int32 SubElementNum = Elements.Num();

	TArray<FLearningAgentsActionObjectElement, TInlineAllocator<16>> SubElements;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	SubElements.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsActionObjectElement>& Element : Elements)
	{
		SubElements.Add(Element.Value);
		SubElementNames.Add(Element.Key);
	}

	return MakeInclusiveUnionActionFromArrayViews(Object, SubElementNames, SubElements, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeInclusiveUnionActionFromArrays(ULearningAgentsActionObject* Object, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Tag)
{
	return MakeInclusiveUnionActionFromArrayViews(Object, ElementNames, Elements, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeInclusiveUnionActionFromArrayViews(ULearningAgentsActionObject* Object, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionActionFromArrayViews: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionActionFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionObjectElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionActionFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsActionObjectElement();
	}

	TArray<UE::Learning::Action::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionObjectElement& Element : Elements)
	{
		if (!Object->ActionObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionActionFromArrayViews: Invalid Action Object."));
			return FLearningAgentsActionObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { Object->ActionObject.CreateOrInclusive({ ElementNames, SubElements }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeStaticArrayAction(ULearningAgentsActionObject* Object, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Tag)
{
	return MakeStaticArrayActionFromArrayView(Object, Elements, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeStaticArrayActionFromArrayView(ULearningAgentsActionObject* Object, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStaticArrayActionFromArrayView: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStaticArrayActionFromArrayView: Creating zero-sized Static Array Action."));
	}

	TArray<UE::Learning::Action::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionObjectElement& Element : Elements)
	{
		if (!Object->ActionObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeStaticArrayActionFromArrayView: Invalid Action Object."));
			return FLearningAgentsActionObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { Object->ActionObject.CreateArray({ SubElements }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakePairAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Key, const FLearningAgentsActionObjectElement Value, const FName Tag)
{
	return MakeStructActionFromArrayViews(Object, { TEXT("Key"), TEXT("Value") }, { Key, Value }, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeEnumAction(
	ULearningAgentsActionObject* Object,
	const UEnum* Enum,
	const uint8 EnumValue,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEnumAction: Enum is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	const int32 EnumValueIndex = Enum->GetIndexByValue(EnumValue);

	if (EnumValueIndex == INDEX_NONE || EnumValueIndex < 0 || EnumValueIndex >= Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEnumAction: EnumValue %i not valid for Enum '%s'."), EnumValue , *Enum->GetName());
		return FLearningAgentsActionObjectElement();
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEnum: %s\nSize: [%i]\nValue: [%s]\nIndex: [%i]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*Enum->GetName(),
			Enum->NumEnums() - 1,
			*Enum->GetDisplayNameTextByValue(EnumValue).ToString(),
			EnumValueIndex);
	}
#endif

	return MakeExclusiveDiscreteAction(Object, EnumValueIndex, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeBitmaskAction(
	ULearningAgentsActionObject* Object,
	const UEnum* Enum,
	const int32 BitmaskValue,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeBitmaskAction: Enum is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeBitmaskAction: Too many values in Enum to use as Bitmask (%i)."), Enum->NumEnums() - 1);
		return FLearningAgentsActionObjectElement();
	}

	TArray<int32, TInlineAllocator<32>> BitmaskIndices;
	BitmaskIndices.Empty(Enum->NumEnums() - 1);

	for (int32 BitmaskIdx = 0; BitmaskIdx < Enum->NumEnums() - 1; BitmaskIdx++)
	{
		if (BitmaskValue & (1 << BitmaskIdx))
		{
			BitmaskIndices.Add(BitmaskIdx);
		}
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		FString ValuesString;
		FString IndicesString;

		for (int32 EnumIdx = 0; EnumIdx < Enum->NumEnums() - 1; EnumIdx++)
		{
			if (BitmaskValue & (1 << EnumIdx))
			{
				ValuesString += Enum->GetDisplayNameTextByIndex(EnumIdx).ToString() + TEXT(" ");
				IndicesString += FString::FromInt(EnumIdx) + TEXT(" ");
			}
		}

		ValuesString = ValuesString.TrimEnd();
		IndicesString = IndicesString.TrimEnd();

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEnum: %s\nSize: [%i]\nValues: [%s]\nIndices: [%s]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*Enum->GetName(),
			Enum->NumEnums() - 1,
			*ValuesString,
			*IndicesString);
	}
#endif

	return MakeInclusiveDiscreteActionFromArrayView(Object, BitmaskIndices, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeOptionalAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const ELearningAgentsOptionalAction Option, const FName Tag)
{
	return MakeExclusiveUnionAction(Object,
		Option == ELearningAgentsOptionalAction::Null ? TEXT("Null") : TEXT("Valid"),
		Option == ELearningAgentsOptionalAction::Null ? MakeNullAction(Object) : Element,
		Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeOptionalNullAction(ULearningAgentsActionObject* Object, const FName Tag)
{
	return MakeExclusiveUnionAction(Object, TEXT("Null"), MakeNullAction(Object), Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeOptionalValidAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	return MakeExclusiveUnionAction(Object, TEXT("Valid"), Element, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeEitherAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const ELearningAgentsEitherAction Either, const FName Tag)
{
	return MakeExclusiveUnionAction(Object, Either == ELearningAgentsEitherAction::A ? TEXT("A") : TEXT("B"), Element, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeEitherAAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement A, const FName Tag)
{
	return MakeExclusiveUnionAction(Object, TEXT("A"), A, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeEitherBAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement B, const FName Tag)
{
	return MakeExclusiveUnionAction(Object, TEXT("B"), B, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeEncodingAction(ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEncodingAction: Object is nullptr."));
		return FLearningAgentsActionObjectElement();
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEncodingAction: Invalid Action Object."));
		return FLearningAgentsActionObjectElement();
	}

	return { Object->ActionObject.CreateEncoding({ Element.ObjectElement }, Tag) };
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeBoolAction(
	ULearningAgentsActionObject* Object,
	const bool bValue,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nValue: [%s]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			bValue ? TEXT("true") : TEXT("false"));
	}
#endif

	return MakeExclusiveDiscreteAction(Object, bValue ? 1 : 0, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeFloatAction(
	ULearningAgentsActionObject* Object,
	const float Value,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nValue: [% 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Value);
	}
#endif

	return MakeContinuousActionFromArrayView(Object, { Value }, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeLocationAction(
	ULearningAgentsActionObject* Object,
	const FVector Location,
	const FTransform RelativeTransform,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FVector LocalLocation = RelativeTransform.InverseTransformPosition(Location);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_LOCATION(VisualLoggerObject, LogLearning, Display,
			Location,
			10,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			Location,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Location: [% 6.1f % 6.1f % 6.1f]\nLocation: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalLocation.X, LocalLocation.Y, LocalLocation.Z,
			Location.X, Location.Y, Location.Z);
	}
#endif

	return MakeContinuousActionFromArrayView(Object, {
		(float)LocalLocation.X,
		(float)LocalLocation.Y,
		(float)LocalLocation.Z }, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeRotationAction(
	ULearningAgentsActionObject* Object,
	const FRotator Rotation,
	const FRotator RelativeRotation,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerRotationLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	return MakeRotationActionFromQuat(Object, FQuat::MakeFromRotator(Rotation), FQuat::MakeFromRotator(RelativeRotation), Tag,
		bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerRotationLocation, VisualLoggerLocation, VisualLoggerColor);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeRotationActionFromQuat(
	ULearningAgentsActionObject* Object,
	const FQuat Rotation,
	const FQuat RelativeRotation,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerRotationLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	FQuat LocalRotation = (RelativeRotation.Inverse() * Rotation).GetShortestArcWith(FQuat::Identity);
	const FVector RotationVector = LocalRotation.ToRotationVector();

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			VisualLoggerRotationLocation,
			LocalRotation.Rotator(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Rotation Vector: [% 6.1f % 6.1f % 6.1f]\nLocal Rotation: [% 6.1f % 6.1f % 6.1f % 6.1f]\nRotation: [% 6.1f % 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			RotationVector.X, RotationVector.Y, RotationVector.Z,
			LocalRotation.X, LocalRotation.Y, LocalRotation.Z, LocalRotation.W,
			Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
	}
#endif

	return MakeContinuousActionFromArrayView(Object, {
		(float)FMath::RadiansToDegrees(RotationVector.X),
		(float)FMath::RadiansToDegrees(RotationVector.Y),
		(float)FMath::RadiansToDegrees(RotationVector.Z),
		}, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeScaleAction(
	ULearningAgentsActionObject* Object,
	const FVector Scale,
	const FVector RelativeScale,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FVector LocalLogScale =
		UE::Learning::Agents::Action::Private::VectorLogSafe(Scale) -
		UE::Learning::Agents::Action::Private::VectorLogSafe(RelativeScale);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Log Scale: [% 6.1f % 6.1f % 6.1f]\nScale: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalLogScale.X, LocalLogScale.Y, LocalLogScale.Z,
			Scale.X, Scale.Y, Scale.Z);
	}
#endif

	return MakeContinuousActionFromArrayView(Object, {
		(float)LocalLogScale.X,
		(float)LocalLogScale.Y,
		(float)LocalLogScale.Z,
		}, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeTransformAction(
	ULearningAgentsActionObject* Object,
	const FTransform Transform,
	const FTransform RelativeTransform,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	FVector Location = Transform.GetLocation();
	FQuat Rotation = Transform.GetRotation();
	FVector Scale = Transform.GetScale3D();

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			Location,
			Rotation,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocation: [% 6.1f % 6.1f % 6.1f]\nRotation: [% 6.1f % 6.1f % 6.1f % 6.1f]\nScale: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Location.X, Location.Y, Location.Z,
			Rotation.X, Rotation.Y, Rotation.Z, Rotation.W,
			Scale.X, Scale.Y, Scale.Z);
	}
#endif

	return MakeStructActionFromArrayViews(Object,
		{
			TEXT("Location"),
			TEXT("Rotation"),
			TEXT("Scale")
		},
		{
			MakeLocationAction(Object, Location, RelativeTransform),
			MakeRotationActionFromQuat(Object, Rotation, RelativeTransform.GetRotation()),
			MakeScaleAction(Object, Scale, RelativeTransform.GetScale3D())
		},
		Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeAngleAction(
	ULearningAgentsActionObject* Object,
	const float Angle,
	const float RelativeAngle,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerAngleLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	float LocalAngle = FMath::FindDeltaAngleDegrees(RelativeAngle, Angle);
	
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ANGLE_DEGREES(VisualLoggerObject, LogLearning, Display,
			Angle,
			0.0f,
			VisualLoggerAngleLocation,
			10.0f,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Angle: [% 6.1f]\nAngle: [% 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalAngle,
			Angle);
	}
#endif

	return MakeContinuousActionFromArrayView(Object, { LocalAngle }, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeAngleActionRadians(
	ULearningAgentsActionObject* Object,
	const float Angle,
	const float RelativeAngle,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerAngleLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	return MakeAngleAction(Object, FMath::RadiansToDegrees(Angle), FMath::RadiansToDegrees(RelativeAngle), Tag,
		bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerAngleLocation, VisualLoggerLocation, VisualLoggerColor);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeVelocityAction(
	ULearningAgentsActionObject* Object,
	const FVector Velocity,
	const FTransform RelativeTransform,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerVelocityLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FVector LocalVelocity = RelativeTransform.InverseTransformVectorNoScale(Velocity);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ARROW(VisualLoggerObject, LogLearning, Display,
			VisualLoggerVelocityLocation,
			VisualLoggerVelocityLocation + Velocity,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Velocity: [% 6.1f % 6.1f % 6.1f]\nVelocity: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalVelocity.X, LocalVelocity.Y, LocalVelocity.Z,
			Velocity.X, Velocity.Y, Velocity.Z);
	}
#endif

	return MakeContinuousActionFromArrayView(Object, {
		(float)LocalVelocity.X,
		(float)LocalVelocity.Y,
		(float)LocalVelocity.Z,
		}, Tag);
}

FLearningAgentsActionObjectElement ULearningAgentsActions::MakeDirectionAction(
	ULearningAgentsActionObject* Object,
	const FVector Direction,
	const FTransform RelativeTransform,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerDirectionLocation,
	const FVector VisualLoggerLocation,
	const float VisualLoggerArrowLength,
	const FLinearColor VisualLoggerColor)
{
	const FVector LocalDirection = RelativeTransform.InverseTransformVectorNoScale(Direction).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ARROW(VisualLoggerObject, LogLearning, Display,
			VisualLoggerDirectionLocation,
			VisualLoggerDirectionLocation + VisualLoggerArrowLength * Direction,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Direction: [% 6.1f % 6.1f % 6.1f]\nDirection: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalDirection.X, LocalDirection.Y, LocalDirection.Z,
			Direction.X, Direction.Y, Direction.Z);
	}
#endif

	return MakeContinuousActionFromArrayView(Object, {
		(float)LocalDirection.X,
		(float)LocalDirection.Y,
		(float)LocalDirection.Z,
		}, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeNullActionModifier(ULearningAgentsActionModifier* Modifier, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNullActionModifier: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	return { Modifier->ActionModifier.CreateNull(Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeContinuousActionModifier(ULearningAgentsActionModifier* Modifier, const TArray<bool>& Masked, const TArray<float>& MaskedValues, const FName Tag)
{
	return MakeContinuousActionModifierFromArrayView(Modifier, Masked, MaskedValues, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeContinuousActionModifierFromArrayView(ULearningAgentsActionModifier* Modifier, const TArrayView<const bool> Masked, const TArrayView<const float> MaskedValues, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeContinuousActionModifierFromArrayView: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (MaskedValues.Num() != Masked.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeContinuousActionModifierFromArrayView: Masked and MaskedValues sizes don't match. Got %i and %i."), Masked.Num(), MaskedValues.Num());
		return FLearningAgentsActionModifierElement();
	}

	return { Modifier->ActionModifier.CreateContinuous({ Masked, MaskedValues }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeExclusiveDiscreteActionModifier(ULearningAgentsActionModifier* Modifier, const TArray<int32>& MaskedIndices, const FName Tag)
{
	return MakeExclusiveDiscreteActionModifierFromArrayView(Modifier, MaskedIndices, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeExclusiveDiscreteActionModifierFromArrayView(ULearningAgentsActionModifier* Modifier, const TArrayView<const int32> MaskedIndices, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveDiscreteActionModifierFromArrayView: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(MaskedIndices))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveDiscreteActionModifierFromArrayView: MaskedIndices contain duplicates."));
		return FLearningAgentsActionModifierElement();
	}	

	return { Modifier->ActionModifier.CreateDiscreteExclusive({ MaskedIndices }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeNamedExclusiveDiscreteActionModifier(ULearningAgentsActionModifier* Modifier, const TArray<FName>& MaskedNames, const FName Tag)
{
	return MakeNamedExclusiveDiscreteActionModifierFromArrayView(Modifier, MaskedNames, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeNamedExclusiveDiscreteActionModifierFromArrayView(ULearningAgentsActionModifier* Modifier, const TArrayView<const FName> MaskedNames, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNamedExclusiveDiscreteActionModifierFromArrayView: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(MaskedNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNamedExclusiveDiscreteActionModifierFromArrayView: MaskedNames contain duplicates."));
		return FLearningAgentsActionModifierElement();
	}

	return { Modifier->ActionModifier.CreateNamedDiscreteExclusive({ MaskedNames }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeInclusiveDiscreteActionModifier(ULearningAgentsActionModifier* Modifier, const TArray<int32>& MaskedIndices, const FName Tag)
{
	return MakeInclusiveDiscreteActionModifierFromArrayView(Modifier, MaskedIndices, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeInclusiveDiscreteActionModifierFromArrayView(ULearningAgentsActionModifier* Modifier, const TArrayView<const int32> MaskedIndices, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveDiscreteActionModifierFromArrayView: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(MaskedIndices))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNamedExclusiveDiscreteActionModifierFromArrayView: MaskedIndices contain duplicates."));
		return FLearningAgentsActionModifierElement();
	}	

	return { Modifier->ActionModifier.CreateDiscreteInclusive({ MaskedIndices }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeNamedInclusiveDiscreteActionModifier(ULearningAgentsActionModifier* Modifier, const TArray<FName>& MaskedNames, const FName Tag)
{
	return MakeNamedInclusiveDiscreteActionModifierFromArrayView(Modifier, MaskedNames, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeNamedInclusiveDiscreteActionModifierFromArrayView(ULearningAgentsActionModifier* Modifier, const TArrayView<const FName> MaskedNames, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNamedInclusiveDiscreteActionModifierFromArrayView: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}
	
	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(MaskedNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNamedExclusiveDiscreteActionModifierFromArrayView: MaskedNames contain duplicates."));
		return FLearningAgentsActionModifierElement();
	}	

	return { Modifier->ActionModifier.CreateNamedDiscreteInclusive({ MaskedNames }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeStructActionModifier(ULearningAgentsActionModifier* Modifier, const TMap<FName, FLearningAgentsActionModifierElement>& Elements, const FName Tag)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStructActionModifier: Creating zero-sized Struct Action Modifier."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<FLearningAgentsActionModifierElement, TInlineAllocator<16>> SubElements;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	SubElements.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsActionModifierElement>& Element : Elements)
	{
		SubElements.Add(Element.Value);
		SubElementNames.Add(Element.Key);
	}

	return MakeStructActionModifierFromArrayViews(Modifier, SubElementNames, SubElements, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeStructActionModifierFromArrays(ULearningAgentsActionModifier* Modifier, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionModifierElement>& Elements, const FName Tag)
{
	return MakeStructActionModifierFromArrayViews(Modifier, ElementNames, Elements, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeStructActionModifierFromArrayViews(ULearningAgentsActionModifier* Modifier, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionModifierElement> Elements, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructActionModifierFromArrayViews: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStructActionModifierFromArrayViews: Creating zero-sized Struct Action Modifier."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructActionModifierFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionModifierElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructActionModifierFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsActionModifierElement();
	}

	TArray<UE::Learning::Action::FModifierElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionModifierElement& Element : Elements)
	{
		if (!Modifier->ActionModifier.IsValid(Element.ModifierElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeStructActionModifierFromArrayViews: Invalid Action Modifier."));
			return FLearningAgentsActionModifierElement();
		}

		SubElements.Add(Element.ModifierElement);
	}

	return { Modifier->ActionModifier.CreateAnd({ ElementNames, SubElements }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeExclusiveUnionActionModifier(ULearningAgentsActionModifier* Modifier, const TMap<FName, FLearningAgentsActionModifierElement>& Elements, const TArray<FName>& MaskedElements, const FName Tag)
{
	const int32 SubElementNum = Elements.Num();

	TArray<FLearningAgentsActionModifierElement, TInlineAllocator<16>> SubElements;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	SubElements.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsActionModifierElement>& Element : Elements)
	{
		SubElements.Add(Element.Value);
		SubElementNames.Add(Element.Key);
	}

	return MakeExclusiveUnionActionModifierFromArrayViews(Modifier, SubElementNames, SubElements, MaskedElements, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeExclusiveUnionActionModifierFromArrays(ULearningAgentsActionModifier* Modifier, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionModifierElement>& Elements, const TArray<FName>& MaskedElements, const FName Tag)
{
	return MakeExclusiveUnionActionModifierFromArrayViews(Modifier, ElementNames, Elements, MaskedElements, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeExclusiveUnionActionModifierFromArrayViews(ULearningAgentsActionModifier* Modifier, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionModifierElement> Elements, const TArrayView<const FName> MaskedElements, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveUnionActionModifierFromArrayViews: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveUnionActionModifierFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionModifierElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveUnionActionModifierFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsActionModifierElement();
	}

	TArray<UE::Learning::Action::FModifierElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionModifierElement& Element : Elements)
	{
		if (!Modifier->ActionModifier.IsValid(Element.ModifierElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeExclusiveUnionActionModifierFromArrayViews: Invalid Action Modifier."));
			return FLearningAgentsActionModifierElement();
		}

		SubElements.Add(Element.ModifierElement);
	}

	return { Modifier->ActionModifier.CreateOrExclusive({ ElementNames, SubElements, MaskedElements }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeInclusiveUnionActionModifier(ULearningAgentsActionModifier* Modifier, const TMap<FName, FLearningAgentsActionModifierElement>& Elements, const TArray<FName>& MaskedElements, const FName Tag)
{
	const int32 SubElementNum = Elements.Num();

	TArray<FLearningAgentsActionModifierElement, TInlineAllocator<16>> SubElements;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	SubElements.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsActionModifierElement>& Element : Elements)
	{
		SubElements.Add(Element.Value);
		SubElementNames.Add(Element.Key);
	}

	return MakeInclusiveUnionActionModifierFromArrayViews(Modifier, SubElementNames, SubElements, MaskedElements, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeInclusiveUnionActionModifierFromArrays(ULearningAgentsActionModifier* Modifier, const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionModifierElement>& Elements, const TArray<FName>& MaskedElements, const FName Tag)
{
	return MakeInclusiveUnionActionModifierFromArrayViews(Modifier, ElementNames, Elements, MaskedElements, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeInclusiveUnionActionModifierFromArrayViews(ULearningAgentsActionModifier* Modifier, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionModifierElement> Elements, const TArrayView<const FName> MaskedElements, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionActionModifierFromArrayViews: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionActionModifierFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionModifierElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionActionModifierFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsActionModifierElement();
	}

	TArray<UE::Learning::Action::FModifierElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionModifierElement& Element : Elements)
	{
		if (!Modifier->ActionModifier.IsValid(Element.ModifierElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionActionModifierFromArrayViews: Invalid Action Modifier."));
			return FLearningAgentsActionModifierElement();
		}

		SubElements.Add(Element.ModifierElement);
	}

	return { Modifier->ActionModifier.CreateOrInclusive({ ElementNames, SubElements, MaskedElements }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeStaticArrayActionModifier(ULearningAgentsActionModifier* Modifier, const TArray<FLearningAgentsActionModifierElement>& Elements, const FName Tag)
{
	return MakeStaticArrayActionModifierFromArrayView(Modifier, Elements, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeStaticArrayActionModifierFromArrayView(ULearningAgentsActionModifier* Modifier, const TArrayView<const FLearningAgentsActionModifierElement> Elements, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStaticArrayActionModifierFromArrayView: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStaticArrayActionModifierFromArrayView: Creating zero-sized Static Array Action."));
	}

	TArray<UE::Learning::Action::FModifierElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionModifierElement& Element : Elements)
	{
		if (!Modifier->ActionModifier.IsValid(Element.ModifierElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeStaticArrayActionModifierFromArrayView: Invalid Action Modifier."));
			return FLearningAgentsActionModifierElement();
		}

		SubElements.Add(Element.ModifierElement);
	}

	return { Modifier->ActionModifier.CreateArray({ SubElements }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakePairActionModifier(ULearningAgentsActionModifier* Modifier, const FLearningAgentsActionModifierElement Key, const FLearningAgentsActionModifierElement Value, const FName Tag)
{
	return MakeStructActionModifierFromArrayViews(Modifier, { TEXT("Key"), TEXT("Value") }, { Key, Value }, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeEnumActionModifier(ULearningAgentsActionModifier* Modifier, const UEnum* Enum, const TArray<uint8>& EnumMaskedValues, const FName Tag)
{
	return MakeEnumActionModifierFromArrayView(Modifier, Enum, EnumMaskedValues, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeEnumActionModifierFromArrayView(ULearningAgentsActionModifier* Modifier, const UEnum* Enum, const TArrayView<const uint8> EnumMaskedValues, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEnumActionModifierFromArrayView: Enum is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	TArray<int32, TInlineAllocator<32>> EnumValueIndices;

	for (const uint8 EnumValue : EnumMaskedValues)
	{
		const int32 EnumValueIndex = Enum->GetIndexByValue(EnumValue);

		if (EnumValueIndex == INDEX_NONE || EnumValueIndex < 0 || EnumValueIndex >= Enum->NumEnums() - 1)
		{
			UE_LOG(LogLearning, Error, TEXT("MakeEnumActionModifierFromArrayView: EnumValue %i not valid for Enum '%s'."), EnumValue, *Enum->GetName());
			return FLearningAgentsActionModifierElement();
		}
	}

	return MakeExclusiveDiscreteActionModifierFromArrayView(Modifier, EnumValueIndices, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeBitmaskActionModifier(ULearningAgentsActionModifier* Modifier, const UEnum* Enum, const int32 MaskedBitmask, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeBitmaskActionModifier: Enum is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeBitmaskActionModifier: Too many values in Enum to use as Bitmask (%i)."), Enum->NumEnums() - 1);
		return FLearningAgentsActionModifierElement();
	}

	TArray<int32, TInlineAllocator<32>> BitmaskIndices;
	BitmaskIndices.Empty(Enum->NumEnums() - 1);

	for (int32 BitmaskIdx = 0; BitmaskIdx < Enum->NumEnums() - 1; BitmaskIdx++)
	{
		if (MaskedBitmask & (1 << BitmaskIdx))
		{
			BitmaskIndices.Add(BitmaskIdx);
		}
	}

	return MakeInclusiveDiscreteActionModifierFromArrayView(Modifier, BitmaskIndices, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeOptionalActionModifier(ULearningAgentsActionModifier* Modifier, const FLearningAgentsActionModifierElement Element, const bool bAllowOnlyValid, const bool bAllowOnlyNull, const FName Tag)
{
	if (bAllowOnlyValid && bAllowOnlyNull)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeOptionalActionModifier: Must either set bAllowOnlyValid or bAllowOnlyNull."));
	}

	if (bAllowOnlyValid)
	{
		return MakeExclusiveUnionActionModifierFromArrayViews(Modifier, { TEXT("Valid") }, { Element }, { TEXT("Null") }, Tag);
	}

	if (bAllowOnlyNull)
	{
		return MakeExclusiveUnionActionModifierFromArrayViews(Modifier, { TEXT("Valid") }, { Element }, { TEXT("Valid") }, Tag);
	}

	return MakeExclusiveUnionActionModifierFromArrayViews(Modifier, { TEXT("Valid") }, { Element }, {}, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeEitherActionModifier(ULearningAgentsActionModifier* Modifier, const FLearningAgentsActionModifierElement A, const FLearningAgentsActionModifierElement B, const bool bAllowOnlyA, const bool bAllowOnlyB, const FName Tag)
{
	if (bAllowOnlyA && bAllowOnlyB)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeEitherActionModifier: Must either set bAllowOnlyA or bAllowOnlyB."));
	}

	if (bAllowOnlyA)
	{
		return MakeExclusiveUnionActionModifierFromArrayViews(Modifier, { TEXT("A"), TEXT("B") }, { A, B }, { TEXT("B") }, Tag);
	}

	if (bAllowOnlyB)
	{
		return MakeExclusiveUnionActionModifierFromArrayViews(Modifier, { TEXT("A"), TEXT("B") }, { A, B }, { TEXT("A") }, Tag);
	}

	return MakeExclusiveUnionActionModifierFromArrayViews(Modifier, { TEXT("A"), TEXT("B") }, { A, B }, {}, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeEncodingActionModifier(ULearningAgentsActionModifier* Modifier, const FLearningAgentsActionModifierElement Element, const FName Tag)
{
	if (!Modifier)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEncodingActionModifier: Modifier is nullptr."));
		return FLearningAgentsActionModifierElement();
	}

	if (!Modifier->ActionModifier.IsValid(Element.ModifierElement))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEncodingActionModifier: Invalid Action Modifier."));
		return FLearningAgentsActionModifierElement();
	}

	return { Modifier->ActionModifier.CreateEncoding({ Element.ModifierElement }, Tag) };
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeBoolActionModifier(ULearningAgentsActionModifier* Modifier, const bool bValue, const FName Tag)
{
	return MakeExclusiveDiscreteActionModifierFromArrayView(Modifier, { bValue ? 0 : 1 }, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeFloatActionModifier(ULearningAgentsActionModifier* Modifier, const float MaskedValue, const bool bMasked, const FName Tag)
{
	return MakeContinuousActionModifierFromArrayView(Modifier, { bMasked }, { MaskedValue }, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeLocationActionModifier(ULearningAgentsActionModifier* Modifier, const FVector MaskedLocation, const bool bMaskedX, const bool bMaskedY, const bool bMaskedZ, const FTransform RelativeTransform, const FName Tag)
{
	const FVector LocalLocation = RelativeTransform.InverseTransformPosition(MaskedLocation);

	return MakeContinuousActionModifierFromArrayView(Modifier, {
		bMaskedX,
		bMaskedY,
		bMaskedZ
		}, {
		(float)LocalLocation.X, 
		(float)LocalLocation.Y, 
		(float)LocalLocation.Z }, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeScaleActionModifier(ULearningAgentsActionModifier* Modifier, const FVector MaskedScale, const bool bMaskedX, const bool bMaskedY, const bool bMaskedZ, const FVector RelativeScale, const FName Tag)
{
	const FVector LocalLogScale =
		UE::Learning::Agents::Action::Private::VectorLogSafe(MaskedScale) -
		UE::Learning::Agents::Action::Private::VectorLogSafe(RelativeScale);

	return MakeContinuousActionModifierFromArrayView(Modifier, {
		bMaskedX,
		bMaskedY,
		bMaskedZ
		}, {
		(float)LocalLogScale.X, 
		(float)LocalLogScale.Y, 
		(float)LocalLogScale.Z } , Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeAngleActionModifier(ULearningAgentsActionModifier* Modifier, const float MaskedAngle, const bool bMask, const float RelativeAngle, const FName Tag)
{
	return MakeContinuousActionModifierFromArrayView(Modifier, { bMask }, { FMath::FindDeltaAngleDegrees(RelativeAngle, MaskedAngle) }, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeAngleActionModifierRadians(ULearningAgentsActionModifier* Modifier, const float MaskedAngle, const bool bMask, const float RelativeAngle, const FName Tag)
{
	return MakeAngleActionModifier(Modifier, FMath::RadiansToDegrees(MaskedAngle), bMask, FMath::RadiansToDegrees(RelativeAngle), Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeVelocityActionModifier(ULearningAgentsActionModifier* Modifier, const FVector MaskedVelocity, const bool bMaskedX, const bool bMaskedY, const bool bMaskedZ, const FTransform RelativeTransform, const FName Tag)
{
	const FVector LocalVelocity = RelativeTransform.InverseTransformVectorNoScale(MaskedVelocity);

	return MakeContinuousActionModifierFromArrayView(Modifier, {
		bMaskedX,
		bMaskedY,
		bMaskedZ }, {
		(float)LocalVelocity.X,
		(float)LocalVelocity.Y,
		(float)LocalVelocity.Z,
		}, Tag);
}

FLearningAgentsActionModifierElement ULearningAgentsActions::MakeDirectionActionModifier(ULearningAgentsActionModifier* Modifier, const FVector MaskedDirection, const bool bMaskedX, const bool bMaskedY, const bool bMaskedZ, const FTransform RelativeTransform, const FName Tag)
{
	const FVector LocalDirection = RelativeTransform.InverseTransformVectorNoScale(MaskedDirection).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	return MakeContinuousActionModifierFromArrayView(Modifier, {
		bMaskedX,
		bMaskedY,
		bMaskedZ }, {
		(float)LocalDirection.X,
		(float)LocalDirection.Y,
		(float)LocalDirection.Z,
		}, Tag);
}

bool ULearningAgentsActions::GetNullAction(const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNullAction: Object is nullptr."));
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetNullAction: Invalid Action Object."));
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetNullAction: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Null)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNullAction: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Null));
		return false;
	}

	return true;
}

bool ULearningAgentsActions::GetContinuousActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousActionNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousActionNum: Invalid Action Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetContinuousActionNum: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Continuous)
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousActionNum: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Continuous));
		OutNum = 0;
		return false;
	}

	OutNum = Object->ActionObject.GetContinuous(Element.ObjectElement).Values.Num();
	return true;
}

bool ULearningAgentsActions::GetContinuousAction(
	TArray<float>& OutValues, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	int32 OutValueNum = 0;
	if (!GetContinuousActionNum(OutValueNum, Object, Element, Tag))
	{
		OutValues.Empty();
		return false;
	}

	OutValues.SetNumUninitialized(OutValueNum);

	if (!GetContinuousActionToArrayView(OutValues, Object, Element, Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerLocation, VisualLoggerColor))
	{
		OutValues.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActions::GetContinuousActionToArrayView(
	TArrayView<float> OutValues, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousActionToArrayView: Object is nullptr."));
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousActionToArrayView: Invalid Action Object."));
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetContinuousActionToArrayView: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Continuous)
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousActionToArrayView: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Continuous));
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	const TArrayView<const float> Values = Object->ActionObject.GetContinuous(Element.ObjectElement).Values;

	if (Values.Num() != OutValues.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousActionToArrayView: Action '%s' size does not match. Action is '%i' values but asked for '%i'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			Values.Num(), OutValues.Num());
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	UE::Learning::Array::Copy<1, float>(OutValues, Values);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEncoded: %s\nValues: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*UE::Learning::Array::FormatFloat(OutValues),
			*UE::Learning::Array::FormatFloat(OutValues)); // Encoded is identical to provided values
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetExclusiveDiscreteAction(
	int32& OutIndex, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetExclusiveDiscreteAction: Object is nullptr."));
		OutIndex = 0;
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetExclusiveDiscreteAction: Invalid Action Object."));
		OutIndex = 0;
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetExclusiveDiscreteAction: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::DiscreteExclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetExclusiveDiscreteAction: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::DiscreteExclusive));
		OutIndex = 0;
		return false;
	}

	OutIndex = Object->ActionObject.GetDiscreteExclusive(Element.ObjectElement).DiscreteIndex;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nIndex: [%i]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			OutIndex);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetNamedExclusiveDiscreteAction(
	FName& OutName,
	const ULearningAgentsActionObject* Object,
	const FLearningAgentsActionObjectElement Element,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedExclusiveDiscreteAction: Object is nullptr."));
		OutName = NAME_None;
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedExclusiveDiscreteAction: Invalid Action Object."));
		OutName = NAME_None;
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetNamedExclusiveDiscreteAction: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::NamedDiscreteExclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedExclusiveDiscreteAction: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::NamedDiscreteExclusive));
		OutName = NAME_None;
		return false;
	}

	OutName = Object->ActionObject.GetNamedDiscreteExclusive(Element.ObjectElement).ElementName;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nName: [%s]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*OutName.ToString());
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetInclusiveDiscreteActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteActionNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteActionNum: Invalid Action Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetInclusiveDiscreteActionNum: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::DiscreteInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteActionNum: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::DiscreteInclusive));
		OutNum = 0;
		return false;
	}

	OutNum = Object->ActionObject.GetDiscreteInclusive(Element.ObjectElement).DiscreteIndices.Num();
	return true;
}

bool ULearningAgentsActions::GetInclusiveDiscreteAction(
	TArray<int32>& OutIndices, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	int32 OutIndexNum = 0;
	if (!GetInclusiveDiscreteActionNum(OutIndexNum, Object, Element, Tag))
	{
		OutIndices.Empty();
		return false;
	}

	OutIndices.SetNumUninitialized(OutIndexNum);

	if (!GetInclusiveDiscreteActionToArrayView(OutIndices, Object, Element, Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerLocation, VisualLoggerColor))
	{
		OutIndices.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActions::GetInclusiveDiscreteActionToArrayView(
	TArrayView<int32> OutIndices,
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteActionToArrayView: Object is nullptr."));
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteActionToArrayView: Invalid Action Object."));
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetInclusiveDiscreteActionToArrayView: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::DiscreteInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteActionToArrayView: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::DiscreteInclusive));
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	const TArrayView<const int32> Indices = Object->ActionObject.GetDiscreteInclusive(Element.ObjectElement).DiscreteIndices;

	if (Indices.Num() != OutIndices.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteActionToArrayView: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			Indices.Num(), OutIndices.Num());
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	UE::Learning::Array::Copy<1, int32>(OutIndices, Indices);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nIndices: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*UE::Learning::Array::FormatInt32(OutIndices, 256));
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetNamedInclusiveDiscreteActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedInclusiveDiscreteActionNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedInclusiveDiscreteActionNum: Invalid Action Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetNamedInclusiveDiscreteActionNum: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::NamedDiscreteInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedInclusiveDiscreteActionNum: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::NamedDiscreteInclusive));
		OutNum = 0;
		return false;
	}

	OutNum = Object->ActionObject.GetNamedDiscreteInclusive(Element.ObjectElement).ElementNames.Num();
	return true;
}

bool ULearningAgentsActions::GetNamedInclusiveDiscreteAction(
	TArray<FName>& OutNames,
	const ULearningAgentsActionObject* Object,
	const FLearningAgentsActionObjectElement Element,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	int32 OutNameNum = 0;
	if (!GetNamedInclusiveDiscreteActionNum(OutNameNum, Object, Element, Tag))
	{
		OutNames.Empty();
		return false;
	}

	OutNames.SetNumUninitialized(OutNameNum);

	if (!GetNamedInclusiveDiscreteActionToArrayView(OutNames, Object, Element, Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerLocation, VisualLoggerColor))
	{
		OutNames.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActions::GetNamedInclusiveDiscreteActionToArrayView(
	TArrayView<FName> OutNames,
	const ULearningAgentsActionObject* Object,
	const FLearningAgentsActionObjectElement Element,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedInclusiveDiscreteActionToArrayView: Object is nullptr."));
		UE::Learning::Array::Set<1, FName>(OutNames, NAME_None);
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedInclusiveDiscreteActionToArrayView: Invalid Action Object."));
		UE::Learning::Array::Set<1, FName>(OutNames, NAME_None);
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetNamedInclusiveDiscreteActionToArrayView: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::NamedDiscreteInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedInclusiveDiscreteActionToArrayView: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::NamedDiscreteInclusive));
		UE::Learning::Array::Set<1, FName>(OutNames, NAME_None);
		return false;
	}

	const TArrayView<const FName> Names = Object->ActionObject.GetNamedDiscreteInclusive(Element.ObjectElement).ElementNames;

	if (Names.Num() != OutNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetNamedInclusiveDiscreteActionToArrayView: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			Names.Num(), OutNames.Num());
		UE::Learning::Array::Set<1, FName>(OutNames, NAME_None);
		return false;
	}

	UE::Learning::Array::Copy<1, FName>(OutNames, Names);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nNames: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*UE::Learning::Agents::Action::Private::FormatNames(OutNames, 256));
	}
#endif

	return true;
}


bool ULearningAgentsActions::GetStructActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionNum: Invalid Action Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStructActionNum: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionNum: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::And));
		OutNum = 0;
		return false;
	}
	
	const UE::Learning::Action::FObjectAndParameters Parameters = Object->ActionObject.GetAnd(Element.ObjectElement);

	OutNum = Parameters.Elements.Num();
	return true;
}

bool ULearningAgentsActions::GetStructAction(TMap<FName, FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetStructActionNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.SetNumUninitialized(OutElementNum);
	SubElements.SetNumUninitialized(OutElementNum);

	if (!GetStructActionToArrayViews(SubElementNames, SubElements, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(OutElementNum);
	for (int32 ElementIdx = 0; ElementIdx < OutElementNum; ElementIdx++)
	{
		OutElements.Add(SubElementNames[ElementIdx], SubElements[ElementIdx]);
	}

	return true;
}

bool ULearningAgentsActions::GetStructActionElement(FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName ElementName, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionElement: Object is nullptr."));
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionElement: Invalid Action Object."));
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStructActionElement: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionElement: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::And));
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	const UE::Learning::Action::FObjectAndParameters Parameters = Object->ActionObject.GetAnd(Element.ObjectElement);

	const int32 ElementIdx = Parameters.ElementNames.Find(ElementName);

	if (ElementIdx == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionElement: Element '%s' not found."), *ElementName.ToString());
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	OutElement = { Parameters.Elements[ElementIdx] };
	return true;
}

bool ULearningAgentsActions::GetStructActionToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetStructActionNum(OutElementNum, Object, Element, Tag))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	OutElementNames.SetNumUninitialized(OutElementNum);
	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetStructActionToArrayViews(OutElementNames, OutElements, Object, Element, Tag))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActions::GetStructActionToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsActionObjectElement> OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionToArrayViews: Object is nullptr."));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionToArrayViews: Invalid Action Object."));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStructActionToArrayViews: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionToArrayViews: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::And));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	const UE::Learning::Action::FObjectAndParameters Parameters = Object->ActionObject.GetAnd(Element.ObjectElement);

	if (Parameters.Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStructActionToArrayViews: Getting zero-sized And Action."));
	}

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructActionToArrayViews: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!Object->ActionObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("GetStructActionToArrayViews: Invalid Action Object."));
			UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
			UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
			return false;
		}

		OutElementNames[ElementIdx] = Parameters.ElementNames[ElementIdx];
		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsActions::GetExclusiveUnionAction(FName& OutElementName, FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetExclusiveUnionAction: Object is nullptr."));
		OutElementName = NAME_None;
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetExclusiveUnionAction: Invalid Action Object."));
		OutElementName = NAME_None;
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetExclusiveUnionAction: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::OrExclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetExclusiveUnionAction: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::OrExclusive));
		OutElementName = NAME_None;
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	const UE::Learning::Action::FObjectOrExclusiveParameters Parameters = Object->ActionObject.GetOrExclusive(Element.ObjectElement);

	OutElementName = Parameters.ElementName;
	OutElement = { Parameters.Element };
	return true;
}

bool ULearningAgentsActions::GetInclusiveUnionActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionActionNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionActionNum: Invalid Action Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetInclusiveUnionActionNum: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::OrInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionActionNum: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::OrInclusive));
		OutNum = 0;
		return false;
	}

	const UE::Learning::Action::FObjectOrInclusiveParameters Parameters = Object->ActionObject.GetOrInclusive(Element.ObjectElement);

	OutNum = Parameters.Elements.Num();
	return true;
}

bool ULearningAgentsActions::GetInclusiveUnionAction(TMap<FName, FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetInclusiveUnionActionNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.SetNumUninitialized(OutElementNum);
	SubElements.SetNumUninitialized(OutElementNum);

	if (!GetInclusiveUnionActionToArrayViews(SubElementNames, SubElements, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(OutElementNum);
	for (int32 ElementIdx = 0; ElementIdx < OutElementNum; ElementIdx++)
	{
		OutElements.Add(SubElementNames[ElementIdx], SubElements[ElementIdx]);
	}

	return true;
}

bool ULearningAgentsActions::GetInclusiveUnionActionToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetInclusiveUnionActionNum(OutElementNum, Object, Element, Tag))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	OutElementNames.SetNumUninitialized(OutElementNum);
	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetInclusiveUnionActionToArrayViews(OutElementNames, OutElements, Object, Element, Tag))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActions::GetInclusiveUnionActionToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsActionObjectElement> OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionActionToArrayViews: Object is nullptr."));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionActionToArrayViews: Invalid Action Object."));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetInclusiveUnionActionToArrayViews: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::OrInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionActionToArrayViews: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::OrInclusive));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	const UE::Learning::Action::FObjectOrInclusiveParameters Parameters = Object->ActionObject.GetOrInclusive(Element.ObjectElement);

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionActionToArrayViews: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!Object->ActionObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionActionToArrayViews: Invalid Action Object."));
			UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
			UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
			return false;
		}

		OutElementNames[ElementIdx] = Parameters.ElementNames[ElementIdx];
		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsActions::GetStaticArrayActionNum(int32& OutNum, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayActionNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayActionNum: Invalid Action Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStaticArrayActionNum: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Array)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayActionNum: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Array));
		OutNum = 0;
		return false;
	}

	OutNum = Object->ActionObject.GetArray(Element.ObjectElement).Elements.Num();
	return true;
}

bool ULearningAgentsActions::GetStaticArrayAction(TArray<FLearningAgentsActionObjectElement>& OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetStaticArrayActionNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetStaticArrayActionToArrayView(OutElements, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActions::GetStaticArrayActionToArrayView(TArrayView<FLearningAgentsActionObjectElement> OutElements, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayActionToArrayView: Object is nullptr."));
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayActionToArrayView: Invalid Action Object."));
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStaticArrayActionToArrayView: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Array)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayActionToArrayView: Action '%s' type does not match. Action is '%s' but asked for '%s'."), 
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Array));
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	const TArrayView<const UE::Learning::Action::FObjectElement> SubElements = Object->ActionObject.GetArray(Element.ObjectElement).Elements;

	if (SubElements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStaticArrayActionToArrayView: Getting zero-sized Array Action."));
	}

	if (SubElements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayActionToArrayView: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			SubElements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < SubElements.Num(); ElementIdx++)
	{
		if (!Object->ActionObject.IsValid(SubElements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("GetStaticArrayActionToArrayView: Invalid Action Object."));
			UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
			return false;
		}

		OutElements[ElementIdx] = { SubElements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsActions::GetPairAction(FLearningAgentsActionObjectElement& OutKey, FLearningAgentsActionObjectElement& OutValue, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	TStaticArray<FName, 2> OutElementNames;
	TStaticArray<FLearningAgentsActionObjectElement, 2> OutElements;
	if (!GetStructActionToArrayViews(OutElementNames, OutElements, Object, Element, Tag))
	{
		OutKey = FLearningAgentsActionObjectElement();
		OutValue = FLearningAgentsActionObjectElement();
		return false;
	}

	OutKey = OutElements[MakeArrayView(OutElementNames).Find(TEXT("Key"))];
	OutValue = OutElements[MakeArrayView(OutElementNames).Find(TEXT("Value"))];
	return true;
}

bool ULearningAgentsActions::GetEnumAction(
	uint8& OutEnumValue, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const UEnum* Enum, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEnumAction: Enum is nullptr."));
		OutEnumValue = 0;
		return false;
	}

	int32 OutIndex = 0;
	if (!GetExclusiveDiscreteAction(OutIndex, Object, Element, Tag))
	{
		OutEnumValue = 0;
		return false;
	}

	if (OutIndex >= Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEnumAction: EnumValue out of range for Enum '%s'. Expected %i or less, got %i."), *Enum->GetName(), Enum->NumEnums() - 1, OutIndex);
		OutEnumValue = 0;
		return false;
	}

	const int32 EnumValue = Enum->GetValueByIndex(OutIndex);

	if (EnumValue == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEnumAction: Enum Value not found for index %i."), OutIndex);
		OutEnumValue = 0;
		return false;
	}

	OutEnumValue = (uint8)EnumValue;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEnum: %s\nSize: [%i]\nValue: [%s]\nIndex: [%i]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*Enum->GetName(),
			Enum->NumEnums() - 1,
			*Enum->GetDisplayNameTextByValue(OutEnumValue).ToString(),
			OutIndex);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetBitmaskAction(
	int32& OutBitmaskValue, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const UEnum* Enum, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("GetBitmaskAction: Enum is nullptr."));
		OutBitmaskValue = 0;
		return false;
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("GetBitmaskAction: Too many values in Enum to use as Bitmask (%i)."), Enum->NumEnums() - 1);
		OutBitmaskValue = 0;
		return false;
	}

	int32 EnumValueNum;
	if (!GetInclusiveDiscreteActionNum(EnumValueNum, Object, Element, Tag))
	{
		OutBitmaskValue = 0;
		return false;
	}

	if (EnumValueNum > Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("GetBitmaskAction: Too many values for Enum '%s'. Expected %i or less, got %i."), *Enum->GetName(), Enum->NumEnums() - 1, EnumValueNum);
		OutBitmaskValue = 0;
		return false;
	}

	TArray<int32, TInlineAllocator<32>> OutIndices;
	OutIndices.SetNumUninitialized(EnumValueNum);
	if (!GetInclusiveDiscreteActionToArrayView(OutIndices, Object, Element, Tag))
	{
		OutBitmaskValue = 0;
		return false;
	}

	OutBitmaskValue = 0;
	for (const int32 OutIndex : OutIndices)
	{
		OutBitmaskValue |= (1 << OutIndex);
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		FString ValuesString;
		FString IndicesString;

		for (int32 EnumIdx = 0; EnumIdx < Enum->NumEnums() - 1; EnumIdx++)
		{
			if (OutBitmaskValue & (1 << EnumIdx))
			{
				ValuesString += Enum->GetDisplayNameTextByIndex(EnumIdx).ToString() + TEXT(" ");
				IndicesString += FString::FromInt(EnumIdx) + TEXT(" ");
			}
		}

		ValuesString = ValuesString.TrimEnd();
		IndicesString = IndicesString.TrimEnd();

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEnum: %s\nSize: [%i]\nValues: [%s]\nIndices: [%s]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*Enum->GetName(),
			Enum->NumEnums() - 1,
			*ValuesString,
			*IndicesString);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetOptionalAction(ELearningAgentsOptionalAction& OutOption, FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	FName OutName = NAME_None;
	if (!GetExclusiveUnionAction(OutName, OutElement, Object, Element, Tag))
	{
		OutOption = ELearningAgentsOptionalAction::Null;
		return false;
	}

	OutOption = OutName == TEXT("Null") ? ELearningAgentsOptionalAction::Null : ELearningAgentsOptionalAction::Valid;
	return true;
}

bool ULearningAgentsActions::GetEitherAction(ELearningAgentsEitherAction& OutEither, FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	FName OutName = NAME_None;
	if (!GetExclusiveUnionAction(OutName, OutElement, Object, Element, Tag))
	{
		OutEither = ELearningAgentsEitherAction::A;
		return false;
	}

	OutEither = OutName == TEXT("A") ? ELearningAgentsEitherAction::A : ELearningAgentsEitherAction::B;
	return true;
}

bool ULearningAgentsActions::GetEncodingAction(FLearningAgentsActionObjectElement& OutElement, const ULearningAgentsActionObject* Object, const FLearningAgentsActionObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEncodingAction: Object is nullptr."));
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	if (!Object->ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetEncodingAction: Invalid Action Object."));
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	if (Object->ActionObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetEncodingAction: Action tag does not match. Action is '%s' but asked for '%s'."), *Object->ActionObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Encoding)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEncodingAction: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*Object->ActionObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(Object->ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Encoding));
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	OutElement = { Object->ActionObject.GetEncoding(Element.ObjectElement).Element };

	return true;
}

bool ULearningAgentsActions::GetBoolAction(
	bool& bOutValue, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	int32 OutIndex = 0;
	if (!GetExclusiveDiscreteAction(OutIndex, Object, Element, Tag))
	{
		bOutValue = false;
		return false;
	}

	bOutValue = OutIndex == 1;
	
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nValue: [%s]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			bOutValue ? TEXT("true") : TEXT("false"));
	}
#endif
	
	return true;
}

bool ULearningAgentsActions::GetFloatAction(
	float& OutValue, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!GetContinuousActionToArrayView(MakeArrayView(&OutValue, 1), Object, Element, Tag))
	{
		OutValue = 0.0f;
		return false;
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nValue: [% 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			OutValue);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetLocationAction(
	FVector& OutLocation, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FTransform RelativeTransform, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Object, Element, Tag))
	{
		OutLocation = FVector::ZeroVector;
		return false;
	}

	const FVector LocalLocation = FVector(OutValues[0], OutValues[1], OutValues[2]);
	OutLocation = RelativeTransform.TransformPosition(LocalLocation);
	
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_LOCATION(VisualLoggerObject, LogLearning, Display,
			OutLocation,
			10,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			OutLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Location: [% 6.1f % 6.1f % 6.1f]\nLocation: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalLocation.X, LocalLocation.Y, LocalLocation.Z,
			OutLocation.X, OutLocation.Y, OutLocation.Z);
	}
#endif
	
	return true;
}

bool ULearningAgentsActions::GetRotationAction(
	FRotator& OutRotation, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FRotator RelativeRotation, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerRotationLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	FQuat OutRotationQuat;
	if (!GetRotationActionAsQuat(OutRotationQuat, Object, Element, FQuat::MakeFromRotator(RelativeRotation), Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerRotationLocation, VisualLoggerLocation, VisualLoggerColor))
	{
		OutRotation = FRotator::ZeroRotator;
		return false;
	}

	OutRotation = OutRotationQuat.Rotator();
	return true;
}

bool ULearningAgentsActions::GetRotationActionAsQuat(
	FQuat& OutRotation, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FQuat RelativeRotation, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerRotationLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Object, Element, Tag))
	{
		OutRotation = FQuat::Identity;
		return false;
	}

	const FVector LocalRotationVector = FVector(FMath::DegreesToRadians(OutValues[0]), FMath::DegreesToRadians(OutValues[1]), FMath::DegreesToRadians(OutValues[2]));
	const FQuat LocalRotation = FQuat::MakeFromRotationVector(LocalRotationVector);
	OutRotation = RelativeRotation * LocalRotation;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			VisualLoggerRotationLocation,
			LocalRotation.Rotator(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Rotation Vector: [% 6.1f % 6.1f % 6.1f]\nLocal Rotation: [% 6.1f % 6.1f % 6.1f % 6.1f]\nRotation: [% 6.1f % 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalRotationVector.X, LocalRotationVector.Y, LocalRotationVector.Z,
			LocalRotation.X, LocalRotation.Y, LocalRotation.Z, LocalRotation.W,
			OutRotation.X, OutRotation.Y, OutRotation.Z, OutRotation.W);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetScaleAction(
	FVector& OutScale, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FVector RelativeScale, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Object, Element, Tag))
	{
		OutScale = FVector::OneVector;
		return false;
	}

	const FVector LocalScaleVector = UE::Learning::Agents::Action::Private::VectorExp(FVector(OutValues[0], OutValues[1], OutValues[2]));
	OutScale = RelativeScale * LocalScaleVector;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Scale: [% 6.1f % 6.1f % 6.1f]\nScale: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalScaleVector.X, LocalScaleVector.Y, LocalScaleVector.Z,
			OutScale.X, OutScale.Y, OutScale.Z);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetTransformAction(
	FTransform& OutTransform, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FTransform RelativeTransform, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	TStaticArray<FName, 3> OutElementNames;
	TStaticArray<FLearningAgentsActionObjectElement, 3> OutElements;
	if (!GetStructActionToArrayViews(OutElementNames, OutElements, Object, Element, Tag))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	FVector OutLocation;
	if (!GetLocationAction(OutLocation, Object, OutElements[MakeArrayView(OutElementNames).Find(TEXT("Location"))], RelativeTransform))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	FQuat OutRotation;
	if (!GetRotationActionAsQuat(OutRotation, Object, OutElements[MakeArrayView(OutElementNames).Find(TEXT("Rotation"))], RelativeTransform.GetRotation()))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	FVector OutScale;
	if (!GetScaleAction(OutScale, Object, OutElements[MakeArrayView(OutElementNames).Find(TEXT("Scale"))], RelativeTransform.GetScale3D()))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	OutTransform = FTransform(OutRotation, OutLocation, OutScale);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			OutLocation,
			OutRotation,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocation: [% 6.1f % 6.1f % 6.1f]\nRotation: [% 6.1f % 6.1f % 6.1f % 6.1f]\nScale: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			OutLocation.X, OutLocation.Y, OutLocation.Z,
			OutRotation.X, OutRotation.Y, OutRotation.Z, OutRotation.W,
			OutScale.X, OutScale.Y, OutScale.Z);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetAngleAction(
	float& OutAngle, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const float RelativeAngle, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerAngleLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	float LocalAngle = 0.0f;
	if (!GetContinuousActionToArrayView(MakeArrayView(&LocalAngle, 1), Object, Element, Tag))
	{
		OutAngle = 0.0f;
		return false;
	}

	OutAngle = RelativeAngle + LocalAngle;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ANGLE_DEGREES(VisualLoggerObject, LogLearning, Display,
			OutAngle,
			0.0f,
			VisualLoggerAngleLocation,
			10.0f,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Angle: [% 6.1f]\nAngle: [% 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalAngle,
			OutAngle);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetAngleActionRadians(
	float& OutAngle, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const float RelativeAngle, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerAngleLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!GetAngleAction(OutAngle, Object, Element, FMath::RadiansToDegrees(RelativeAngle), Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerAngleLocation, VisualLoggerLocation, VisualLoggerColor))
	{
		OutAngle = 0.0f;
		return false;
	}

	OutAngle = FMath::DegreesToRadians(OutAngle);
	return true;
}

bool ULearningAgentsActions::GetVelocityAction(
	FVector& OutVelocity, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FTransform RelativeTransform, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerVelocityLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Object, Element, Tag))
	{
		OutVelocity = FVector::OneVector;
		return false;
	}

	const FVector LocalVelocity = FVector(OutValues[0], OutValues[1], OutValues[2]);
	OutVelocity = RelativeTransform.TransformVector(LocalVelocity);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ARROW(VisualLoggerObject, LogLearning, Display,
			VisualLoggerVelocityLocation,
			VisualLoggerVelocityLocation + OutVelocity,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocal Velocity: [% 6.1f % 6.1f % 6.1f]\nVelocity: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			LocalVelocity.X, LocalVelocity.Y, LocalVelocity.Z,
			OutVelocity.X, OutVelocity.Y, OutVelocity.Z);
	}
#endif

	return true;
}

bool ULearningAgentsActions::GetDirectionAction(
	FVector& OutDirection, 
	const ULearningAgentsActionObject* Object, 
	const FLearningAgentsActionObjectElement Element, 
	const FTransform RelativeTransform, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerDirectionLocation,
	const FVector VisualLoggerLocation,
	const float VisualLoggerArrowLength,
	const FLinearColor VisualLoggerColor)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Object, Element, Tag))
	{
		OutDirection = FVector::ForwardVector;
		return false;
	}

	const FVector LocalDirection = FVector(OutValues[0], OutValues[1], OutValues[2]).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	OutDirection = RelativeTransform.TransformVectorNoScale(LocalDirection);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ARROW(VisualLoggerObject, LogLearning, Display,
			VisualLoggerDirectionLocation,
			VisualLoggerDirectionLocation + VisualLoggerArrowLength * OutDirection,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEncoded: [% 6.2f % 6.2f % 6.2f]\nLocal Direction: [% 6.1f % 6.1f % 6.1f]\nDirection: [% 6.1f % 6.1f % 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			OutValues[0], OutValues[1], OutValues[2],
			LocalDirection.X, LocalDirection.Y, LocalDirection.Z,
			OutDirection.X, OutDirection.Y, OutDirection.Z);
	}
#endif

	return true;
}
