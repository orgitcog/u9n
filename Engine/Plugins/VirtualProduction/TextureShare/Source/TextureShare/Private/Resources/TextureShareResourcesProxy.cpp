// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResourcesProxy.h"
#include "Resources/TextureShareResourcesPool.h"
#include "Resources/TextureShareResourceUtils.h"

#include "Containers/TextureShareContainers.h"
#include "Containers/TextureShareCoreContainers.h"

#include "Core/TextureShareCoreHelpers.h"

#include "Module/TextureShareLog.h"

#include "ITextureShareCore.h"
#include "ITextureShareCoreAPI.h"
#include "ITextureShareCoreD3D11ResourcesCache.h"
#include "ITextureShareCoreD3D12ResourcesCache.h"
#include "ITextureShareCoreVulkanResourcesCache.h"

#include "RHIStaticStates.h"

#include "RenderingThread.h"
#include "RenderResource.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"

#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

#include "ScreenRendering.h"
#include "PostProcess/DrawRectangle.h"
#include "TextureResource.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareResourcesProxy
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareResourcesProxy::FTextureShareResourcesProxy()
{
	SendResourcesPool = MakeUnique<FTextureShareResourcesPool>();
	ReceiveResourcesPool = MakeUnique<FTextureShareResourcesPool>();
}

FTextureShareResourcesProxy::~FTextureShareResourcesProxy()
{ }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareResourcesProxy::Empty()
{
	// Release caches from prev frame (handle sync lost, etc)
	ResourceCrossGPUTransferPreSyncData.Empty();
	ResourceCrossGPUTransferPostSyncData.Empty();

	RegisteredResources.Empty();
	ReceiveResourceData.Empty();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareResourcesProxy::RHIThreadFlush_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	bool bRHIFlushRequired = false;

	if (SendResourcesPool.IsValid() && SendResourcesPool->IsRHICommandsListChanged_RenderThread())
	{
		SendResourcesPool->ClearFlagRHICommandsListChanged_RenderThread();
		bRHIFlushRequired = true;
	}

	if (ReceiveResourcesPool.IsValid() && ReceiveResourcesPool->IsRHICommandsListChanged_RenderThread())
	{
		ReceiveResourcesPool->ClearFlagRHICommandsListChanged_RenderThread();
		bRHIFlushRequired = true;
	}

	if (bRHIFlushRequired || bRHIThreadChanged || bForceRHIFlush || PooledTempRTTs.Num())
	{
		UE_TS_LOG(LogTextureShareResource, Log, TEXT("RHIThreadFlush_RenderThread( %s%s%s%s)"),
			bRHIFlushRequired ? TEXT("bRHIFlushRequired ") : TEXT(""),
			bRHIThreadChanged ? TEXT("bRHIThreadChanged ") : TEXT(""),
			bForceRHIFlush ? TEXT("bForceRHIFlush ") : TEXT(""),
			PooledTempRTTs.Num() ? TEXT("PooledTempRTTs ") : TEXT("")
		);

		bRHIThreadChanged = false;
		bForceRHIFlush = false;

		// Flush RHI if needed
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

		// Releasing temporary RTTs after RHI reset(Pending operations with RTTs on RHIThread completed)
		PooledTempRTTs.Empty();
	}
}

void FTextureShareResourcesProxy::PushCrossGPUTransfer_RenderThread(const ECrossGPUTransferType InType, FTextureShareResource* InSharedResource, const int32 InSrcGPUIndex, const int32 InDstGPUIndex)
{
	if (InSharedResource && (InSrcGPUIndex >= 0 || InDstGPUIndex >= 0))
	{
		switch (InType)
		{
		case ECrossGPUTransferType::BeforeSync:
			ResourceCrossGPUTransferPreSyncData.AddUnique(FResourceCrossGPUTransferData(InSharedResource, InSrcGPUIndex, InDstGPUIndex));
			break;
		case ECrossGPUTransferType::AfterSync:
			ResourceCrossGPUTransferPostSyncData.AddUnique(FResourceCrossGPUTransferData(InSharedResource, InSrcGPUIndex, InDstGPUIndex));
			break;
		default:
			break;
		}
	}
}

void FTextureShareResourcesProxy::RunCrossGPUTransfer_RenderThread(const ECrossGPUTransferType InType, FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep)
{
	switch (InType)
	{
	case ECrossGPUTransferType::BeforeSync:
		DoCrossGPUTransfers_RenderThread(RHICmdList, InSyncStep, ResourceCrossGPUTransferPreSyncData);
		break;

	case ECrossGPUTransferType::AfterSync:
		DoCrossGPUTransfers_RenderThread(RHICmdList, InSyncStep, ResourceCrossGPUTransferPostSyncData);
		break;

	default:
		break;
	}
}

void FTextureShareResourcesProxy::DoCrossGPUTransfers_RenderThread(FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep, TArray<FResourceCrossGPUTransferData>& InOutData)
{
	check(IsInRenderingThread());
	TArray<FResourceCrossGPUTransferData> DelayedData;

#if WITH_MGPU
	// Copy the view render results to all GPUs that are native to the viewport.
	TArray<FTransferResourceParams> TransferResources;

	for (const FResourceCrossGPUTransferData& CrossGPUDataIt : InOutData)
	{
		if (CrossGPUDataIt.SharedResource && CrossGPUDataIt.SharedResource->IsInitialized())
		{
			const ETextureShareSyncStep ResourceSyncStep = CrossGPUDataIt.SharedResource->GetResourceDesc().SyncStep;
			if (ResourceSyncStep != ETextureShareSyncStep::Undefined && ResourceSyncStep > InSyncStep)
			{
				DelayedData.Add(CrossGPUDataIt);
			}
			else
			{
				const FRHIGPUMask SrcGPUMask = (CrossGPUDataIt.SrcGPUIndex > 0) ? FRHIGPUMask::FromIndex(CrossGPUDataIt.SrcGPUIndex) : FRHIGPUMask::GPU0();
				const FRHIGPUMask DestGPUMask = (CrossGPUDataIt.DestGPUIndex > 0) ? FRHIGPUMask::FromIndex(CrossGPUDataIt.DestGPUIndex) : FRHIGPUMask::GPU0();
				if (SrcGPUMask != DestGPUMask)
				{
					// Clamp the view rect by the rendertarget rect to prevent issues when resizing the viewport.
					const FIntRect TransferRect(FIntPoint(0, 0), FIntPoint(CrossGPUDataIt.SharedResource->GetSizeX(), CrossGPUDataIt.SharedResource->GetSizeY()));
					if (TransferRect.Width() > 0 && TransferRect.Height() > 0)
					{
						TransferResources.Add(FTransferResourceParams(CrossGPUDataIt.SharedResource->GetTextureRHI(), TransferRect, SrcGPUMask.GetFirstIndex(), DestGPUMask.GetFirstIndex(), true, true));
					}
				}
			}
		}
	}

	if (TransferResources.Num() > 0)
	{
		RHICmdList.TransferResources(TransferResources);
	}

#endif

	InOutData.Empty();
	InOutData.Append(DelayedData);
}

void FTextureShareResourcesProxy::PushReceiveResource_RenderThread(
	const FTextureShareCoreResourceRequest& InResourceRequest,
	FTextureShareResource* InSrcSharedResource,
	FRHITexture* InDestTexture,
	const FTextureShareColorDesc& InDestTextureColorDesc,
	const FIntRect* InDestTextureSubRect)
{
	if (InSrcSharedResource && InDestTexture)
	{
		UE_TS_LOG(LogTextureShareResource, Log, TEXT("%s:PushReceiveResource_RenderThread(%s.%s)"), *InSrcSharedResource->GetCoreObjectName(), *InSrcSharedResource->GetResourceDesc().ViewDesc.Id, *InSrcSharedResource->GetResourceDesc().ResourceName);
		ReceiveResourceData.AddUnique(FReceiveResourceData(InResourceRequest, InSrcSharedResource, InDestTexture, InDestTextureColorDesc, InDestTextureSubRect));
	}
}

void FTextureShareResourcesProxy::PushReceiveResource_RenderThread(
	const FTextureShareCoreResourceRequest& InResourceRequest,
	FTextureShareResource* InSrcSharedResource,
	FTextureRenderTargetResource* InDestRTT,
	const FTextureShareColorDesc& InDestRTTColorDesc,
	const FIntRect* InDestTextureSubRect)
{
	if (InSrcSharedResource && InDestRTT)
	{
		UE_TS_LOG(LogTextureShareResource, Log, TEXT("%s:PushReceiveResource_RenderThread(%s.%s) [RTT]"), *InSrcSharedResource->GetCoreObjectName(), *InSrcSharedResource->GetResourceDesc().ViewDesc.Id, *InSrcSharedResource->GetResourceDesc().ResourceName);
		ReceiveResourceData.AddUnique(FReceiveResourceData(InResourceRequest, InSrcSharedResource, InDestRTT, InDestRTTColorDesc, InDestTextureSubRect));
	}
}

void FTextureShareResourcesProxy::RunReceiveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const ETextureShareSyncStep InSyncStep)
{
	{
		using namespace UE::TextureShareCore;
		UE_TS_LOG(LogTextureShareResource, Log, TEXT("RunReceiveResources_RenderThread(%s) %d"), GetTEXT(InSyncStep), ReceiveResourceData.Num());
	}

	TArray<FReceiveResourceData> DelayedData;
	for (FReceiveResourceData& ResourceDataIt : ReceiveResourceData)
	{
		const ETextureShareSyncStep ResourceSyncStep = ResourceDataIt.ResourceRequest.ResourceDesc.SyncStep;
		if (ResourceSyncStep != ETextureShareSyncStep::Undefined && ResourceSyncStep > InSyncStep)
		{
			DelayedData.Add(ResourceDataIt);
		}
		else
		{
			// This code after
			const FIntRect* DestTextureRect = ResourceDataIt.InDestTextureSubRect.IsEmpty() ? nullptr : &ResourceDataIt.InDestTextureSubRect;

			if (ResourceDataIt.DestTexture)
			{
				// Copy Shared Texture to Dest Texture
				ReadFromShareTexture_RenderThread(RHICmdList, ResourceDataIt.SrcSharedResource, ResourceDataIt.DestTexture, ResourceDataIt.DestColorDesc, DestTextureRect);
			}
			else if (ResourceDataIt.DestRTT)
			{
				// Copy Shared Texture to Dest RTT Resource
				ReadFromShareTexture_RenderThread(RHICmdList, ResourceDataIt.SrcSharedResource, ResourceDataIt.DestRTT->TextureRHI, ResourceDataIt.DestColorDesc, DestTextureRect);
			}
		}
	}

	ReceiveResourceData.Empty();
	ReceiveResourceData.Append(DelayedData);
}

void FTextureShareResourcesProxy::PushRegisterResource_RenderThread(const FTextureShareCoreResourceRequest& InResourceRequest, FTextureShareResource* InSharedResource)
{
	if (InSharedResource)
	{
		RegisteredResources.AddUnique(FRegisteredResourceData(InResourceRequest, InSharedResource));
	}
}

void FTextureShareResourcesProxy::RunRegisterResourceHandles_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	for (FRegisteredResourceData& ResourceIt : RegisteredResources)
	{
		if (ResourceIt.SharedResource)
		{
			ResourceIt.SharedResource->RegisterResourceHandle_RenderThread(RHICmdList, ResourceIt.ResourceRequest);
		}
	};

	RegisteredResources.Empty();
}

FTextureShareResource* FTextureShareResourcesProxy::GetSharedTexture_RenderThread(FRHICommandListImmediate& RHICmdList, const TSharedRef<ITextureShareCoreObject, ESPMode::ThreadSafe>& InCoreObject, FRHITexture* InSrcTexture, const FTextureShareCoreResourceRequest& InResourceRequest)
{
	switch (InResourceRequest.ResourceDesc.OperationType)
	{
	case ETextureShareTextureOp::Read:
		if (SendResourcesPool.IsValid())
		{
			return SendResourcesPool->GetSharedResource_RenderThread(RHICmdList, InCoreObject, InSrcTexture, InResourceRequest);
		}
		break;
	case ETextureShareTextureOp::Write:
		if (ReceiveResourcesPool.IsValid())
		{
			return ReceiveResourcesPool->GetSharedResource_RenderThread(RHICmdList, InCoreObject, InSrcTexture, InResourceRequest);
		}
		break;
	default:
		break;
	}

	return nullptr;
}


bool FTextureShareResourcesProxy::WriteToShareTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture* InSrcTexture,
	const FTextureShareColorDesc& InSrcTextureColorDesc,
	const FIntRect* InSrcTextureRectPtr,
	FTextureShareResource* InDestSharedResource)
{
	if (InSrcTexture && InDestSharedResource)
	{
		if (FRHITexture* DestSharedTexture = InDestSharedResource->GetResourceTextureRHI())
		{
			UE_TS_LOG(LogTextureShareResource, Log, TEXT("%s:WriteToShareTexture_RenderThread(%s.%s)"), *InDestSharedResource->GetCoreObjectName(), *InDestSharedResource->GetResourceDesc().ViewDesc.Id, *InDestSharedResource->GetResourceDesc().ResourceName);

			if (FTextureShareResourceUtils::WriteToShareTexture_RenderThread(
				RHICmdList,
				PooledTempRTTs,
				InSrcTexture,
				DestSharedTexture,
				InSrcTextureColorDesc,
				InDestSharedResource->GetResourceSettings().ColorDesc,
				InSrcTextureRectPtr))
			{
				bRHIThreadChanged = true;

				return true;
			}
		}
	}

	return false;
}

bool FTextureShareResourcesProxy::ReadFromShareTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FTextureShareResource* InSrcSharedResource,
	FRHITexture* InDestTexture,
	const FTextureShareColorDesc& InDestTextureColorDesc,
	const FIntRect* InDestTextureRectPtr)
{
	if (InSrcSharedResource && InDestTexture)
	{
		if (FRHITexture* SrcSharedTexture = InSrcSharedResource->GetResourceTextureRHI())
		{
			UE_TS_LOG(LogTextureShareResource, Log, TEXT("%s:ReadFromShareTexture_RenderThread(%s.%s)"), *InSrcSharedResource->GetCoreObjectName(), *InSrcSharedResource->GetResourceDesc().ViewDesc.Id, *InSrcSharedResource->GetResourceDesc().ResourceName);

			if (FTextureShareResourceUtils::ReadFromShareTexture_RenderThread(
				RHICmdList,
				PooledTempRTTs,
				SrcSharedTexture,
				InDestTexture,
				InSrcSharedResource->GetResourceSettings().ColorDesc,
				InDestTextureColorDesc,
				InDestTextureRectPtr))
			{
				bRHIThreadChanged = true;

				return true;
			}
		}
	}

	return false;
}
