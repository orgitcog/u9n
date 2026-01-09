// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningObservation.h"
#include "LearningRandom.h"

#include "NNERuntimeBasicCpuBuilder.h"

namespace UE::Learning::Observation
{
	namespace Private
	{
		static inline bool ContainsDuplicates(const TArrayView<const FName> ElementNames)
		{
			TSet<FName, DefaultKeyFuncs<FName>, TInlineSetAllocator<32>> ElementNameSet;
			ElementNameSet.Append(ElementNames);
			return ElementNames.Num() != ElementNameSet.Num();
		}

		static inline bool CheckAllValid(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			for (const FSchemaElement SubElement : Elements)
			{
				if (!Schema.IsValid(SubElement)) { return false; }
			}
			return true;
		}

		static inline int32 GetMaxObservationVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size = FMath::Max(Size, Schema.GetObservationVectorSize(SubElement));
			}
			return Size;
		}

		static inline int32 GetTotalObservationVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size += Schema.GetObservationVectorSize(SubElement);
			}
			return Size;
		}

		static inline int32 GetTotalEncodedObservationVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size += Schema.GetEncodedVectorSize(SubElement);
			}
			return Size;
		}

		static inline bool CheckAllValid(const FObject& Object, const TArrayView<const FObjectElement> Elements)
		{
			for (const FObjectElement SubElement : Elements)
			{
				if (!Object.IsValid(SubElement)) { return false; }
			}
			return true;
		}
	}

	FSchemaElement FSchema::CreateNull(const FName Tag)
	{
		const int32 Index = Types.Add(EType::Null);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(0);
		EncodedVectorSizes.Add(0);
		TypeDataIndices.Add(INDEX_NONE);

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateContinuous(const FSchemaContinuousParameters Parameters, const FName Tag)
	{
		check(Parameters.Num >= 0);
		check(Parameters.Scale >= 0.0f);

		FContinuousData ElementData;
		ElementData.Num = Parameters.Num;
		ElementData.Scale = Parameters.Scale;

		const int32 Index = Types.Add(EType::Continuous);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Parameters.Num);
		EncodedVectorSizes.Add(Parameters.Num);
		TypeDataIndices.Add(ContinuousData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateDiscreteExclusive(const FSchemaDiscreteExclusiveParameters Parameters, const FName Tag)
	{
		FDiscreteExclusiveData ElementData;
		ElementData.Num = Parameters.Num;

		const int32 Index = Types.Add(EType::DiscreteExclusive);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Parameters.Num);
		EncodedVectorSizes.Add(Parameters.Num);
		TypeDataIndices.Add(DiscreteExclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateDiscreteInclusive(const FSchemaDiscreteInclusiveParameters Parameters, const FName Tag)
	{
		FDiscreteInclusiveData ElementData;
		ElementData.Num = Parameters.Num;

		const int32 Index = Types.Add(EType::DiscreteInclusive);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Parameters.Num);
		EncodedVectorSizes.Add(Parameters.Num);
		TypeDataIndices.Add(DiscreteInclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateNamedDiscreteExclusive(const FSchemaNamedDiscreteExclusiveParameters Parameters, const FName Tag)
	{
		check(!Private::ContainsDuplicates(Parameters.ElementNames));

		FNamedDiscreteExclusiveData ElementData;
		ElementData.Num = Parameters.ElementNames.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();

		SubElementNames.Append(Parameters.ElementNames);
		for (int32 Idx = 0; Idx < ElementData.Num; Idx++) { SubElementObjects.Add(FSchemaElement()); }

		const int32 Index = Types.Add(EType::NamedDiscreteExclusive);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Parameters.ElementNames.Num());
		EncodedVectorSizes.Add(Parameters.ElementNames.Num());
		TypeDataIndices.Add(NamedDiscreteExclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateNamedDiscreteInclusive(const FSchemaNamedDiscreteInclusiveParameters Parameters, const FName Tag)
	{
		check(!Private::ContainsDuplicates(Parameters.ElementNames));

		FNamedDiscreteInclusiveData ElementData;
		ElementData.Num = Parameters.ElementNames.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();

		SubElementNames.Append(Parameters.ElementNames);
		for (int32 Idx = 0; Idx < ElementData.Num; Idx++) { SubElementObjects.Add(FSchemaElement()); }

		const int32 Index = Types.Add(EType::NamedDiscreteInclusive);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Parameters.ElementNames.Num());
		EncodedVectorSizes.Add(Parameters.ElementNames.Num());
		TypeDataIndices.Add(NamedDiscreteInclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateAnd(const FSchemaAndParameters Parameters, const FName Tag)
	{
		check(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		check(!Private::ContainsDuplicates(Parameters.ElementNames));
		check(Private::CheckAllValid(*this, Parameters.Elements));

		FAndData ElementData;
		ElementData.Num = Parameters.Elements.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		const int32 Index = Types.Add(EType::And);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Private::GetTotalObservationVectorSize(*this, Parameters.Elements));
		EncodedVectorSizes.Add(Private::GetTotalEncodedObservationVectorSize(*this, Parameters.Elements));
		TypeDataIndices.Add(AndData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateOrExclusive(const FSchemaOrExclusiveParameters Parameters, const FName Tag)
	{
		check(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		check(!Private::ContainsDuplicates(Parameters.ElementNames));
		check(Private::CheckAllValid(*this, Parameters.Elements));

		FOrExclusiveData ElementData;
		ElementData.Num = Parameters.Elements.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();
		ElementData.EncodingSize = Parameters.EncodingSize;

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		const int32 Index = Types.Add(EType::OrExclusive);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Private::GetMaxObservationVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		EncodedVectorSizes.Add(Parameters.EncodingSize + Parameters.Elements.Num());
		TypeDataIndices.Add(OrExclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateOrInclusive(const FSchemaOrInclusiveParameters Parameters, const FName Tag)
	{
		check(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		check(!Private::ContainsDuplicates(Parameters.ElementNames));
		check(Private::CheckAllValid(*this, Parameters.Elements));

		FOrInclusiveData ElementData;
		ElementData.Num = Parameters.Elements.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();
		ElementData.AttentionEncodingSize = Parameters.AttentionEncodingSize;
		ElementData.AttentionHeadNum = Parameters.AttentionHeadNum;
		ElementData.ValueEncodingSize = Parameters.ValueEncodingSize;

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		const int32 Index = Types.Add(EType::OrInclusive);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Private::GetTotalObservationVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		EncodedVectorSizes.Add(Parameters.AttentionHeadNum * Parameters.ValueEncodingSize + Parameters.Elements.Num());
		TypeDataIndices.Add(OrInclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateArray(const FSchemaArrayParameters Parameters, const FName Tag)
	{
		check(Parameters.Num >= 0);
		check(IsValid(Parameters.Element));

		FArrayData ElementData;
		ElementData.Num = Parameters.Num;
		ElementData.ElementIndex = SubElementObjects.Num();

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		const int32 Index = Types.Add(EType::Array);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(GetObservationVectorSize(Parameters.Element) * Parameters.Num);
		EncodedVectorSizes.Add(GetEncodedVectorSize(Parameters.Element) * Parameters.Num);
		TypeDataIndices.Add(ArrayData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateSet(const FSchemaSetParameters Parameters, const FName Tag)
	{
		check(IsValid(Parameters.Element));

		FSetData ElementData;
		ElementData.MaxNum = Parameters.MaxNum;
		ElementData.ElementIndex = SubElementObjects.Num();
		ElementData.AttentionEncodingSize = Parameters.AttentionEncodingSize;
		ElementData.AttentionHeadNum = Parameters.AttentionHeadNum;
		ElementData.ValueEncodingSize = Parameters.ValueEncodingSize;

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		const int32 Index = Types.Add(EType::Set);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(GetObservationVectorSize(Parameters.Element) * Parameters.MaxNum + Parameters.MaxNum);
		EncodedVectorSizes.Add(Parameters.ValueEncodingSize * Parameters.AttentionHeadNum + 1);
		TypeDataIndices.Add(SetData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateEncoding(const FSchemaEncodingParameters Parameters, const FName Tag)
	{
		check(IsValid(Parameters.Element));

		FEncodingData ElementData;
		ElementData.ElementIndex = SubElementObjects.Num();
		ElementData.EncodingSize = Parameters.EncodingSize;
		ElementData.LayerNum = Parameters.LayerNum;
		ElementData.ActivationFunction = Parameters.ActivationFunction;

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		const int32 Index = Types.Add(EType::Encoding);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(GetObservationVectorSize(Parameters.Element));
		EncodedVectorSizes.Add(Parameters.EncodingSize);
		TypeDataIndices.Add(EncodingData.Add(ElementData));

		return { Index, Generation };
	}

	bool FSchema::IsValid(const FSchemaElement Element) const
	{
		return Element.Generation == Generation && Element.Index != INDEX_NONE;
	}

	EType FSchema::GetType(const FSchemaElement Element) const
	{
		check(IsValid(Element));
		return Types[Element.Index];
	}

	FName FSchema::GetTag(const FSchemaElement Element) const
	{
		check(IsValid(Element));
		return Tags[Element.Index];
	}

	int32 FSchema::GetObservationVectorSize(const FSchemaElement Element) const
	{
		check(IsValid(Element));
		return ObservationVectorSizes[Element.Index];
	}

	int32 FSchema::GetEncodedVectorSize(const FSchemaElement Element) const
	{
		check(IsValid(Element));
		return EncodedVectorSizes[Element.Index];
	}

	FSchemaContinuousParameters FSchema::GetContinuous(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::Continuous);
		const FContinuousData& ElementData = ContinuousData[TypeDataIndices[Element.Index]];

		FSchemaContinuousParameters Parameters;
		Parameters.Num = ElementData.Num;
		Parameters.Scale = ElementData.Scale;
		return Parameters;
	}

	FSchemaDiscreteExclusiveParameters FSchema::GetDiscreteExclusive(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::DiscreteExclusive);
		const FDiscreteExclusiveData& ElementData = DiscreteExclusiveData[TypeDataIndices[Element.Index]];

		FSchemaDiscreteExclusiveParameters Parameters;
		Parameters.Num = ElementData.Num;
		return Parameters;
	}

	FSchemaDiscreteInclusiveParameters FSchema::GetDiscreteInclusive(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::DiscreteInclusive);
		const FDiscreteInclusiveData& ElementData = DiscreteInclusiveData[TypeDataIndices[Element.Index]];

		FSchemaDiscreteInclusiveParameters Parameters;
		Parameters.Num = ElementData.Num;
		return Parameters;
	}

	FSchemaNamedDiscreteExclusiveParameters FSchema::GetNamedDiscreteExclusive(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::NamedDiscreteExclusive);
		const FNamedDiscreteExclusiveData& ElementData = NamedDiscreteExclusiveData[TypeDataIndices[Element.Index]];

		FSchemaNamedDiscreteExclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		return Parameters;
	}

	FSchemaNamedDiscreteInclusiveParameters FSchema::GetNamedDiscreteInclusive(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::NamedDiscreteInclusive);
		const FNamedDiscreteInclusiveData& ElementData = NamedDiscreteInclusiveData[TypeDataIndices[Element.Index]];

		FSchemaNamedDiscreteInclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		return Parameters;
	}

	FSchemaAndParameters FSchema::GetAnd(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::And);
		const FAndData& ElementData = AndData[TypeDataIndices[Element.Index]];

		FSchemaAndParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.Elements = TArrayView<const FSchemaElement>(SubElementObjects.GetData() + ElementData.ElementsOffset, ElementData.Num);
		return Parameters;
	}

	FSchemaOrExclusiveParameters FSchema::GetOrExclusive(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::OrExclusive);
		const FOrExclusiveData& ElementData = OrExclusiveData[TypeDataIndices[Element.Index]];

		FSchemaOrExclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.Elements = TArrayView<const FSchemaElement>(SubElementObjects.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.EncodingSize = ElementData.EncodingSize;
		return Parameters;
	}

	FSchemaOrInclusiveParameters FSchema::GetOrInclusive(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::OrInclusive);
		const FOrInclusiveData& ElementData = OrInclusiveData[TypeDataIndices[Element.Index]];

		FSchemaOrInclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.Elements = TArrayView<const FSchemaElement>(SubElementObjects.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.AttentionEncodingSize = ElementData.AttentionEncodingSize;
		Parameters.AttentionHeadNum = ElementData.AttentionHeadNum;
		Parameters.ValueEncodingSize = ElementData.ValueEncodingSize;
		return Parameters;
	}

	FSchemaArrayParameters FSchema::GetArray(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::Array);
		const FArrayData& ElementData = ArrayData[TypeDataIndices[Element.Index]];

		FSchemaArrayParameters Parameters;
		Parameters.Num = ElementData.Num;
		Parameters.Element = SubElementObjects[ElementData.ElementIndex];
		return Parameters;
	}

	FSchemaSetParameters FSchema::GetSet(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::Set);
		const FSetData& ElementData = SetData[TypeDataIndices[Element.Index]];

		FSchemaSetParameters Parameters;
		Parameters.MaxNum = ElementData.MaxNum;
		Parameters.Element = SubElementObjects[ElementData.ElementIndex];
		Parameters.AttentionEncodingSize = ElementData.AttentionEncodingSize;
		Parameters.AttentionHeadNum = ElementData.AttentionHeadNum;
		Parameters.ValueEncodingSize = ElementData.ValueEncodingSize;
		return Parameters;
	}

	FSchemaEncodingParameters FSchema::GetEncoding(const FSchemaElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::Encoding);
		const FEncodingData& ElementData = EncodingData[TypeDataIndices[Element.Index]];

		FSchemaEncodingParameters Parameters;
		Parameters.Element = SubElementObjects[ElementData.ElementIndex];
		Parameters.EncodingSize = ElementData.EncodingSize;
		Parameters.LayerNum = ElementData.LayerNum;
		Parameters.ActivationFunction = ElementData.ActivationFunction;
		return Parameters;
	}

	uint32 FSchema::GetGeneration() const
	{
		return Generation;
	}

	void FSchema::Empty()
	{
		Types.Empty();
		Tags.Empty();
		ObservationVectorSizes.Empty();
		EncodedVectorSizes.Empty();
		TypeDataIndices.Empty();

		ContinuousData.Empty();
		DiscreteExclusiveData.Empty();
		DiscreteInclusiveData.Empty();
		NamedDiscreteExclusiveData.Empty();
		NamedDiscreteInclusiveData.Empty();	
		AndData.Empty();
		OrExclusiveData.Empty();
		OrInclusiveData.Empty();
		ArrayData.Empty();
		SetData.Empty();
		EncodingData.Empty();

		SubElementNames.Empty();
		SubElementObjects.Empty();

		Generation++;
	}

	bool FSchema::IsEmpty() const
	{
		return Types.IsEmpty();
	}

	void FSchema::Reset()
	{
		Types.Reset();
		Tags.Reset();
		ObservationVectorSizes.Reset();
		EncodedVectorSizes.Reset();
		TypeDataIndices.Reset();

		ContinuousData.Reset();
		DiscreteExclusiveData.Reset();
		DiscreteInclusiveData.Reset();
		NamedDiscreteExclusiveData.Reset();
		NamedDiscreteInclusiveData.Reset();			
		AndData.Reset();
		OrExclusiveData.Reset();
		OrInclusiveData.Reset();
		ArrayData.Reset();
		SetData.Reset();
		EncodingData.Reset();

		SubElementNames.Reset();
		SubElementObjects.Reset();

		Generation++;
	}

	FObjectElement FObject::CreateNull(const FName Tag)
	{
		const int32 Index = Types.Add(EType::Null);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(0);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateContinuous(const FObjectContinuousParameters Parameters, const FName Tag)
	{
		const int32 Index = Types.Add(EType::Continuous);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(Parameters.Values.Num());

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(0);

		ContinuousValues.Append(Parameters.Values);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateDiscreteExclusive(const FObjectDiscreteExclusiveParameters Parameters, const FName Tag)
	{
		const int32 Index = Types.Add(EType::DiscreteExclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(1);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(0);

		DiscreteValues.Add(Parameters.DiscreteIndex);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateDiscreteInclusive(const FObjectDiscreteInclusiveParameters Parameters, const FName Tag)
	{
		const int32 Index = Types.Add(EType::DiscreteInclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(Parameters.DiscreteIndices.Num());

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(0);

		DiscreteValues.Append(Parameters.DiscreteIndices);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateNamedDiscreteExclusive(const FObjectNamedDiscreteExclusiveParameters Parameters, const FName Tag)
	{
		const int32 Index = Types.Add(EType::NamedDiscreteExclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(1);

		SubElementNames.Add(Parameters.ElementName);
		SubElementObjects.Add(FObjectElement());

		return { Index, Generation };
	}

	FObjectElement FObject::CreateNamedDiscreteInclusive(const FObjectNamedDiscreteInclusiveParameters Parameters, const FName Tag)
	{
		check(!Private::ContainsDuplicates(Parameters.ElementNames));

		const int32 Index = Types.Add(EType::NamedDiscreteInclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.ElementNames.Num());

		SubElementNames.Append(Parameters.ElementNames);
		for (int32 Idx = 0; Idx < Parameters.ElementNames.Num(); Idx++) { SubElementObjects.Add(FObjectElement()); }

		return { Index, Generation };
	}

	FObjectElement FObject::CreateAnd(const FObjectAndParameters Parameters, const FName Tag)
	{
		check(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		check(!Private::ContainsDuplicates(Parameters.ElementNames));
		check(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::And);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.Elements.Num());

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateOrExclusive(const FObjectOrExclusiveParameters Parameters, const FName Tag)
	{
		check(IsValid(Parameters.Element));

		const int32 Index = Types.Add(EType::OrExclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(1);

		SubElementNames.Add(Parameters.ElementName);
		SubElementObjects.Add(Parameters.Element);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateOrInclusive(const FObjectOrInclusiveParameters Parameters, const FName Tag)
	{
		check(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		check(!Private::ContainsDuplicates(Parameters.ElementNames));
		check(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::OrInclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.Elements.Num());

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateArray(const FObjectArrayParameters Parameters, const FName Tag)
	{
		check(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::Array);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.Elements.Num());

		for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
		{
			SubElementNames.Add(NAME_Name);
		}
		SubElementObjects.Append(Parameters.Elements);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateSet(const FObjectSetParameters Parameters, const FName Tag)
	{
		check(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::Set);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.Elements.Num());

		for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
		{
			SubElementNames.Add(NAME_Name);
		}
		SubElementObjects.Append(Parameters.Elements);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateEncoding(const FObjectEncodingParameters Parameters, const FName Tag)
	{
		check(IsValid(Parameters.Element));

		const int32 Index = Types.Add(EType::Encoding);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(1);

		SubElementNames.Add(NAME_Name);
		SubElementObjects.Add(Parameters.Element);

		return { Index, Generation };
	}

	bool FObject::IsValid(const FObjectElement Element) const
	{
		return Element.Generation == Generation && Element.Index != INDEX_NONE;
	}

	EType FObject::GetType(const FObjectElement Element) const
	{
		check(IsValid(Element));
		return Types[Element.Index];
	}

	FName FObject::GetTag(const FObjectElement Element) const
	{
		check(IsValid(Element));
		return Tags[Element.Index];
	}

	FObjectContinuousParameters FObject::GetContinuous(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::Continuous);

		FObjectContinuousParameters Parameters;
		Parameters.Values = TArrayView<const float>(ContinuousValues.GetData() + ContinuousDataOffsets[Element.Index], ContinuousDataNums[Element.Index]);
		return Parameters;
	}

	FObjectDiscreteExclusiveParameters FObject::GetDiscreteExclusive(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::DiscreteExclusive);

		FObjectDiscreteExclusiveParameters Parameters;
		Parameters.DiscreteIndex = DiscreteValues[DiscreteDataOffsets[Element.Index]];
		return Parameters;
	}

	FObjectDiscreteInclusiveParameters FObject::GetDiscreteInclusive(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::DiscreteInclusive);

		FObjectDiscreteInclusiveParameters Parameters;
		Parameters.DiscreteIndices = TArrayView<const int32>(DiscreteValues.GetData() + DiscreteDataOffsets[Element.Index], DiscreteDataNums[Element.Index]);
		return Parameters;
	}

	FObjectNamedDiscreteExclusiveParameters FObject::GetNamedDiscreteExclusive(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::NamedDiscreteExclusive);

		FObjectNamedDiscreteExclusiveParameters Parameters;
		Parameters.ElementName = SubElementNames[SubElementDataOffsets[Element.Index]];
		return Parameters;
	}

	FObjectNamedDiscreteInclusiveParameters FObject::GetNamedDiscreteInclusive(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::NamedDiscreteInclusive);

		FObjectNamedDiscreteInclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectAndParameters FObject::GetAnd(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::And);

		FObjectAndParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectOrExclusiveParameters FObject::GetOrExclusive(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::OrExclusive);

		FObjectOrExclusiveParameters Parameters;
		Parameters.ElementName = SubElementNames[SubElementDataOffsets[Element.Index]];
		Parameters.Element = SubElementObjects[SubElementDataOffsets[Element.Index]];
		return Parameters;
	}

	FObjectOrInclusiveParameters FObject::GetOrInclusive(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::OrInclusive);

		FObjectOrInclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectArrayParameters FObject::GetArray(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::Array);

		FObjectArrayParameters Parameters;
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectSetParameters FObject::GetSet(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::Set);

		FObjectSetParameters Parameters;
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectEncodingParameters FObject::GetEncoding(const FObjectElement Element) const
	{
		check(IsValid(Element) && GetType(Element) == EType::Encoding);

		FObjectEncodingParameters Parameters;
		Parameters.Element = SubElementObjects[SubElementDataOffsets[Element.Index]];
		return Parameters;
	}

	uint32 FObject::GetGeneration() const
	{
		return Generation;
	}

	void FObject::Empty()
	{
		Types.Empty();
		Tags.Empty();

		ContinuousDataOffsets.Empty();
		ContinuousDataNums.Empty();

		DiscreteDataOffsets.Reset();
		DiscreteDataNums.Reset();

		SubElementDataOffsets.Empty();
		SubElementDataNums.Empty();

		ContinuousValues.Empty();
		DiscreteValues.Empty();
		SubElementNames.Empty();
		SubElementObjects.Empty();

		Generation++;
	}

	bool FObject::IsEmpty() const
	{
		return Types.IsEmpty();
	}

	void FObject::Reset()
	{
		Types.Reset();
		Tags.Reset();

		ContinuousDataOffsets.Reset();
		ContinuousDataNums.Reset();

		DiscreteDataOffsets.Reset();
		DiscreteDataNums.Reset();

		SubElementDataOffsets.Reset();
		SubElementDataNums.Reset();

		ContinuousValues.Reset();
		DiscreteValues.Reset();
		SubElementNames.Reset();
		SubElementObjects.Reset();

		Generation++;
	}

	namespace Private
	{
		static inline NNE::RuntimeBasic::FModelBuilder::EActivationFunction GetNNEActivationFunction(const EEncodingActivationFunction ActivationFunction)
		{
			switch (ActivationFunction)
			{
			case EEncodingActivationFunction::ReLU: return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ReLU;
			case EEncodingActivationFunction::ELU: return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ELU;
			case EEncodingActivationFunction::TanH: return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::TanH;
			case EEncodingActivationFunction::GELU: return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::GELU;
			default: checkNoEntry(); return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ReLU;
			}
		}

		static inline int32 HashFNameStable(const FName Name)
		{
			const FString NameString = Name.ToString().ToLower();
			return (int32)CityHash32(
				(const char*)NameString.GetCharArray().GetData(), 
				NameString.GetCharArray().GetTypeSize() * 
				NameString.GetCharArray().Num());
		}

		static inline int32 HashInt(const int32 Int)
		{
			return (int32)CityHash32((const char*)&Int, sizeof(int32));
		}

		static inline int32 HashCombine(const TArrayView<const int32> Hashes)
		{
			return (int32)CityHash32((const char*)Hashes.GetData(), Hashes.Num() * Hashes.GetTypeSize());
		}

		static inline int32 HashElements(
			const FSchema& Schema,
			const TArrayView<const FName> SchemaElementNames,
			const int32 Salt)
		{
			// Note: Here we xor all entries together. 
			// This makes the hash in invariant to the ordering of names which is actually what we want 
			// since this array is representing a set-like structure and it is fine to pass elements in a different order.

			int32 Hash = 0x5592716a;
			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaElementNames.Num(); SchemaElementIdx++)
			{
				Hash ^= HashFNameStable(SchemaElementNames[SchemaElementIdx]);
			}

			return Hash;
		}

		static inline int32 HashElements(
			const FSchema& Schema,
			const TArrayView<const FName> SchemaElementNames,
			const TArrayView<const FSchemaElement> SchemaElements,
			const int32 Salt)
		{
			// Note: Here we xor all entries together. 
			// This makes the hash in invariant to the ordering of pairs of names and elements 
			// which is actually what we want since these two arrays are representing a map-like 
			// structure and it is fine to pass keys and values in a different order.

			int32 Hash = 0x5b3bbe4d;
			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaElements.Num(); SchemaElementIdx++)
			{
				Hash ^= HashCombine({ HashFNameStable(SchemaElementNames[SchemaElementIdx]), GetSchemaObjectsCompatibilityHash(Schema, SchemaElements[SchemaElementIdx], Salt) });
			}

			return Hash;
		}
	}

	int32 GetSchemaObjectsCompatibilityHash(
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const int32 Salt)
	{
		check(Schema.IsValid(SchemaElement));
		const EType SchemaElementType = Schema.GetType(SchemaElement);

		const int32 Hash = Private::HashCombine({ Salt, Private::HashInt((int32)SchemaElementType) });

		switch (SchemaElementType)
		{
		case EType::Null: return Hash;

		case EType::Continuous: return Private::HashCombine({ Hash, Private::HashInt(Schema.GetContinuous(SchemaElement).Num) });

		case EType::DiscreteExclusive: return Private::HashCombine({ Hash, Private::HashInt(Schema.GetDiscreteExclusive(SchemaElement).Num) });
		case EType::DiscreteInclusive: return Private::HashCombine({ Hash, Private::HashInt(Schema.GetDiscreteInclusive(SchemaElement).Num) });

		case EType::NamedDiscreteExclusive:
		{
			const FSchemaNamedDiscreteExclusiveParameters Parameters = Schema.GetNamedDiscreteExclusive(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashElements(Schema, Parameters.ElementNames, Salt) });
		}

		case EType::NamedDiscreteInclusive:
		{
			const FSchemaNamedDiscreteInclusiveParameters Parameters = Schema.GetNamedDiscreteInclusive(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashElements(Schema, Parameters.ElementNames, Salt) });
		}

		case EType::And:
		{
			const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashElements(Schema, Parameters.ElementNames, Parameters.Elements, Salt) });
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashElements(Schema, Parameters.ElementNames, Parameters.Elements, Salt) });
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashElements(Schema, Parameters.ElementNames, Parameters.Elements, Salt) });
		}

		case EType::Array:
		{
			const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashInt(Parameters.Num), GetSchemaObjectsCompatibilityHash(Schema, Parameters.Element, Salt) });
		}

		case EType::Set:
		{
			const FSchemaSetParameters Parameters = Schema.GetSet(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashInt(Parameters.MaxNum), GetSchemaObjectsCompatibilityHash(Schema, Parameters.Element, Salt) });
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters Parameters = Schema.GetEncoding(SchemaElement);
			return GetSchemaObjectsCompatibilityHash(Schema, Parameters.Element, Salt);
		}

		default:
		{
			checkNoEntry();
			return 0;
		}

		}
	}

	bool AreSchemaObjectsCompatible(
		const FSchema& SchemaA,
		const FSchemaElement SchemaElementA,
		const FSchema& SchemaB,
		const FSchemaElement SchemaElementB)
	{
		check(SchemaA.IsValid(SchemaElementA));
		check(SchemaB.IsValid(SchemaElementB));

		const EType SchemaElementTypeA = SchemaA.GetType(SchemaElementA);
		const EType SchemaElementTypeB = SchemaB.GetType(SchemaElementB);
		
		// If any element is an encoding element we forward the comparison to the sub-element since encoding elements don't affect compatibility
		if (SchemaElementTypeA == EType::Encoding) { return AreSchemaObjectsCompatible(SchemaA, SchemaA.GetEncoding(SchemaElementA).Element, SchemaB, SchemaElementB); }
		if (SchemaElementTypeB == EType::Encoding) { return AreSchemaObjectsCompatible(SchemaA, SchemaElementA, SchemaB, SchemaB.GetEncoding(SchemaElementB).Element); }
		
		// Otherwise if types don't match we immediately know elements are incompatible
		if (SchemaElementTypeA != SchemaElementTypeB) { return false; }

		// This is an early-out since if the input sizes are different we are definitely incompatible
		if (SchemaA.GetObservationVectorSize(SchemaElementA) != SchemaB.GetObservationVectorSize(SchemaElementB)) { return false; }

		switch (SchemaElementTypeA)
		{
		case EType::Null: return true;

		case EType::Continuous: return SchemaA.GetContinuous(SchemaElementA).Num == SchemaB.GetContinuous(SchemaElementB).Num;
		
		case EType::DiscreteExclusive: return SchemaA.GetDiscreteExclusive(SchemaElementA).Num == SchemaB.GetDiscreteExclusive(SchemaElementB).Num;
		case EType::DiscreteInclusive: return SchemaA.GetDiscreteInclusive(SchemaElementA).Num == SchemaB.GetDiscreteInclusive(SchemaElementB).Num;

		case EType::NamedDiscreteExclusive:
		{
			const FSchemaNamedDiscreteExclusiveParameters ParametersA = SchemaA.GetNamedDiscreteExclusive(SchemaElementA);
			const FSchemaNamedDiscreteExclusiveParameters ParametersB = SchemaB.GetNamedDiscreteExclusive(SchemaElementB);

			if (ParametersA.ElementNames.Num() != ParametersB.ElementNames.Num()) { return false; }

			for (int32 SchemaElementAIdx = 0; SchemaElementAIdx < ParametersA.ElementNames.Num(); SchemaElementAIdx++)
			{
				const int32 SchemaElementBIdx = ParametersB.ElementNames.Find(ParametersA.ElementNames[SchemaElementAIdx]);
				if (SchemaElementBIdx == INDEX_NONE) { return false; }
			}

			return true;
		}

		case EType::NamedDiscreteInclusive:
		{
			const FSchemaNamedDiscreteInclusiveParameters ParametersA = SchemaA.GetNamedDiscreteInclusive(SchemaElementA);
			const FSchemaNamedDiscreteInclusiveParameters ParametersB = SchemaB.GetNamedDiscreteInclusive(SchemaElementB);

			if (ParametersA.ElementNames.Num() != ParametersB.ElementNames.Num()) { return false; }

			for (int32 SchemaElementAIdx = 0; SchemaElementAIdx < ParametersA.ElementNames.Num(); SchemaElementAIdx++)
			{
				const int32 SchemaElementBIdx = ParametersB.ElementNames.Find(ParametersA.ElementNames[SchemaElementAIdx]);
				if (SchemaElementBIdx == INDEX_NONE) { return false; }
			}

			return true;
		}

		case EType::And:
		{
			const FSchemaAndParameters ParametersA = SchemaA.GetAnd(SchemaElementA);
			const FSchemaAndParameters ParametersB = SchemaB.GetAnd(SchemaElementB);

			if (ParametersA.Elements.Num() != ParametersB.Elements.Num()) { return false; }

			for (int32 SchemaElementAIdx = 0; SchemaElementAIdx < ParametersA.Elements.Num(); SchemaElementAIdx++)
			{
				const int32 SchemaElementBIdx = ParametersB.ElementNames.Find(ParametersA.ElementNames[SchemaElementAIdx]);
				if (SchemaElementBIdx == INDEX_NONE) { return false; }
				if (!AreSchemaObjectsCompatible(SchemaA, ParametersA.Elements[SchemaElementAIdx], SchemaB, ParametersB.Elements[SchemaElementBIdx])) { return false; }
			}

			return true;
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters ParametersA = SchemaA.GetOrExclusive(SchemaElementA);
			const FSchemaOrExclusiveParameters ParametersB = SchemaB.GetOrExclusive(SchemaElementB);

			if (ParametersA.Elements.Num() != ParametersB.Elements.Num()) { return false; }

			for (int32 SchemaElementAIdx = 0; SchemaElementAIdx < ParametersA.Elements.Num(); SchemaElementAIdx++)
			{
				const int32 SchemaElementBIdx = ParametersB.ElementNames.Find(ParametersA.ElementNames[SchemaElementAIdx]);
				if (SchemaElementBIdx == INDEX_NONE) { return false; }
				if (!AreSchemaObjectsCompatible(SchemaA, ParametersA.Elements[SchemaElementAIdx], SchemaB, ParametersB.Elements[SchemaElementBIdx])) { return false; }
			}

			return true;
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters ParametersA = SchemaA.GetOrInclusive(SchemaElementA);
			const FSchemaOrInclusiveParameters ParametersB = SchemaB.GetOrInclusive(SchemaElementB);

			if (ParametersA.Elements.Num() != ParametersB.Elements.Num()) { return false; }

			for (int32 SchemaElementAIdx = 0; SchemaElementAIdx < ParametersA.Elements.Num(); SchemaElementAIdx++)
			{
				const int32 SchemaElementBIdx = ParametersB.ElementNames.Find(ParametersA.ElementNames[SchemaElementAIdx]);
				if (SchemaElementBIdx == INDEX_NONE) { return false; }
				if (!AreSchemaObjectsCompatible(SchemaA, ParametersA.Elements[SchemaElementAIdx], SchemaB, ParametersB.Elements[SchemaElementBIdx])) { return false; }
			}

			return true;
		}

		case EType::Array:
		{
			const FSchemaArrayParameters ParametersA = SchemaA.GetArray(SchemaElementA);
			const FSchemaArrayParameters ParametersB = SchemaB.GetArray(SchemaElementB);

			return (ParametersA.Num == ParametersB.Num) && AreSchemaObjectsCompatible(SchemaA, ParametersA.Element, SchemaB, ParametersB.Element);
		}

		case EType::Set:
		{
			const FSchemaSetParameters ParametersA = SchemaA.GetSet(SchemaElementA);
			const FSchemaSetParameters ParametersB = SchemaB.GetSet(SchemaElementB);

			return (ParametersA.MaxNum == ParametersB.MaxNum) && AreSchemaObjectsCompatible(SchemaA, ParametersA.Element, SchemaB, ParametersB.Element);
		}

		case EType::Encoding:
		{
			checkf(false, TEXT("Encoding elements should always be forwarded..."));
			return false;
		}

		default:
		{
			checkNoEntry();
			return false;
		}

		}
	}

	void MakeEncoderNetworkModelBuilderElementFromSchema(
		NNE::RuntimeBasic::FModelBuilderElement& OutElement,
		NNE::RuntimeBasic::FModelBuilder& Builder,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const FNetworkSettings& NetworkSettings)
	{
		const EType SchemaElementType = Schema.GetType(SchemaElement);

		switch (SchemaElementType)
		{
		case EType::Null:
		{
			OutElement = Builder.MakeCopy(0);
			break;
		}

		case EType::Continuous:
		{
			const int32 ValueNum = Schema.GetContinuous(SchemaElement).Num;

			OutElement = Builder.MakeDenormalize(
				ValueNum,
				Builder.MakeValuesZero(ValueNum),
				Builder.MakeValuesOne(ValueNum));
			break;
		}

		case EType::DiscreteExclusive:
		{
			const int32 ValueNum = Schema.GetDiscreteExclusive(SchemaElement).Num;

			OutElement = Builder.MakeDenormalize(
				ValueNum,
				Builder.MakeValuesZero(ValueNum),
				Builder.MakeValuesOne(ValueNum));
			break;
		}

		case EType::DiscreteInclusive:
		{
			const int32 ValueNum = Schema.GetDiscreteInclusive(SchemaElement).Num;

			OutElement = Builder.MakeDenormalize(
				ValueNum,
				Builder.MakeValuesZero(ValueNum),
				Builder.MakeValuesOne(ValueNum));
			break;
		}

		case EType::NamedDiscreteExclusive:
		{
			const int32 ValueNum = Schema.GetNamedDiscreteExclusive(SchemaElement).ElementNames.Num();

			OutElement = Builder.MakeDenormalize(
				ValueNum,
				Builder.MakeValuesZero(ValueNum),
				Builder.MakeValuesOne(ValueNum));
			break;
		}

		case EType::NamedDiscreteInclusive:
		{
			const int32 ValueNum = Schema.GetNamedDiscreteInclusive(SchemaElement).ElementNames.Num();

			OutElement = Builder.MakeDenormalize(
				ValueNum,
				Builder.MakeValuesZero(ValueNum),
				Builder.MakeValuesOne(ValueNum));
			break;
		}

		case EType::And:
		{
			const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);

			TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderLayers;
			BuilderLayers.Reserve(Parameters.Elements.Num());
			for (const FSchemaElement SubElement : Parameters.Elements)
			{
				NNE::RuntimeBasic::FModelBuilderElement BuilderSubElement;
				MakeEncoderNetworkModelBuilderElementFromSchema(BuilderSubElement, Builder, Schema, SubElement, NetworkSettings);
				BuilderLayers.Emplace(BuilderSubElement);
			}

			OutElement = Builder.MakeConcat(BuilderLayers);
			break;
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);

			TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderSubLayers;
			TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderEncoders;
			BuilderSubLayers.Reserve(Parameters.Elements.Num());
			BuilderEncoders.Reserve(Parameters.Elements.Num());
			for (const FSchemaElement SubElement : Parameters.Elements)
			{
				const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(SubElement);
				NNE::RuntimeBasic::FModelBuilderElement BuilderSubElement;
				MakeEncoderNetworkModelBuilderElementFromSchema(BuilderSubElement, Builder, Schema, SubElement, NetworkSettings);
				BuilderSubLayers.Emplace(BuilderSubElement);

				NNE::RuntimeBasic::FModelBuilder::FLinearLayerSettings LinearLayerSettings;
				LinearLayerSettings.Type = NetworkSettings.bUseCompressedLinearLayers ?
					NNE::RuntimeBasic::FModelBuilder::ELinearLayerType::Compressed :
					NNE::RuntimeBasic::FModelBuilder::ELinearLayerType::Normal;

				switch (NetworkSettings.WeightInitialization)
				{
				case EWeightInitialization::KaimingGaussian: LinearLayerSettings.WeightInitializationSettings.Type = 
					NNE::RuntimeBasic::FModelBuilder::EWeightInitializationType::KaimingGaussian; break;
				case EWeightInitialization::KaimingUniform: LinearLayerSettings.WeightInitializationSettings.Type =
					NNE::RuntimeBasic::FModelBuilder::EWeightInitializationType::KaimingUniform; break;
				default: checkNoEntry();
				}

				BuilderEncoders.Emplace(Builder.MakeLinearLayer(SubElementEncodedSize, Parameters.EncodingSize, LinearLayerSettings));
			}

			OutElement = Builder.MakeAggregateOrExclusive(Parameters.EncodingSize, BuilderSubLayers, BuilderEncoders);
			break;
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);

			TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderSubLayers;
			TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderQueryLayers;
			TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderKeyLayers;
			TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderValueLayers;
			BuilderSubLayers.Reserve(Parameters.Elements.Num());
			BuilderQueryLayers.Reserve(Parameters.Elements.Num());
			BuilderValueLayers.Reserve(Parameters.Elements.Num());
			for (const FSchemaElement SubElement : Parameters.Elements)
			{
				const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(SubElement);
				NNE::RuntimeBasic::FModelBuilderElement BuilderSubElement;
				MakeEncoderNetworkModelBuilderElementFromSchema(BuilderSubElement, Builder, Schema, SubElement, NetworkSettings);
				BuilderSubLayers.Emplace(BuilderSubElement);

				NNE::RuntimeBasic::FModelBuilder::FLinearLayerSettings LinearLayerSettings;
				LinearLayerSettings.Type = NetworkSettings.bUseCompressedLinearLayers ?
					NNE::RuntimeBasic::FModelBuilder::ELinearLayerType::Compressed :
					NNE::RuntimeBasic::FModelBuilder::ELinearLayerType::Normal;

				switch (NetworkSettings.WeightInitialization)
				{
				case EWeightInitialization::KaimingGaussian: LinearLayerSettings.WeightInitializationSettings.Type = 
					NNE::RuntimeBasic::FModelBuilder::EWeightInitializationType::KaimingGaussian; break;
				case EWeightInitialization::KaimingUniform: LinearLayerSettings.WeightInitializationSettings.Type =
					NNE::RuntimeBasic::FModelBuilder::EWeightInitializationType::KaimingUniform; break;
				default: checkNoEntry();
				}

				BuilderQueryLayers.Emplace(Builder.MakeLinearLayer(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.AttentionEncodingSize, LinearLayerSettings));
				BuilderKeyLayers.Emplace(Builder.MakeLinearLayer(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.AttentionEncodingSize, LinearLayerSettings));
				BuilderValueLayers.Emplace(Builder.MakeLinearLayer(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.ValueEncodingSize, LinearLayerSettings));
			}

			OutElement = Builder.MakeAggregateOrInclusive(
				Parameters.ValueEncodingSize,
				Parameters.AttentionEncodingSize,
				Parameters.AttentionHeadNum,
				BuilderSubLayers,
				BuilderQueryLayers,
				BuilderKeyLayers,
				BuilderValueLayers);

			break;
		}

		case EType::Array:
		{
			const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);

			NNE::RuntimeBasic::FModelBuilderElement BuilderSubElement;
			MakeEncoderNetworkModelBuilderElementFromSchema(BuilderSubElement, Builder, Schema, Parameters.Element, NetworkSettings);
			OutElement = Builder.MakeArray(Parameters.Num, BuilderSubElement);
			break;
		}

		case EType::Set:
		{
			const FSchemaSetParameters Parameters = Schema.GetSet(SchemaElement);

			const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(Parameters.Element);

			NNE::RuntimeBasic::FModelBuilderElement BuilderSubElement;
			MakeEncoderNetworkModelBuilderElementFromSchema(BuilderSubElement, Builder, Schema, Parameters.Element, NetworkSettings);

			NNE::RuntimeBasic::FModelBuilder::FLinearLayerSettings LinearLayerSettings;
			LinearLayerSettings.Type = NetworkSettings.bUseCompressedLinearLayers ?
				NNE::RuntimeBasic::FModelBuilder::ELinearLayerType::Compressed :
				NNE::RuntimeBasic::FModelBuilder::ELinearLayerType::Normal;

			switch (NetworkSettings.WeightInitialization)
			{
			case EWeightInitialization::KaimingGaussian: LinearLayerSettings.WeightInitializationSettings.Type = 
				NNE::RuntimeBasic::FModelBuilder::EWeightInitializationType::KaimingGaussian; break;
			case EWeightInitialization::KaimingUniform: LinearLayerSettings.WeightInitializationSettings.Type =
				NNE::RuntimeBasic::FModelBuilder::EWeightInitializationType::KaimingUniform; break;
			default: checkNoEntry();
			}

			OutElement = Builder.MakeAggregateSet(
				Parameters.MaxNum,
				Parameters.ValueEncodingSize,
				Parameters.AttentionEncodingSize,
				Parameters.AttentionHeadNum,
				BuilderSubElement,
				Builder.MakeLinearLayer(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.AttentionEncodingSize, LinearLayerSettings),
				Builder.MakeLinearLayer(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.AttentionEncodingSize, LinearLayerSettings),
				Builder.MakeLinearLayer(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.ValueEncodingSize, LinearLayerSettings));

			break;
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters Parameters = Schema.GetEncoding(SchemaElement);

			const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(Parameters.Element);

			NNE::RuntimeBasic::FModelBuilderElement BuilderSubElement;
			MakeEncoderNetworkModelBuilderElementFromSchema(BuilderSubElement, Builder, Schema, Parameters.Element, NetworkSettings);

			NNE::RuntimeBasic::FModelBuilder::FLinearLayerSettings LinearLayerSettings;
			LinearLayerSettings.Type = NetworkSettings.bUseCompressedLinearLayers ?
				NNE::RuntimeBasic::FModelBuilder::ELinearLayerType::Compressed :
				NNE::RuntimeBasic::FModelBuilder::ELinearLayerType::Normal;

			switch (NetworkSettings.WeightInitialization)
			{
			case EWeightInitialization::KaimingGaussian: LinearLayerSettings.WeightInitializationSettings.Type = 
				NNE::RuntimeBasic::FModelBuilder::EWeightInitializationType::KaimingGaussian; break;
			case EWeightInitialization::KaimingUniform: LinearLayerSettings.WeightInitializationSettings.Type =
				NNE::RuntimeBasic::FModelBuilder::EWeightInitializationType::KaimingUniform; break;
			default: checkNoEntry();
			}

			OutElement = Builder.MakeSequence({
				BuilderSubElement,
				Builder.MakeMLP(
					SubElementEncodedSize,
					Parameters.EncodingSize,
					Parameters.EncodingSize,
					Parameters.LayerNum + 1, // Add 1 to account for input layer
					Private::GetNNEActivationFunction(Parameters.ActivationFunction),
					true,
					LinearLayerSettings)
				});

			break;
		}

		default:
		{
			checkNoEntry();
		}
		}

		checkf(OutElement.GetInputSize() == Schema.GetObservationVectorSize(SchemaElement),
			TEXT("Encoder Network Input unexpected size for %s. Got %i, expected %i according to Schema."),
			*Schema.GetTag(SchemaElement).ToString(), OutElement.GetInputSize(), Schema.GetObservationVectorSize(SchemaElement));

		checkf(OutElement.GetOutputSize() == Schema.GetEncodedVectorSize(SchemaElement),
			TEXT("Encoder Network Output unexpected size for %s. Got %i, expected %i according to Schema."),
			*Schema.GetTag(SchemaElement).ToString(), OutElement.GetOutputSize(), Schema.GetEncodedVectorSize(SchemaElement));
	}

	void GenerateEncoderNetworkFileDataFromSchema(
		TArray<uint8>& OutFileData,
		uint32& OutInputSize,
		uint32& OutOutputSize,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const FNetworkSettings& NetworkSettings,
		const uint32 Seed)
	{
		check(Schema.IsValid(SchemaElement));

		NNE::RuntimeBasic::FModelBuilder Builder(Seed);
		NNE::RuntimeBasic::FModelBuilderElement Element;
		MakeEncoderNetworkModelBuilderElementFromSchema(Element, Builder, Schema, SchemaElement, NetworkSettings);
		Builder.WriteFileDataAndReset(OutFileData, OutInputSize, OutOutputSize, Element);
	}

	void SetVectorFromObject(
		TLearningArrayView<1, float> OutObservationVector,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const FObject& Object,
		const FObjectElement ObjectElement)
	{
		check(Schema.IsValid(SchemaElement));
		check(Object.IsValid(ObjectElement));
		check(OutObservationVector.Num() == Schema.GetObservationVectorSize(SchemaElement));

		// Check that the types match

		const EType SchemaElementType = Schema.GetType(SchemaElement);
		const EType ObjectElementType = Object.GetType(ObjectElement);
		check(ObjectElementType == SchemaElementType);

		// Zero Observation Vector

		Array::Zero(OutObservationVector);

		// Logic for each specific element type

		switch (SchemaElementType)
		{

		case EType::Null: return;

		case EType::Continuous:
		{
			// Check the input sizes match

			const FSchemaContinuousParameters SchemaParameters = Schema.GetContinuous(SchemaElement);

			const TArrayView<const float> ObservationValues = Object.GetContinuous(ObjectElement).Values;
			check(Schema.GetObservationVectorSize(SchemaElement) == ObservationValues.Num());
			check(Schema.GetObservationVectorSize(SchemaElement) == OutObservationVector.Num());
			check(Schema.GetObservationVectorSize(SchemaElement) == SchemaParameters.Num);

			// Copy in and scale the values from the observation object

			const int32 ValueNum = SchemaParameters.Num;
			const float ValueScale = FMath::Max(SchemaParameters.Scale, UE_SMALL_NUMBER);

			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				OutObservationVector[ValueIdx] = ObservationValues[ValueIdx] / ValueScale;
			}

			return;
		}

		case EType::DiscreteExclusive:
		{
			const int32 ObservationValue = Object.GetDiscreteExclusive(ObjectElement).DiscreteIndex;
			check(Schema.GetObservationVectorSize(SchemaElement) > ObservationValue && ObservationValue >= 0);
			check(Schema.GetObservationVectorSize(SchemaElement) == OutObservationVector.Num());

			// Set the single value in the observation vector

			OutObservationVector[ObservationValue] = 1.0f;
			return;
		}

		case EType::DiscreteInclusive:
		{
			const TArrayView<const int32> ObservationValues = Object.GetDiscreteInclusive(ObjectElement).DiscreteIndices;
			check(Schema.GetObservationVectorSize(SchemaElement) >= ObservationValues.Num());
			check(Schema.GetObservationVectorSize(SchemaElement) == OutObservationVector.Num());

			// Set values in the observation vector

			for (int32 ObservationValueIdx = 0; ObservationValueIdx < ObservationValues.Num(); ObservationValueIdx++)
			{
				check(Schema.GetObservationVectorSize(SchemaElement) > ObservationValues[ObservationValueIdx] && ObservationValues[ObservationValueIdx] >= 0);
				OutObservationVector[ObservationValues[ObservationValueIdx]] = 1.0f;
			}

			return;
		}

		case EType::NamedDiscreteExclusive:
		{
			const TArrayView<const FName> SchemaNames = Schema.GetNamedDiscreteExclusive(SchemaElement).ElementNames;
			const FName ObservationValue = Object.GetNamedDiscreteExclusive(ObjectElement).ElementName;
			check(Schema.GetObservationVectorSize(SchemaElement) == OutObservationVector.Num());

			// Set the single value in the observation vector
			const int32 ObservationIndex = SchemaNames.Find(ObservationValue);
			check(ObservationIndex != INDEX_NONE);
			OutObservationVector[ObservationIndex] = 1.0f;
			return;
		}

		case EType::NamedDiscreteInclusive:
		{
			const TArrayView<const FName> SchemaNames = Schema.GetNamedDiscreteInclusive(SchemaElement).ElementNames;
			const TArrayView<const FName> ObservationValues = Object.GetNamedDiscreteInclusive(ObjectElement).ElementNames;
			check(Schema.GetObservationVectorSize(SchemaElement) >= ObservationValues.Num());
			check(Schema.GetObservationVectorSize(SchemaElement) == OutObservationVector.Num());

			// Set values in the observation vector
			for (int32 ObservationValueIdx = 0; ObservationValueIdx < ObservationValues.Num(); ObservationValueIdx++)
			{
				const int32 ObservationIndex = SchemaNames.Find(ObservationValues[ObservationValueIdx]);
				check(ObservationIndex != INDEX_NONE);
				OutObservationVector[ObservationIndex] = 1.0f;
			}
			return;
		}

		case EType::And:
		{
			// Check the number of sub-elements match

			const FSchemaAndParameters SchemaParameters = Schema.GetAnd(SchemaElement);
			const FObjectAndParameters ObjectParameters = Object.GetAnd(ObjectElement);
			check(SchemaParameters.Elements.Num() == ObjectParameters.Elements.Num());

			// Update Sub-elements

			int32 SubElementOffset = 0;

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 ObjectElementIndex = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);
				check(ObjectElementIndex != INDEX_NONE);

				const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Elements[SchemaElementIdx]);

				SetVectorFromObject(
					OutObservationVector.Slice(SubElementOffset, SubElementSize),
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Object,
					ObjectParameters.Elements[ObjectElementIndex]);

				SubElementOffset += SubElementSize;
			}

			check(SubElementOffset == OutObservationVector.Num());
			return;
		}

		case EType::OrExclusive:
		{
			// Check only one sub-element is given and index is valid

			const FSchemaOrExclusiveParameters SchemaParameters = Schema.GetOrExclusive(SchemaElement);
			const FObjectOrExclusiveParameters ObjectParameters = Object.GetOrExclusive(ObjectElement);

			const int32 SchemaElementIndex = SchemaParameters.ElementNames.Find(ObjectParameters.ElementName);
			check(SchemaElementIndex != INDEX_NONE);

			// Update sub-element

			const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Elements[SchemaElementIndex]);

			SetVectorFromObject(
				OutObservationVector.Slice(0, SubElementSize),
				Schema,
				SchemaParameters.Elements[SchemaElementIndex],
				Object,
				ObjectParameters.Element);

			// Set Mask

			const int32 MaxSubElementSize = Private::GetMaxObservationVectorSize(Schema, SchemaParameters.Elements);

			OutObservationVector[MaxSubElementSize + SchemaElementIndex] = 1.0f;

			check(OutObservationVector.Num() == MaxSubElementSize + SchemaParameters.Elements.Num());
			return;
		}

		case EType::OrInclusive:
		{
			// Check all indices are in range

			const FSchemaOrInclusiveParameters SchemaParameters = Schema.GetOrInclusive(SchemaElement);
			const FObjectOrInclusiveParameters ObjectParameters = Object.GetOrInclusive(ObjectElement);
			check(ObjectParameters.Elements.Num() <= SchemaParameters.Elements.Num());

			// Update sub-elements

			int32 SubElementOffset = 0;

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Elements[SchemaElementIdx]);
				const int32 ObjectElementIdx = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);

				if (ObjectElementIdx != INDEX_NONE)
				{
					SetVectorFromObject(
						OutObservationVector.Slice(SubElementOffset, SubElementSize),
						Schema,
						SchemaParameters.Elements[SchemaElementIdx],
						Object,
						ObjectParameters.Elements[ObjectElementIdx]);
				}

				SubElementOffset += SubElementSize;
			}

			// Set Mask

			check(SubElementOffset + SchemaParameters.Elements.Num() == OutObservationVector.Num());

			for (int32 ObjectElementIdx = 0; ObjectElementIdx < ObjectParameters.Elements.Num(); ObjectElementIdx++)
			{
				const int32 SchemaElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectElementIdx]);
				check(SchemaElementIdx != INDEX_NONE);

				OutObservationVector[SubElementOffset + SchemaElementIdx] = 1.0f;
			}

			return;
		}

		case EType::Array:
		{
			// Check number of array elements is correct

			const FSchemaArrayParameters SchemaParameters = Schema.GetArray(SchemaElement);
			const FObjectArrayParameters ObjectParameters = Object.GetArray(ObjectElement);
			check(SchemaParameters.Num == ObjectParameters.Elements.Num());

			// Update sub-elements

			const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < SchemaParameters.Num; ElementIdx++)
			{
				SetVectorFromObject(
					OutObservationVector.Slice(ElementIdx * SubElementSize, SubElementSize),
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx]);
			}

			return;
		}

		case EType::Set:
		{
			// Check number of set elements is correct

			const FSchemaSetParameters SchemaParameters = Schema.GetSet(SchemaElement);
			const FObjectSetParameters ObjectParameters = Object.GetSet(ObjectElement);
			check(SchemaParameters.MaxNum >= ObjectParameters.Elements.Num());

			// Update sub-elements

			int32 SubElementOffset = 0;

			const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < ObjectParameters.Elements.Num(); ElementIdx++)
			{
				SetVectorFromObject(
					OutObservationVector.Slice(SubElementOffset, SubElementSize),
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx]);

				SubElementOffset += SubElementSize;
			}

			SubElementOffset = SubElementSize * SchemaParameters.MaxNum;

			// Set Mask

			Array::Set(OutObservationVector.Slice(SubElementOffset, ObjectParameters.Elements.Num()), 1.0f);

			check(SubElementOffset + SchemaParameters.MaxNum == OutObservationVector.Num());
			return;
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters SchemaParameters = Schema.GetEncoding(SchemaElement);
			const FObjectEncodingParameters ObjectParameters = Object.GetEncoding(ObjectElement);

			SetVectorFromObject(
				OutObservationVector,
				Schema,
				SchemaParameters.Element,
				Object,
				ObjectParameters.Element);

			return;
		}

		default:
		{
			checkNoEntry();
			return;
		}

		}
	}

	void GetObjectFromVector(
		FObject& OutObject,
		FObjectElement& OutObjectElement,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const TLearningArrayView<1, const float> ObservationVector)
	{
		check(Schema.IsValid(SchemaElement));

		// Check that the types match

		const EType SchemaElementType = Schema.GetType(SchemaElement);
		const FName SchemaElementTag = Schema.GetTag(SchemaElement);

		// Get Observation Vector Size

		const int32 ObservationVectorSize = ObservationVector.Num();
		check(ObservationVectorSize == Schema.GetObservationVectorSize(SchemaElement));

		// Logic for each specific element type

		switch (SchemaElementType)
		{

		case EType::Null:
		{
			OutObjectElement = OutObject.CreateNull(SchemaElementTag);
			return;
		}

		case EType::Continuous:
		{
			const FSchemaContinuousParameters SchemaParameters = Schema.GetContinuous(SchemaElement);
			check(ObservationVectorSize == SchemaParameters.Num);

			const int32 ValueNum = SchemaParameters.Num;
			const float ValueScale = FMath::Max(SchemaParameters.Scale, UE_SMALL_NUMBER);

			TLearningArray<1, float, TInlineAllocator<32>> ObservationValues;
			ObservationValues.SetNumUninitialized({ ValueNum });
			for (int32 ValueIdx = 0; ValueIdx < ValueNum; ValueIdx++)
			{
				ObservationValues[ValueIdx] = ValueScale * ObservationVector[ValueIdx];
			}

			OutObjectElement = OutObject.CreateContinuous({ MakeArrayView(ObservationValues.GetData(), ObservationValues.Num()) }, SchemaElementTag);
			return;
		}
		
		case EType::DiscreteExclusive:
		{
			check(ObservationVectorSize == Schema.GetDiscreteExclusive(SchemaElement).Num);

			// Find Index
			int32 ExclusiveIndex = INDEX_NONE;
			for (int32 Idx = 0; Idx < ObservationVectorSize; Idx++)
			{
				check(ObservationVector[Idx] == 0.0f || ObservationVector[Idx] == 1.0f);
				if (ObservationVector[Idx])
				{
					ExclusiveIndex = Idx;
					break;
				}
			}
			check(ExclusiveIndex != INDEX_NONE);

			OutObjectElement = OutObject.CreateDiscreteExclusive({ ExclusiveIndex }, SchemaElementTag);
			return;
		}

		case EType::DiscreteInclusive:
		{
			check(ObservationVectorSize == Schema.GetDiscreteInclusive(SchemaElement).Num);

			// Find Indices
			TArray<int32, TInlineAllocator<8>> InclusiveIndices;
			InclusiveIndices.Reserve(ObservationVectorSize);
			for (int32 Idx = 0; Idx < ObservationVectorSize; Idx++)
			{
				check(ObservationVector[Idx] == 0.0f || ObservationVector[Idx] == 1.0f);
				if (ObservationVector[Idx])
				{
					InclusiveIndices.Add(Idx);
				}
			}

			OutObjectElement = OutObject.CreateDiscreteInclusive({ InclusiveIndices }, SchemaElementTag);
			return;
		}


		case EType::NamedDiscreteExclusive:
		{
			const TArrayView<const FName> SchemaNames = Schema.GetNamedDiscreteExclusive(SchemaElement).ElementNames;
			check(ObservationVectorSize == Schema.GetNamedDiscreteExclusive(SchemaElement).ElementNames.Num());

			// Find Name
			FName ExclusiveName = NAME_None;
			for (int32 Idx = 0; Idx < ObservationVectorSize; Idx++)
			{
				check(ObservationVector[Idx] == 0.0f || ObservationVector[Idx] == 1.0f);
				if (ObservationVector[Idx])
				{
					ExclusiveName = SchemaNames[Idx];
					break;
				}
			}
			check(ExclusiveName != NAME_None);

			OutObjectElement = OutObject.CreateNamedDiscreteExclusive({ ExclusiveName }, SchemaElementTag);
			return;
		}

		case EType::NamedDiscreteInclusive:
		{
			const TArrayView<const FName> SchemaNames = Schema.GetNamedDiscreteInclusive(SchemaElement).ElementNames;
			check(ObservationVectorSize == Schema.GetNamedDiscreteInclusive(SchemaElement).ElementNames.Num());

			// Find Names
			TArray<FName, TInlineAllocator<8>> InclusiveNames;
			InclusiveNames.Reserve(ObservationVectorSize);
			for (int32 Idx = 0; Idx < ObservationVectorSize; Idx++)
			{
				check(ObservationVector[Idx] == 0.0f || ObservationVector[Idx] == 1.0f);
				if (ObservationVector[Idx])
				{
					InclusiveNames.Add(SchemaNames[Idx]);
				}
			}

			OutObjectElement = OutObject.CreateNamedDiscreteInclusive({ InclusiveNames }, SchemaElementTag);
			return;
		}

		case EType::And:
		{
			const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);

			// Create Sub-elements

			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElements.SetNumUninitialized(Parameters.Elements.Num());

			int32 SubElementOffset = 0;
			for (int32 SchemaElementIdx = 0; SchemaElementIdx < Parameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SchemaElementIdx]);

				GetObjectFromVector(
					OutObject,
					SubElements[SchemaElementIdx],
					Schema,
					Parameters.Elements[SchemaElementIdx],
					ObservationVector.Slice(SubElementOffset, SubElementSize));

				SubElementOffset += SubElementSize;
			}
			check(SubElementOffset == ObservationVectorSize);

			OutObjectElement = OutObject.CreateAnd({ Parameters.ElementNames, SubElements }, SchemaElementTag);
			return;
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);

			// Find active element

			const int32 MaxSubElementSize = Private::GetMaxObservationVectorSize(Schema, Parameters.Elements);

			int32 SchemaElementIndex = INDEX_NONE;
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				check(ObservationVector[MaxSubElementSize + SubElementIdx] == 0.0f || ObservationVector[MaxSubElementSize + SubElementIdx] == 1.0f);
				if (ObservationVector[MaxSubElementSize + SubElementIdx])
				{
					SchemaElementIndex = SubElementIdx;
					break;
				}
			}
			check(SchemaElementIndex != INDEX_NONE);

			// Create sub-element

			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SchemaElementIndex]);

			FObjectElement SubElement;
			GetObjectFromVector(
				OutObject,
				SubElement,
				Schema,
				Parameters.Elements[SchemaElementIndex],
				ObservationVector.Slice(0, SubElementSize));

			OutObjectElement = OutObject.CreateOrExclusive({ Parameters.ElementNames[SchemaElementIndex], SubElement }, SchemaElementTag);
			return;
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);

			// Find total sub-element size

			const int32 TotalSubElementSize = Private::GetTotalObservationVectorSize(Schema, Parameters.Elements);

			// Create sub-elements

			TArray<FName, TInlineAllocator<8>> SubElementNames;
			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElementNames.Reserve(Parameters.Elements.Num());
			SubElements.Reserve(Parameters.Elements.Num());

			int32 SubElementOffset = 0;
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SubElementIdx]);

				check(
					ObservationVector[TotalSubElementSize + SubElementIdx] == 0.0f || 
					ObservationVector[TotalSubElementSize + SubElementIdx] == 1.0f);

				if (ObservationVector[TotalSubElementSize + SubElementIdx] == 1.0f)
				{
					FObjectElement SubElement;
					GetObjectFromVector(
						OutObject,
						SubElement,
						Schema,
						Parameters.Elements[SubElementIdx],
						ObservationVector.Slice(SubElementOffset, SubElementSize));

					SubElementNames.Add(Parameters.ElementNames[SubElementIdx]);
					SubElements.Add(SubElement);
				}

				SubElementOffset += SubElementSize;
			}
			check(SubElementOffset + Parameters.Elements.Num() == ObservationVectorSize);

			OutObjectElement = OutObject.CreateOrInclusive({ SubElementNames, SubElements }, SchemaElementTag);
			return;
		}

		case EType::Array:
		{
			const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);

			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElements.SetNumUninitialized(Parameters.Num);

			// Create sub-elements

			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < Parameters.Num; ElementIdx++)
			{
				GetObjectFromVector(
					OutObject,
					SubElements[ElementIdx],
					Schema,
					Parameters.Element,
					ObservationVector.Slice(ElementIdx * SubElementSize, SubElementSize));
			}

			OutObjectElement = OutObject.CreateArray({ SubElements }, SchemaElementTag);
			return;
		}

		case EType::Set:
		{
			const FSchemaSetParameters Parameters = Schema.GetSet(SchemaElement);

			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Element);

			// Create sub-elements

			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElements.Reserve(Parameters.MaxNum);

			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.MaxNum; SubElementIdx++)
			{
				check(
					ObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 0.0f || 
					ObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 1.0f);

				if (ObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 0.0f)
				{
					break;
				}

				FObjectElement SubElement;

				GetObjectFromVector(
					OutObject,
					SubElement,
					Schema,
					Parameters.Element,
					ObservationVector.Slice(SubElementIdx * SubElementSize, SubElementSize));

				SubElements.Add(SubElement);
			}

			OutObjectElement = OutObject.CreateSet({ SubElements }, SchemaElementTag);
			return;
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters Parameters = Schema.GetEncoding(SchemaElement);

			FObjectElement SubElement;
			GetObjectFromVector(
				OutObject,
				SubElement,
				Schema,
				Parameters.Element,
				ObservationVector);

			OutObjectElement = OutObject.CreateEncoding({ SubElement }, SchemaElementTag);
			return;
		}

		default:
		{
			checkNoEntry();
			OutObjectElement = FObjectElement();
			return;
		}
		}
	}

	void AddGaussianNoiseToVector(
		uint32& InOutRandomState,
		TLearningArrayView<1, float> InOutObservationVector,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const float NoiseScale)
	{
		check(Schema.IsValid(SchemaElement));

		const EType SchemaElementType = Schema.GetType(SchemaElement);

		const int32 ObservationVectorSize = InOutObservationVector.Num();
		check(ObservationVectorSize == Schema.GetObservationVectorSize(SchemaElement));

		switch (SchemaElementType)
		{

		case EType::Null:
		{
			return;
		}

		case EType::Continuous:
		{
			const FSchemaContinuousParameters SchemaParameters = Schema.GetContinuous(SchemaElement);
			check(ObservationVectorSize == SchemaParameters.Num);

			TLearningArray<1, float, TInlineAllocator<32>> NoiseValues;
			NoiseValues.SetNumUninitialized({ SchemaParameters.Num });
			Random::SampleGaussianArray(NoiseValues, InOutRandomState, 0.0f, NoiseScale);
			for (int32 ValueIdx = 0; ValueIdx < SchemaParameters.Num; ValueIdx++)
			{
				InOutObservationVector[ValueIdx] += NoiseValues[ValueIdx];
			}

			return;
		}

		case EType::DiscreteExclusive:
		{
			return;
		}

		case EType::DiscreteInclusive:
		{
			return;
		}

		case EType::NamedDiscreteExclusive:
		{
			return;
		}

		case EType::NamedDiscreteInclusive:
		{
			return;
		}

		case EType::And:
		{
			const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);

			int32 SubElementOffset = 0;
			for (int32 SchemaElementIdx = 0; SchemaElementIdx < Parameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SchemaElementIdx]);

				AddGaussianNoiseToVector(
					InOutRandomState,
					InOutObservationVector.Slice(SubElementOffset, SubElementSize),
					Schema,
					Parameters.Elements[SchemaElementIdx],
					NoiseScale);

				SubElementOffset += SubElementSize;
			}
			check(SubElementOffset == ObservationVectorSize);

			return;
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);
			const int32 MaxSubElementSize = Private::GetMaxObservationVectorSize(Schema, Parameters.Elements);

			int32 SchemaElementIndex = INDEX_NONE;
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				check(InOutObservationVector[MaxSubElementSize + SubElementIdx] == 0.0f || InOutObservationVector[MaxSubElementSize + SubElementIdx] == 1.0f);
				if (InOutObservationVector[MaxSubElementSize + SubElementIdx])
				{
					SchemaElementIndex = SubElementIdx;
					break;
				}
			}
			check(SchemaElementIndex != INDEX_NONE);

			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SchemaElementIndex]);

			AddGaussianNoiseToVector(
				InOutRandomState,
				InOutObservationVector.Slice(0, SubElementSize),
				Schema,
				Parameters.Elements[SchemaElementIndex],
				NoiseScale);

			return;
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);
			const int32 TotalSubElementSize = Private::GetTotalObservationVectorSize(Schema, Parameters.Elements);

			int32 SubElementOffset = 0;
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SubElementIdx]);

				check(
					InOutObservationVector[TotalSubElementSize + SubElementIdx] == 0.0f ||
					InOutObservationVector[TotalSubElementSize + SubElementIdx] == 1.0f);

				if (InOutObservationVector[TotalSubElementSize + SubElementIdx] == 1.0f)
				{
					AddGaussianNoiseToVector(
						InOutRandomState,
						InOutObservationVector.Slice(SubElementOffset, SubElementSize),
						Schema,
						Parameters.Elements[SubElementIdx],
						NoiseScale);
				}

				SubElementOffset += SubElementSize;
			}
			check(SubElementOffset + Parameters.Elements.Num() == ObservationVectorSize);

			return;
		}

		case EType::Array:
		{
			const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);
			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < Parameters.Num; ElementIdx++)
			{
				AddGaussianNoiseToVector(
					InOutRandomState,
					InOutObservationVector.Slice(ElementIdx* SubElementSize, SubElementSize),
					Schema,
					Parameters.Element,
					NoiseScale);
			}

			return;
		}

		case EType::Set:
		{
			const FSchemaSetParameters Parameters = Schema.GetSet(SchemaElement);
			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Element);

			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.MaxNum; SubElementIdx++)
			{
				check(
					InOutObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 0.0f ||
					InOutObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 1.0f);

				if (InOutObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 0.0f)
				{
					break;
				}

				AddGaussianNoiseToVector(
					InOutRandomState,
					InOutObservationVector.Slice(SubElementIdx * SubElementSize, SubElementSize),
					Schema,
					Parameters.Element,
					NoiseScale);
			}

			return;
		}

		case EType::Encoding:
		{
			AddGaussianNoiseToVector(
				InOutRandomState,
				InOutObservationVector,
				Schema,
				Schema.GetEncoding(SchemaElement).Element,
				NoiseScale);

			return;
		}

		default:
		{
			checkNoEntry();
			return;
		}
		}
	}

}