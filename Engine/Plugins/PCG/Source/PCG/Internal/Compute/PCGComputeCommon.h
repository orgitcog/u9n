// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"

#include "PCGComputeCommon.generated.h"

class UPCGComputeKernel;
class UPCGData;
class UPCGDataBinding;
class UPCGSettings;
struct FPCGContext;
struct FPCGDataCollectionDesc;
struct FPCGDataDesc;
struct FPCGPinPropertiesGPU;
struct FPCGTaggedData;

class FRHIShaderResourceView;

#define PCG_KERNEL_LOGGING_ENABLED (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING)

#if PCG_KERNEL_LOGGING_ENABLED
#define PCG_KERNEL_VALIDATION_WARN(Context, Settings, ValidationMessage) PCGComputeHelpers::LogKernelWarning(Context, Settings, ValidationMessage);
#define PCG_KERNEL_VALIDATION_ERR(Context, Settings, ValidationMessage) PCGComputeHelpers::LogKernelError(Context, Settings, ValidationMessage);
#else
#define PCG_KERNEL_VALIDATION_WARN(Context, Settings, ValidationMessage) // Log removed
#define PCG_KERNEL_VALIDATION_ERR(Context, Settings, ValidationMessage) // Log removed
#endif

/** Modes for exporting the buffer from transient to persistent for downstream consumption. */
UENUM(meta = (Bitflags))
enum class EPCGExportMode : uint8
{
	/** Buffer is transient and freed after usage. */
	NoExport = 0,
	/** Buffer will be exported and a proxy will be output from the compute graph and passed to downstream nodes. */
	ComputeGraphOutput = 1 << 0,
	/** Producer node is being inspected, read back data and store in inspection data. */
	Inspection = 1 << 1,
	/** Producer node is being debugged, read back data and execute debug visualization. */
	DebugVisualization = 1 << 2,
};
ENUM_CLASS_FLAGS(EPCGExportMode);

namespace PCGComputeConstants
{
	constexpr int MAX_NUM_ATTRS = 128;
	constexpr int NUM_RESERVED_ATTRS = 32; // Reserved for point properties, spline accessors, etc.
	constexpr int MAX_NUM_CUSTOM_ATTRS = MAX_NUM_ATTRS - NUM_RESERVED_ATTRS; // Reserved for custom attributes

	constexpr int DATA_COLLECTION_HEADER_SIZE_BYTES = 4; // 4 bytes for NumData
	constexpr int DATA_HEADER_PREAMBLE_SIZE_BYTES = 12; // 4 bytes for DataType, 4 bytes for NumAttrs, 4 bytes for NumElements
	constexpr int ATTRIBUTE_HEADER_SIZE_BYTES = 8; // 4 bytes for PackedIdAndStride, 4 bytes for data start address
	constexpr int DATA_HEADER_SIZE_BYTES = DATA_HEADER_PREAMBLE_SIZE_BYTES + MAX_NUM_ATTRS * ATTRIBUTE_HEADER_SIZE_BYTES;

	constexpr int POINT_DATA_TYPE_ID = 0;
	constexpr int PARAM_DATA_TYPE_ID = 1;

	constexpr int NUM_POINT_PROPERTIES = 9;
	constexpr int POINT_POSITION_ATTRIBUTE_ID = 0;
	constexpr int POINT_ROTATION_ATTRIBUTE_ID = 1;
	constexpr int POINT_SCALE_ATTRIBUTE_ID = 2;
	constexpr int POINT_BOUNDS_MIN_ATTRIBUTE_ID = 3;
	constexpr int POINT_BOUNDS_MAX_ATTRIBUTE_ID = 4;
	constexpr int POINT_COLOR_ATTRIBUTE_ID = 5;
	constexpr int POINT_DENSITY_ATTRIBUTE_ID = 6;
	constexpr int POINT_SEED_ATTRIBUTE_ID = 7;
	constexpr int POINT_STEEPNESS_ATTRIBUTE_ID = 8;

	constexpr uint32 KernelExecutedFlag = 1 << 31;

	constexpr uint32 MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER = 256;

	constexpr uint32 THREAD_GROUP_SIZE = 64;

	// Used to represent invalid/removed points. We use a value slightly less than max float,
	// as not all platforms support float infinity in shaders.
	constexpr float INVALID_DENSITY = 3.402823e+38f;

	constexpr TCHAR DataLabelTagPrefix[] = { TEXT("PCG_DATA_LABEL") };
}

namespace PCGComputeHelpers
{
	/** Gets the element count for a given data. E.g. number of points in a PointData, number of metadata entries in a ParamData, etc. */
	int GetElementCount(const UPCGData* InData);

	/** Gets the 2D element count for a given data. E.g. texture size for UPCGTextureData. */
	FIntPoint GetElementCount2D(const UPCGData* InData);

	/** True if 'Type' is valid on a GPU input pin. */
	bool IsTypeAllowedAsInput(EPCGDataType Type);

	/** True if 'Type' is valid on a GPU output pin. */
	bool IsTypeAllowedAsOutput(EPCGDataType Type);

	/** True if 'Type' is valid in a GPU data collection. Some types are only supported as DataInterfaces, and cannot be uploaded in data collections. */
	bool IsTypeAllowedInDataCollection(EPCGDataType Type);

	/** Whether metadata attributes should be read from the given data and registered for use in GPU graphs. */
	bool ShouldImportAttributesFromData(const UPCGData* InData);

#if PCG_KERNEL_LOGGING_ENABLED
	/** Logs a warning on a GPU node in the graph and console. */
	void LogKernelWarning(const FPCGContext* Context, const UPCGSettings* Settings, const FText& InText);

	/** Logs an error on a GPU node in the graph and console. */
	void LogKernelError(const FPCGContext* Context, const UPCGSettings* Settings, const FText& InText);
#endif

	/** Returns true if the given buffer size is dangerously large. Optionally emits error log. */
	bool IsBufferSizeTooLarge(uint64 InBufferSizeBytes, bool bInLogError = true);

	int32 GetAttributeIdFromMetadataAttributeIndex(int32 InAttributeIndex);
	int32 GetMetadataAttributeIndexFromAttributeId(int32 InAttributeId);

	/** Produces the data label prefixed with PCGComputeConstants::DataLabelTagPrefix. */
	FString GetPrefixedDataLabel(const FString& InLabel);

	/** Produces the data interface name of a data label resolver. */
	FString GetDataLabelResolverName(FName InPinLabel);

#if WITH_EDITOR
	void ConvertObjectPathToShaderFilePath(FString& InOutPath);
#endif
}

namespace PCGComputeDummies
{
	FRHIShaderResourceView* GetDummyFloatBuffer();
	FRHIShaderResourceView* GetDummyFloat2Buffer();
	FRHIShaderResourceView* GetDummyFloat4Buffer();
}

/** A by-label reference to a pin, used for wiring kernels within a node. */
struct FPCGPinReference
{
	/** Reference a pin by label only, used for referencing node pins. */
	explicit FPCGPinReference(FName InLabel)
		: Kernel(nullptr)
		, Label(InLabel)
	{
	}

	/** Reference a pin by kernel and label. */
	explicit FPCGPinReference(UPCGComputeKernel* InKernel, FName InLabel)
		: Kernel(InKernel)
		, Label(InLabel)
	{
	}

	bool operator==(const FPCGPinReference& Other) const
	{
		return Label == Other.Label
			&& Kernel == Other.Kernel;
	}

	/** Associated kernel. If null then compiler will look for pin on owning node. */
	UPCGComputeKernel* Kernel = nullptr;

	/** Pin label. */
	FName Label;
};

uint32 GetTypeHash(const FPCGPinReference& In);

/** A connection for wiring kernels within a node. */
struct FPCGKernelEdge
{
	FPCGKernelEdge(const FPCGPinReference& InUpstreamPin, const FPCGPinReference& InDownstreamPin)
		: UpstreamPin(InUpstreamPin)
		, DownstreamPin(InDownstreamPin)
	{
	}

	bool IsConnectedToNodeInput() const { return UpstreamPin.Kernel == nullptr; }
	bool IsConnectedToNodeOutput() const { return DownstreamPin.Kernel == nullptr; }

	UPCGComputeKernel* GetUpstreamKernel() const { return UpstreamPin.Kernel; }
	UPCGComputeKernel* GetDownstreamKernel() const { return DownstreamPin.Kernel; }

	FPCGPinReference UpstreamPin;
	FPCGPinReference DownstreamPin;
};

/** Helper struct for serializing data labels. */
USTRUCT()
struct FPCGDataLabels
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> Labels;
};

/** Helper struct for serializing map of pin name to data labels. */
USTRUCT()
struct FPCGPinDataLabels
{
	GENERATED_BODY()

	UPROPERTY()
	TMap</*PinLabel*/FName, FPCGDataLabels> PinToDataLabels;
};
