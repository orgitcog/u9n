// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/PCGComputeCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "PCGSubsystem.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGPinPropertiesGPU.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGTextureData.h"
#include "Editor/IPCGEditorModule.h"

#include "DynamicRHI.h"
#include "RHIStats.h"
#include "RenderResource.h"

namespace PCGComputeConstants
{
	/** PCG data types supported in GPU node inputs. */
	constexpr EPCGDataType AllowedInputTypes =
		EPCGDataType::Point
		| EPCGDataType::Param
		| EPCGDataType::Landscape
		| EPCGDataType::BaseTexture
		| EPCGDataType::VirtualTexture
		| EPCGDataType::Resource
		| EPCGDataType::ProxyForGPU;

	/** PCG data types supported in GPU node outputs. */
	constexpr EPCGDataType AllowedOutputTypes = EPCGDataType::Point | EPCGDataType::Param | EPCGDataType::ProxyForGPU | EPCGDataType::BaseTexture;

	/** PCG data types supported in GPU data collections. */
	constexpr EPCGDataType AllowedDataCollectionTypes = EPCGDataType::Point | EPCGDataType::Param | EPCGDataType::ProxyForGPU;
}

namespace PCGComputeHelpers
{
	static TAutoConsoleVariable<float> CVarMaxGPUBufferSizeProportion(
		TEXT("pcg.GraphExecution.GPU.MaxBufferSize"),
		0.5f,
		TEXT("Maximum GPU buffer size as proportion of total available graphics memory."));

	int GetElementCount(const UPCGData* InData)
	{
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(InData))
		{
			return PointData->GetPoints().Num();
		}
		else if (const UPCGParamData* ParamData = Cast<UPCGParamData>(InData))
		{
			if (const UPCGMetadata* Metadata = ParamData->ConstMetadata())
			{
				return Metadata->GetItemCountForChild();
			}
		}
		else if (const UPCGProxyForGPUData* Proxy = Cast<UPCGProxyForGPUData>(InData))
		{
			return Proxy->GetElementCount();
		}

		return 0;
	}

	FIntPoint GetElementCount2D(const UPCGData* InData)
	{
		if (const UPCGBaseTextureData* TextureData = Cast<UPCGBaseTextureData>(InData))
		{
			return TextureData->GetTextureSize();
		}

		return FIntPoint::ZeroValue;
	}

	bool IsTypeAllowedAsInput(EPCGDataType Type)
	{
		return (Type | PCGComputeConstants::AllowedInputTypes) == PCGComputeConstants::AllowedInputTypes;
	}

	bool IsTypeAllowedAsOutput(EPCGDataType Type)
	{
		return (Type | PCGComputeConstants::AllowedOutputTypes) == PCGComputeConstants::AllowedOutputTypes;
	}

	bool IsTypeAllowedInDataCollection(EPCGDataType Type)
	{
		return (Type | PCGComputeConstants::AllowedDataCollectionTypes) == PCGComputeConstants::AllowedDataCollectionTypes;
	}

	bool ShouldImportAttributesFromData(const UPCGData* InData)
	{
		// We only read and expose attributes to compute from the following types. Other types are supported but we don't
		// register/upload their metadata attributes automatically.
		return InData && (InData->IsA<UPCGParamData>() || InData->IsA<UPCGPointData>());
	}

#if PCG_KERNEL_LOGGING_ENABLED
	void LogKernelWarning(const FPCGContext* Context, const UPCGSettings* Settings, const FText& InText)
	{
#if WITH_EDITOR
		if (Context && Settings)
		{
			if (IPCGEditorModule* PCGEditorModule = IPCGEditorModule::Get())
			{
				const FPCGStack* Stack = Context->GetStack();
				FPCGStack StackWithNode = Stack ? *Stack : FPCGStack();
				StackWithNode.PushFrame(Settings->GetOuter());

				PCGEditorModule->GetNodeVisualLogsMutable().Log(StackWithNode, ELogVerbosity::Warning, InText);
			}
		}
#endif
		PCGE_LOG_C(Warning, LogOnly, Context, InText);
	}

	void LogKernelError(const FPCGContext* Context, const UPCGSettings* Settings, const FText& InText)
	{
#if WITH_EDITOR
		if (Context && Settings)
		{
			if (IPCGEditorModule* PCGEditorModule = IPCGEditorModule::Get())
			{
				FPCGStack StackWithNode = Context->GetStack() ? *Context->GetStack() : FPCGStack();
				StackWithNode.PushFrame(Settings->GetOuter());

				PCGEditorModule->GetNodeVisualLogsMutable().Log(StackWithNode, ELogVerbosity::Error, InText);
			}
		}
#endif
		PCGE_LOG_C(Error, LogOnly, Context, InText);
	}
#endif

	bool IsBufferSizeTooLarge(uint64 InBufferSizeBytes, bool bInLogError)
	{
		FTextureMemoryStats TextureMemStats;
		RHIGetTextureMemoryStats(TextureMemStats);

		// If buffer size exceeds a proportion of total graphics memory, then it is deemed too large. Using this as a heuristic as there
		// is no RHI API to obtain available graphics memory outside of D3D12.
		const uint64 GPUBudgetBytes = static_cast<uint64>(TextureMemStats.TotalGraphicsMemory) * static_cast<double>(CVarMaxGPUBufferSizeProportion.GetValueOnAnyThread());

		// Buffer size also cannot exceed the max size of a uint32, otherwise the size will get truncated and other systems will break.
		// TODO: This limits the maximum number of points to around 46 million. Support uint64 max instead of uint32 to get up to 2 billion points.
		const uint64 MaxBufferSize = TNumericLimits<uint32>::Max();
		const uint64 BudgetBytes = FMath::Min(GPUBudgetBytes, MaxBufferSize);

		const bool bBufferTooLarge = TextureMemStats.TotalGraphicsMemory > 0 && InBufferSizeBytes > BudgetBytes;

		if (bBufferTooLarge && bInLogError)
		{
			UE_LOG(LogPCG, Error, TEXT("Attempted to allocate a GPU buffer of size %llu bytes which is larger than the safety threshold (%llu bytes). Compute graph execution aborted."),
				InBufferSizeBytes,
				BudgetBytes);
		}

		return bBufferTooLarge;
	}

	int32 GetAttributeIdFromMetadataAttributeIndex(int32 InAttributeIndex)
	{
		return InAttributeIndex >= 0 ? (InAttributeIndex + PCGComputeConstants::NUM_RESERVED_ATTRS) : INDEX_NONE;
	}

	int32 GetMetadataAttributeIndexFromAttributeId(int32 InAttributeId)
	{
		return InAttributeId >= PCGComputeConstants::NUM_RESERVED_ATTRS ? (InAttributeId - PCGComputeConstants::NUM_RESERVED_ATTRS) : INDEX_NONE;
	}

	FString GetPrefixedDataLabel(const FString& InLabel)
	{
		return FString::Format(TEXT("{0}:{1}"), { PCGComputeConstants::DataLabelTagPrefix, InLabel });
	}

	FString GetDataLabelResolverName(FName InPinLabel)
	{
		return FString::Format(TEXT("{0}_DataResolver"), { *InPinLabel.ToString() });
	}

#if WITH_EDITOR
	void ConvertObjectPathToShaderFilePath(FString& InOutPath)
	{
		// Shader compiler recognizes "/Engine/Generated/..." path as special. 
		// It doesn't validate file suffix etc.
		InOutPath = FString::Printf(TEXT("/Engine/Generated/UObject%s.ush"), *InOutPath);
		// Shader compilation result parsing will break if it finds ':' where it doesn't expect.
		InOutPath.ReplaceCharInline(TEXT(':'), TEXT('@'));
	}
#endif
}

namespace PCGComputeDummies
{
	class FPCGEmptyBufferSRV : public FRenderResource
	{
	public:
		FPCGEmptyBufferSRV(EPixelFormat InPixelFormat, const FString& InDebugName)
			: PixelFormat(InPixelFormat)
			, DebugName(InDebugName)
		{}

		EPixelFormat PixelFormat;
		FString DebugName;
		FBufferRHIRef Buffer = nullptr;
		FShaderResourceViewRHIRef BufferSRV = nullptr;
	
		virtual void InitRHI(FRHICommandListBase& RHICmdList) override
		{
			// Create a buffer with one element.
			const uint32 NumBytes = GPixelFormats[PixelFormat].BlockBytes;

			FRHIBufferCreateDesc CreateDesc =
				FRHIBufferCreateDesc::CreateVertex(*DebugName, NumBytes)
				.AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::Static)
				.DetermineInitialState();

			Buffer = RHICmdList.CreateBuffer(CreateDesc.SetInitActionZeroData());
			BufferSRV = RHICmdList.CreateShaderResourceView(
				Buffer,
				FRHIViewDesc::CreateBufferSRV()
					.SetType(FRHIViewDesc::EBufferType::Typed)
					.SetFormat(PixelFormat));
		}
	
		virtual void ReleaseRHI() override
		{
			BufferSRV.SafeRelease();
			Buffer.SafeRelease();
		}
	};
	
	FRHIShaderResourceView* GetDummyFloatBuffer()
	{
		static TGlobalResource<FPCGEmptyBufferSRV> DummyFloatBuffer(PF_R32_FLOAT, TEXT("PCGDummyFloat"));
		return DummyFloatBuffer.BufferSRV;
	}
	
	FRHIShaderResourceView* GetDummyFloat2Buffer()
	{
		static TGlobalResource<FPCGEmptyBufferSRV> DummyFloat2Buffer(PF_G32R32F, TEXT("PCGDummyFloat2"));
		return DummyFloat2Buffer.BufferSRV;
	}
	
	FRHIShaderResourceView* GetDummyFloat4Buffer()
	{
		static TGlobalResource<FPCGEmptyBufferSRV> DummyFloat4Buffer(PF_A32B32G32R32F, TEXT("PCGDummyFloat4"));
		return DummyFloat4Buffer.BufferSRV;
	}
}

uint32 GetTypeHash(const FPCGPinReference& In)
{
	return HashCombine(/*GetTypeHash(In.TaskId),*/ PointerHash(In.Kernel), GetTypeHash(In.Label));
}
