// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGData.h"
#include "Compute/PCGComputeCommon.h"

#include "RHIGPUReadback.h"
#include "Misc/SpinLock.h"

#include "PCGDataForGPU.generated.h"

class FRDGPooledBuffer;
class FReferenceCollector;
class UPCGDataBinding;
class UPCGMetadata;
class UPCGPin;
class UPCGSettings;
struct FPCGContext;
enum class EPCGMetadataTypes : uint8;

enum class EPCGUnpackDataCollectionResult
{
	Success,
	DataMismatch,
	NoData
};

UENUM()
enum class EPCGKernelAttributeType : uint8
{
	Bool = 1,
	Int,
	Float,
	Float2,
	Float3,
	Float4,
	Rotator,
	Quat,
	Transform,
	StringKey,
	Name,

	Invalid = std::numeric_limits<uint8>::max() UMETA(Hidden),
};

/** Attribute name and type which uniquely identify an attribute in a compute graph. */
USTRUCT()
struct FPCGKernelAttributeKey
{
	GENERATED_BODY()

	FPCGKernelAttributeKey() = default;

	explicit FPCGKernelAttributeKey(FPCGAttributeIdentifier InIdentifier, EPCGKernelAttributeType InType)
		: Identifier(InIdentifier)
		, Type(InType)
	{}

	explicit FPCGKernelAttributeKey(const FPCGAttributePropertySelector& InSelector, EPCGKernelAttributeType InType);

	/** To be called everytime the Selector changes, to update the Identifier. Return true if it has changed. */
	bool UpdateIdentifierFromSelector();
	
	void SetSelector(const FPCGAttributePropertySelector& InSelector);

	/** Cached identifier. Need to be updated if the Selector ever change. */
	UPROPERTY()
	FPCGAttributeIdentifier Identifier;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (DisplayAfter = "Attribute"))
	EPCGKernelAttributeType Type = EPCGKernelAttributeType::Float;

	bool IsValid() const;

	bool operator==(const FPCGKernelAttributeKey& Other) const;

	friend uint32 GetTypeHash(const FPCGKernelAttributeKey& In);

protected:
	/** Selector to specify which attribute to create and on which domain. At the moment, only support `@Data` domains or no domain (default one). */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PCG_DiscardPropertySelection, PCG_DiscardExtraSelection))
	FPCGAttributePropertyOutputNoSourceSelector Name;
};

/** Table of attributes used in compute graph with helpers to get unique attribute ID used to read/write attribute in data collection buffers. */
USTRUCT()
struct FPCGKernelAttributeTable
{
	GENERATED_BODY()

	FPCGKernelAttributeTable() = default;

	int32 GetAttributeId(const FPCGKernelAttributeKey& InAttribute) const;

	int32 GetAttributeId(FPCGAttributeIdentifier InIdentifier, EPCGKernelAttributeType InType) const;

	/** Adds an attribute of given name and type. Returns index or INDEX_NONE if add failed (happens if max table size reached). */
	int32 AddAttribute(const FPCGKernelAttributeKey& Key);
	int32 AddAttribute(FPCGAttributeIdentifier InIdentifier, EPCGKernelAttributeType InType);

	int32 Num() const { return AttributeTable.Num(); }

#if PCG_KERNEL_LOGGING_ENABLED
	void DebugLog() const;
#endif

protected:
	UPROPERTY()
	TArray<FPCGKernelAttributeKey> AttributeTable;
};

/** Data description for a metadata attribute. Stores identifying name and type as well as the unique attribute ID. */
struct FPCGKernelAttributeDesc
{
public:
	FPCGKernelAttributeDesc() = default;

	explicit FPCGKernelAttributeDesc(int32 InIndex, EPCGKernelAttributeType InType, FPCGAttributeIdentifier InIdentifier)
		: AttributeId(InIndex)
		, AttributeKey(InIdentifier, InType)
	{
	}

	explicit FPCGKernelAttributeDesc(int32 InIndex, EPCGKernelAttributeType InType, FPCGAttributeIdentifier InIdentifier, const TArray<int32>* InUniqueStringKeys)
		: AttributeId(InIndex)
		, AttributeKey(InIdentifier, InType)
	{
		if (InUniqueStringKeys)
		{
			UniqueStringKeys = *InUniqueStringKeys;
		}
	}

	const TArray<int32>& GetUniqueStringKeys() const { return UniqueStringKeys; }

	void AddUniqueStringKeys(const TArray<int32>& InOtherStringKeys);

	void SetStringKeys(const TArrayView<int32>& InStringKeys);

	bool operator==(const FPCGKernelAttributeDesc& Other) const;

public:
	int32 AttributeId = INDEX_NONE;

	FPCGKernelAttributeKey AttributeKey{};

private:
	/* All possible string keys arriving on this attribute (string keys are indices into the string table in the data binding). */
	TArray<int32> UniqueStringKeys;
};

/** Data description for a single data object (UPCGData). */
struct FPCGDataDesc
{
public:
	FPCGDataDesc() = default;
	explicit FPCGDataDesc(EPCGDataType InType);
	FPCGDataDesc(EPCGDataType InType, int32 InElementCount);
	FPCGDataDesc(EPCGDataType InType, int32 InElementCount, FIntPoint InElementCount2D);
	FPCGDataDesc(const FPCGTaggedData& InTaggedData, const UPCGDataBinding* InBinding);

	uint64 ComputePackedSize() const;

	bool HasElementsMetadataDomainAttributes() const;

	bool ContainsAttribute(FPCGAttributeIdentifier InAttributeIdentifier) const;

	bool ContainsAttribute(FPCGAttributeIdentifier InAttributeIdentifier, EPCGKernelAttributeType InAttributeType) const;

	void AddAttribute(FPCGKernelAttributeKey InAttribute, const UPCGDataBinding* InBinding, const TArray<int32>* InOptionalUniqueStringKeys = nullptr);

	int32 GetElementCountForAttribute(const FPCGKernelAttributeDesc& AttributeDesc) const;

	bool IsDomain2D() const { return !!(Type & EPCGDataType::BaseTexture); }

public:
	EPCGDataType Type = EPCGDataType::Point;
	TArray<FPCGKernelAttributeDesc> AttributeDescs;
	// @todo_pcg: These counts should be merged into one FIntVector
	int32 ElementCount = 0;
	FIntPoint ElementCount2D = FIntPoint::ZeroValue;
	TArray<int32, TInlineAllocator<4>> TagStringKeys;

private:
	void InitializeAttributeDescs(const UPCGData* InData, const UPCGDataBinding* InBinding);
};

/** Data description for a data collection (FPCGDataCollection). */
struct FPCGDataCollectionDesc
{
	static FPCGDataCollectionDesc BuildFromDataCollection(
		const FPCGDataCollection& InDataCollection,
		const UPCGDataBinding* InBinding);

	/** Computes the size (in bytes) of the header portion of the packed data collection buffer. */
	uint32 ComputePackedHeaderSizeBytes() const;

	/** Computes the size (in bytes) of the data collection after packing. */
	uint64 ComputePackedSizeBytes() const;

	void WriteHeader(TArray<uint32>& OutPackedDataCollectionHeader) const;

	/** Pack a data collection into the GPU data format. DataDescs defines which attributes are packed. */
	void PackDataCollection(const FPCGDataCollection& InDataCollection, FName InPin, const UPCGDataBinding* InDataBinding, TArray<uint32>& OutPackedDataCollection) const;

	/** Unpack a buffer of 8-bit uints to a data collection. */
	EPCGUnpackDataCollectionResult UnpackDataCollection(FPCGContext* InContext, const TArray<uint8>& InPackedData, FName InPin, const TArray<FString>& InStringTable, FPCGDataCollection& OutDataCollection) const;

	/** Compute total number of processing elements of the given type. */
	uint32 ComputeDataElementCount(EPCGDataType InDataType) const;

	/** Aggregate another data description. */
	void Combine(const FPCGDataCollectionDesc& Other);

	/** Get description of first attribute with matching identifier in input data. Returns true if such an attribute found and also signals whether multiple matching attributes
	* with conflicting names are present. */
	bool GetAttributeDesc(FPCGAttributeIdentifier InAttributeIdentifier, FPCGKernelAttributeDesc& OutAttributeDesc, bool &bOutConflictingTypesFound) const;

	bool ContainsAttributeOnAnyData(FPCGAttributeIdentifier InAttributeIdentifier) const;

	/** Makes attribute present on all data. If data has existing attribute with same name then the given type will be applied. */
	void AddAttributeToAllData(FPCGKernelAttributeKey InAttribute, const UPCGDataBinding* InBinding, const TArray<int32>* InOptionalUniqueStringKeys = nullptr);

	void GetUniqueStringKeyValues(int32 InAttributeId, TArray<int32>& OutUniqueStringKeys) const;

	/** Description of each data in this data collection. */
	TArray<FPCGDataDesc> DataDescs;
};

/** A proxy for a data collection residing in a GPU buffer along with functionality to retrieve the data on the CPU. Holds onto GPU memory. */
struct FPCGProxyForGPUDataCollection : public TSharedFromThis<FPCGProxyForGPUDataCollection>
{
public:
	FPCGProxyForGPUDataCollection(TRefCountPtr<FRDGPooledBuffer> InBuffer, uint32 InBufferSizeBytes, const FPCGDataCollectionDesc& InDescription, const TArray<FString>& InStringTable);

	/** Populates a CPU data object representing the GPU data for the given index, performing a readback from GPU->CPU if required. */
	bool GetCPUData(FPCGContext* InContext, int32 InDataIndex, FPCGTaggedData& OutData);

	const TRefCountPtr<FRDGPooledBuffer>& GetBuffer() const { return Buffer; }

	uint32 GetBufferSizeBytes() const { return BufferSizeBytes; }

	const FPCGDataCollectionDesc& GetDescription() const { return Description; }

	void UpdateElementCountsFromReadback(const TArray<uint32>& InElementCounts);

	const TArray<FString>& GetStringTable() const { return StringTable; }

protected:
	/** Persistent GPU buffer that can be read back. Buffer will be freed when this ref count is 0. */
	TRefCountPtr<FRDGPooledBuffer> Buffer;

	uint32 BufferSizeBytes = 0;

	FPCGDataCollectionDesc Description;

	/** Used to comprehend string IDs in buffer. */
	TArray<FString> StringTable;

	/** Read back data. Populated once upon first readback request. */
	TArray<const FPCGTaggedData> ReadbackData;
	TArray<TStrongObjectPtr<const UPCGData>> ReadbackDataRefs;

	TSharedPtr<FRHIGPUBufferReadback> ReadbackRequest;

	TArray<uint8> RawReadbackData;

	std::atomic<bool> bReadbackDataProcessed = false;

	bool bReadbackDataArrived = false;

	UE::FSpinLock ReadbackLock;
};

namespace PCGDataForGPUHelpers
{
	/** Returns GPU type that will be used to represent the given metadata type. */
	EPCGKernelAttributeType GetAttributeTypeFromMetadataType(EPCGMetadataTypes MetadataType);

	/** Compute how attributes can be packed to custom floats. */
	void ComputeCustomFloatPacking(
		FPCGContext* InContext,
		const UPCGSettings* InSettings,
		TArray<FName>& InAttributeNames,
		const UPCGDataBinding* InBinding,
		const FPCGDataCollectionDesc* InDataCollectionDescription,
		uint32& OutCustomFloatCount,
		TArray<FUint32Vector4>& OutAttributeIdOffsetStrides);
}
