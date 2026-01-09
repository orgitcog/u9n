// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

#include "UObject/NameTypes.h"

namespace UE::NNE::RuntimeBasic
{
	class FModelBuilderElement;
	class FModelBuilder;
}

namespace UE::Learning::Observation
{
	/**
	 * Observation Type
	 *
	 * The core type of an observation object for which different encoding methods are defined.
	 */
	enum class EType : uint8
	{
		// Empty Observation
		Null		= 0,	

		// Vector of continuous float observations
		Continuous	= 1,	

		// Set of exclusive discrete actions
		DiscreteExclusive = 2,

		// Set of inclusive discrete actions
		DiscreteInclusive = 3,

		// Set of named exclusive discrete actions
		NamedDiscreteExclusive = 4,

		// Set of named inclusive discrete actions
		NamedDiscreteInclusive = 5,

		// Combination of multiple observations
		And			= 6,

		// Exclusive choice from a set of observations
		OrExclusive = 7,

		// Inclusive choice from a set of observations
		OrInclusive = 8,

		// Fixed sized array of observations
		Array		= 9,

		// Variable sized, unordered set of observations
		Set			= 10,

		// Encoding of another observation 
		Encoding	= 11,
	};

	/**
	 * Observation Schema Element
	 *
	 * A single element in the observation schema representing a part of an observation. Internally this consists of a index used by the schema to 
	 * look up the associated observation data and a generation id which can be used to check when this index is no longer valid.
	 */
	struct FSchemaElement { int32 Index = INDEX_NONE; uint32 Generation = INDEX_NONE; };

	struct FSchemaContinuousParameters
	{
		// Number of values in the continuous observation
		int32 Num = 0;

		// Scale factor for the continuous observation
		float Scale = 1.0f;
	};

	struct FSchemaDiscreteExclusiveParameters
	{
		// Number of values in the discrete observation
		int32 Num = 0;
	};

	struct FSchemaDiscreteInclusiveParameters
	{
		// Number of values in the discrete observation
		int32 Num = 0;
	};

	struct FSchemaNamedDiscreteExclusiveParameters
	{
		// Names of the discrete observations.
		TArrayView<const FName> ElementNames;
	};

	struct FSchemaNamedDiscreteInclusiveParameters
	{
		// Names of the discrete observations.
		TArrayView<const FName> ElementNames;
	};

	struct FSchemaAndParameters
	{
		// Names of the sub-observations.
		TArrayView<const FName> ElementNames;

		// The associated sub-observations.
		TArrayView<const FSchemaElement> Elements;
	};

	struct FSchemaOrExclusiveParameters
	{
		// Names of the sub-observations.
		TArrayView<const FName> ElementNames;

		// The associated sub-observations.
		TArrayView<const FSchemaElement> Elements;

		// The size of the encoding to use to combine these sub-observations.
		int32 EncodingSize = 128;
	};

	struct FSchemaOrInclusiveParameters
	{
		// Names of the sub-observations.
		TArrayView<const FName> ElementNames;

		// The associated sub-observations.
		TArrayView<const FSchemaElement> Elements;

		// The size of the attention embedding to use (per-attention-head) for sub-observations
		int32 AttentionEncodingSize = 16;

		// The number of attention heads to use for combining sub-observations
		int32 AttentionHeadNum = 4;

		// The size of the output encoding to use (per-attention-head) for sub-observations
		int32 ValueEncodingSize = 32;
	};

	struct FSchemaArrayParameters
	{
		// The array sub-element.
		FSchemaElement Element;

		// The number of elements in the array.
		int32 Num = 0;
	};

	struct FSchemaSetParameters
	{
		// The set sub-element.
		FSchemaElement Element;

		// The maximum number of elements in the set.
		int32 MaxNum = 0;

		// The size of the attention embedding to use (per-attention-head) for sub-observations
		int32 AttentionEncodingSize = 16;

		// The number of attention heads to use for combining sub-observations
		int32 AttentionHeadNum = 4;

		// The size of the output encoding to use (per-attention-head) for sub-observations
		int32 ValueEncodingSize = 32;
	};

	/** Activation Function to use for encoding */
	enum class EEncodingActivationFunction : uint8
	{
		ELU = 0,
		ReLU = 1,
		TanH = 2,
		GELU = 3,
	};

	struct FSchemaEncodingParameters
	{
		// The sub-element.
		FSchemaElement Element;

		// The size at which the sub-element should be encoded.
		int32 EncodingSize = 32;

		// The number of layers in the encoding
		int32 LayerNum = 1;

		// The activation function to use for encoding
		EEncodingActivationFunction ActivationFunction = EEncodingActivationFunction::ELU;
	};

	/**
	 * Observation Schema
	 *
	 * This object allows you to construct a description of the kind of observations you might want to provide as input to a policy. Internally this 
	 * object contains a pool of individual elements. This allows them to be constructed performantly and in a cache efficient way. This object is 
	 * therefore required to access any data about the individual observation elements that are created.
	 */
	struct LEARNING_API FSchema
	{
		FSchemaElement CreateNull(const FName Tag = NAME_None);
		FSchemaElement CreateContinuous(const FSchemaContinuousParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateDiscreteExclusive(const FSchemaDiscreteExclusiveParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateDiscreteInclusive(const FSchemaDiscreteInclusiveParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateNamedDiscreteExclusive(const FSchemaNamedDiscreteExclusiveParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateNamedDiscreteInclusive(const FSchemaNamedDiscreteInclusiveParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateAnd(const FSchemaAndParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateOrExclusive(const FSchemaOrExclusiveParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateOrInclusive(const FSchemaOrInclusiveParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateArray(const FSchemaArrayParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateSet(const FSchemaSetParameters Parameters, const FName Tag = NAME_None);
		FSchemaElement CreateEncoding(const FSchemaEncodingParameters Parameters, const FName Tag = NAME_None);

		FSchemaContinuousParameters GetContinuous(const FSchemaElement Element) const;
		FSchemaDiscreteExclusiveParameters GetDiscreteExclusive(const FSchemaElement Element) const;
		FSchemaDiscreteInclusiveParameters GetDiscreteInclusive(const FSchemaElement Element) const;
		FSchemaNamedDiscreteExclusiveParameters GetNamedDiscreteExclusive(const FSchemaElement Element) const;
		FSchemaNamedDiscreteInclusiveParameters GetNamedDiscreteInclusive(const FSchemaElement Element) const;
		FSchemaAndParameters GetAnd(const FSchemaElement Element) const;
		FSchemaOrExclusiveParameters GetOrExclusive(const FSchemaElement Element) const;
		FSchemaOrInclusiveParameters GetOrInclusive(const FSchemaElement Element) const;
		FSchemaArrayParameters GetArray(const FSchemaElement Element) const;
		FSchemaSetParameters GetSet(const FSchemaElement Element) const;
		FSchemaEncodingParameters GetEncoding(const FSchemaElement Element) const;

		// Checks if the given element is valid
		bool IsValid(const FSchemaElement Element) const;

		// Gets the type of the given element
		EType GetType(const FSchemaElement Element) const;

		// Gets the tag of the given element
		FName GetTag(const FSchemaElement Element) const;

		// Get the observation vector size of the given element
		int32 GetObservationVectorSize(const FSchemaElement Element) const;

		// Get the encoded vector size of the given element
		int32 GetEncodedVectorSize(const FSchemaElement Element) const;

		// Get the current generation
		uint32 GetGeneration() const;

		// Checks if the given schema is empty of elements.
		bool IsEmpty() const;

		// Empty all internal buffers of elements. This invalidates all existing elements.
		void Empty();

		// Reset all internal buffers (without freeing memory). This invalidates all existing elements.
		void Reset();

	private:

		struct FContinuousData
		{
			int32 Num = INDEX_NONE;
			float Scale = 0.0f;
		};

		struct FDiscreteExclusiveData
		{
			int32 Num = INDEX_NONE;
		};

		struct FDiscreteInclusiveData
		{
			int32 Num = INDEX_NONE;
		};

		struct FNamedDiscreteExclusiveData
		{
			int32 Num = INDEX_NONE;
			int32 ElementsOffset = INDEX_NONE;
		};

		struct FNamedDiscreteInclusiveData
		{
			int32 Num = INDEX_NONE;
			int32 ElementsOffset = INDEX_NONE;
		};

		struct FAndData
		{
			int32 Num = INDEX_NONE;
			int32 ElementsOffset = INDEX_NONE;
		};

		struct FOrExclusiveData
		{
			int32 Num = INDEX_NONE;
			int32 ElementsOffset = INDEX_NONE;
			int32 EncodingSize = INDEX_NONE;
		};

		struct FOrInclusiveData
		{
			int32 Num = INDEX_NONE;
			int32 ElementsOffset = INDEX_NONE;
			int32 AttentionEncodingSize = INDEX_NONE;
			int32 AttentionHeadNum = INDEX_NONE;
			int32 ValueEncodingSize = INDEX_NONE;
		};

		struct FArrayData
		{
			int32 Num = INDEX_NONE;
			int32 ElementIndex = INDEX_NONE;
		};

		struct FSetData
		{
			int32 MaxNum = INDEX_NONE;
			int32 ElementIndex = INDEX_NONE;
			int32 AttentionEncodingSize = INDEX_NONE;
			int32 AttentionHeadNum = INDEX_NONE;
			int32 ValueEncodingSize = INDEX_NONE;
		};

		struct FEncodingData
		{
			int32 ElementIndex = INDEX_NONE;
			int32 EncodingSize = INDEX_NONE;
			int32 LayerNum = INDEX_NONE;
			EEncodingActivationFunction ActivationFunction = EEncodingActivationFunction::ELU;
		};

		uint32 Generation = 0;

		/** These have entries for each Schema Element */
		TArray<EType> Types;
		TArray<FName> Tags;
		TArray<int32> ObservationVectorSizes;
		TArray<int32> EncodedVectorSizes;
		TArray<int32> TypeDataIndices;
		
		/** These are indexed based on the Type using TypeDataIndices */
		TArray<FContinuousData> ContinuousData;
		TArray<FDiscreteExclusiveData> DiscreteExclusiveData;
		TArray<FDiscreteInclusiveData> DiscreteInclusiveData;
		TArray<FNamedDiscreteExclusiveData> NamedDiscreteExclusiveData;
		TArray<FNamedDiscreteInclusiveData> NamedDiscreteInclusiveData;
		TArray<FAndData> AndData;
		TArray<FOrExclusiveData> OrExclusiveData;
		TArray<FOrInclusiveData> OrInclusiveData;
		TArray<FArrayData> ArrayData;
		TArray<FSetData> SetData;
		TArray<FEncodingData> EncodingData;

		/** This is an array of all the SubElements and their names, referenced by other elements */
		TArray<FName> SubElementNames;
		TArray<FSchemaElement> SubElementObjects;
	};

	/**
	 * Observation Object Element
	 *
	 * A single element in the observation object representing part of an observation. Internally this consists of a index used by the object to look 
	 * up the associated observation data and a generation id which can be used to check when this index is no longer valid.
	 */
	struct FObjectElement { int32 Index = INDEX_NONE; uint32 Generation = INDEX_NONE; };

	struct FObjectContinuousParameters
	{
		// Continuous observation values
		TArrayView<const float> Values;
	};

	struct FObjectDiscreteExclusiveParameters
	{
		// Index of the chosen observation.
		int32 DiscreteIndex;
	};

	struct FObjectDiscreteInclusiveParameters
	{
		// Indices of the chosen observations.
		TArrayView<const int32> DiscreteIndices;
	};

	struct FObjectNamedDiscreteExclusiveParameters
	{
		// Name of the chosen observation.
		FName ElementName;
	};

	struct FObjectNamedDiscreteInclusiveParameters
	{
		// Names of the chosen observations.
		TArrayView<const FName> ElementNames;
	};

	struct FObjectAndParameters
	{
		// Names of the sub-observations.
		TArrayView<const FName> ElementNames;

		// The associated sub-element.
		TArrayView<const FObjectElement> Elements;
	};

	struct FObjectOrExclusiveParameters
	{
		// Name of the chosen sub-observation.
		FName ElementName;

		// The associated chosen sub-element.
		FObjectElement Element;
	};

	struct FObjectOrInclusiveParameters
	{
		// Names of the chosen sub-observations.
		TArrayView<const FName> ElementNames;

		// The associated chosen sub-elements.
		TArrayView<const FObjectElement> Elements;
	};

	struct FObjectArrayParameters
	{
		// Array of sub-elements.
		TArrayView<const FObjectElement> Elements;
	};

	struct FObjectSetParameters
	{
		// Set of sub-elements.
		TArrayView<const FObjectElement> Elements;
	};

	struct FObjectEncodingParameters
	{
		// Encoded sub-element.
		FObjectElement Element;
	};

	/**
	 * Observation Object
	 *
	 * This object allows you to construct or get data from an instance of an observation you might give as input to a policy. Internally this object
	 * contains a pool of individual elements. This allows them to be constructed performantly and in a cache efficient way. This object is therefore
	 * required to access any data about the individual observation elements that are created.
	 */
	struct LEARNING_API FObject
	{
		FObjectElement CreateNull(const FName Tag = NAME_None);
		FObjectElement CreateContinuous(const FObjectContinuousParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateDiscreteExclusive(const FObjectDiscreteExclusiveParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateDiscreteInclusive(const FObjectDiscreteInclusiveParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateNamedDiscreteExclusive(const FObjectNamedDiscreteExclusiveParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateNamedDiscreteInclusive(const FObjectNamedDiscreteInclusiveParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateAnd(const FObjectAndParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateOrExclusive(const FObjectOrExclusiveParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateOrInclusive(const FObjectOrInclusiveParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateArray(const FObjectArrayParameters Parametes, const FName Tag = NAME_None);
		FObjectElement CreateSet(const FObjectSetParameters Parameters, const FName Tag = NAME_None);
		FObjectElement CreateEncoding(const FObjectEncodingParameters Parameters, const FName Tag = NAME_None);

		FObjectContinuousParameters GetContinuous(const FObjectElement Element) const;
		FObjectDiscreteExclusiveParameters GetDiscreteExclusive(const FObjectElement Element) const;
		FObjectDiscreteInclusiveParameters GetDiscreteInclusive(const FObjectElement Element) const;
		FObjectNamedDiscreteExclusiveParameters GetNamedDiscreteExclusive(const FObjectElement Element) const;
		FObjectNamedDiscreteInclusiveParameters GetNamedDiscreteInclusive(const FObjectElement Element) const;
		FObjectAndParameters GetAnd(const FObjectElement Element) const;
		FObjectOrExclusiveParameters GetOrExclusive(const FObjectElement Element) const;
		FObjectOrInclusiveParameters GetOrInclusive(const FObjectElement Element) const;
		FObjectArrayParameters GetArray(const FObjectElement Element) const;
		FObjectSetParameters GetSet(const FObjectElement Element) const;
		FObjectEncodingParameters GetEncoding(const FObjectElement Element) const;

		// Checks if the given element is valid
		bool IsValid(const FObjectElement Element) const;

		// Gets the type of the given element
		EType GetType(const FObjectElement Element) const;

		// Gets the tag of the given element
		FName GetTag(const FObjectElement Element) const;

		// Get the current generation
		uint32 GetGeneration() const;

		// Checks if the given object is empty of elements.
		bool IsEmpty() const;

		// Empty all internal buffers of elements. This invalidates all existing elements.
		void Empty();

		// Reset all internal buffers (without freeing memory). This invalidates all existing elements.
		void Reset();

	private:

		uint32 Generation = 0;

		TArray<EType> Types;
		TArray<FName> Tags;
		TArray<int32> ContinuousDataOffsets;
		TArray<int32> ContinuousDataNums;
		TArray<int32> DiscreteDataOffsets;
		TArray<int32> DiscreteDataNums;
		TArray<int32> SubElementDataOffsets;
		TArray<int32> SubElementDataNums;

		TArray<float> ContinuousValues;
		TArray<int32> DiscreteValues;
		TArray<FName> SubElementNames;
		TArray<FObjectElement> SubElementObjects;
	};

	/**
	 * Gets a hash value representing object compatibility between schemas i.e. if objects from one schema can be used by objects expecting another
	 * schema. This is not a cryptographic hash, and so `AreSchemaObjectsCompatible` should still be used as the ultimate source of truth.
	 * This function returns an int32 so that it can be used in blueprints.
	 *
	 * @param Schema					Observation Schema
	 * @param SchemaElement				Observation Schema Element
	 * @param Salt						Hash salt
	 */
	LEARNING_API int32 GetSchemaObjectsCompatibilityHash(
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const int32 Salt = 0x62625165);

	/**
	 * Test if two schemas are compatible i.e. if objects from one schema can be used by objects expecting another schema.
	 *
	 * @param SchemaA					First Schema
	 * @param SchemaElementA			First Schema Element
	 * @param SchemaB					Second Schema
	 * @param SchemaElementB			Second Schema Element
	 * @returns							true when the schemas are compatible
	 */
	LEARNING_API bool AreSchemaObjectsCompatible(
		const FSchema& SchemaA,
		const FSchemaElement SchemaElementA,
		const FSchema& SchemaB,
		const FSchemaElement SchemaElementB);

	/** Network weight initialization type */
	enum class EWeightInitialization : uint8
	{
		KaimingGaussian = 0,
		KaimingUniform = 1,
	};

	/**
	 * Settings for building a network from a schema.
	 */
	struct LEARNING_API FNetworkSettings
	{
		// If to use compressed linear layers. This reduces the memory usage by half at the cost of some small impact on quality and evaluation speed.
		bool bUseCompressedLinearLayers = false;

		// Which weight initialization to use.
		EWeightInitialization WeightInitialization = EWeightInitialization::KaimingGaussian;
	};

	/**
	 * Make a NNE::RuntimeBasic::FModelBuilderElement for the given Schema. This can be used if you want to plug the Encoder generated by this
	 * schema as a part of a larger model you are making with a NNE::RuntimeBasic::FModelBuilder.
	 *
	 * @param OutElement				Output Builder Element
	 * @param Builder					Model Builder
	 * @param Schema					Observation Schema
	 * @param SchemaElement				Observation Schema Element
	 * @param NetworkSettings			Network Generation Settings
	 */
	LEARNING_API void MakeEncoderNetworkModelBuilderElementFromSchema(
		NNE::RuntimeBasic::FModelBuilderElement& OutElement,
		NNE::RuntimeBasic::FModelBuilder& Builder,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const FNetworkSettings& NetworkSettings = FNetworkSettings());

	/**
	 * Generate FileData for a Neural Network that can act as a Encoder for the given schema. This network will take as input a vector of size
	 * ObservationVectorSize and produce a vector of size EncodedVectorSize.
	 *
	 * @param OutFileData				Output File Data to write to
	 * @param OutInputSize				Size of the vector this network takes as input
	 * @param OutOutputSize				Size of the vector this network takes as output
	 * @param Schema					Observation Schema
	 * @param SchemaElement				Observation Schema Element
	 * @param NetworkSettings			Network Generation Settings
	 * @param Seed						The random seed used in initializing the network weights
	 */
	LEARNING_API void GenerateEncoderNetworkFileDataFromSchema(
		TArray<uint8>& OutFileData,
		uint32& OutInputSize,
		uint32& OutOutputSize,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const FNetworkSettings& NetworkSettings = FNetworkSettings(),
		const uint32 Seed = 0x08ab1c49);

	/**
	 * Convert an observation object into an observation vector.
	 *
	 * @param OutObservationVector		Output observation vector
	 * @param Schema					Observation Schema
	 * @param SchemaElement				Observation Schema Element
	 * @param Object					Observation Object
	 * @param ObjectElement				Observation Object Element
	 */
	LEARNING_API void SetVectorFromObject(
		TLearningArrayView<1, float> OutObservationVector,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const FObject& Object,
		const FObjectElement ObjectElement);

	/**
	 * Convert an observation vector into an observation object
	 *
	 * @param OutObject					Output Observation Object
	 * @param OutObjectElement			Output Observation Object Element
	 * @param Schema					Observation Schema
	 * @param SchemaElement				Observation Schema Element
	 * @param ObservationVector			Input Observation vector
	 */
	LEARNING_API void GetObjectFromVector(
		FObject& OutObject,
		FObjectElement& OutObjectElement,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const TLearningArrayView<1, const float> ObservationVector);

	/**
	 * Add Gaussian noise to all the continuous parts of an observation vector.
	 *
	 * @param InOutRandomState			Random State/Seed to use when sampling the noise
	 * @param InOutObservationVector	Observation vector to add the noise to
	 * @param Schema					Observation Schema
	 * @param SchemaElement				Observation Schema Element
	 * @param NoiseScale				Scale of the noise to apply
	 */
	LEARNING_API void AddGaussianNoiseToVector(
		uint32& InOutRandomState,
		TLearningArrayView<1, float> InOutObservationVector,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const float NoiseScale = 1.0f);

}