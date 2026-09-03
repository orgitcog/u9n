// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanViewport.cpp: Vulkan viewport RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanSwapChain.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "VulkanBarriers.h"
#include "GlobalShader.h"
#include "HAL/PlatformAtomics.h"
#include "Engine/RendererSettings.h"
#include "StereoRenderUtils.h"
#include "CommonRenderResources.h"
#include "ScreenRendering.h"
#include "RHIStaticStates.h"

FVulkanBackBuffer::FVulkanBackBuffer(FVulkanDevice& Device, FVulkanViewport* InViewport, EPixelFormat Format, uint32 SizeX, uint32 SizeY, ETextureCreateFlags UEFlags)
	: FVulkanTexture(Device, FRHITextureCreateDesc::Create2D(TEXT("FVulkanBackBuffer"), SizeX, SizeY, Format).SetFlags(UEFlags).SetInitialState(ERHIAccess::Present), VK_NULL_HANDLE, {})
	, Viewport(InViewport)
{
}

void FVulkanBackBuffer::ReleaseAcquiredImage()
{
	if (DefaultView)
	{
		// Do not invalidate view here, just remove a reference to it
		DefaultView = nullptr;
		PartialView = nullptr;
	}

	Image = VK_NULL_HANDLE;
}

void FVulkanBackBuffer::ReleaseViewport()
{
	Viewport = nullptr;
	ReleaseAcquiredImage();
}

void FVulkanBackBuffer::OnGetBackBufferImage(FRHICommandListImmediate& RHICmdList)
{
	check(Viewport);
	if (GVulkanDelayAcquireImage == EDelayAcquireImageType::None)
	{
		FVulkanCommandListContext& Context = (FVulkanCommandListContext&)RHICmdList.GetContext().GetLowestLevelContext();
		AcquireBackBufferImage(Context);
	}
}

void FVulkanBackBuffer::OnAdvanceBackBufferFrame(FRHICommandListImmediate& RHICmdList)
{
	check(Viewport);
	ReleaseAcquiredImage();
}

void FVulkanBackBuffer::AcquireBackBufferImage(FVulkanCommandListContext& Context)
{
	check(Viewport);
	
	if (Image == VK_NULL_HANDLE)
	{
		if (Viewport->TryAcquireImageIndex())
		{
			int32 AcquiredImageIndex = Viewport->AcquiredImageIndex;
			check(AcquiredImageIndex >= 0 && AcquiredImageIndex < Viewport->TextureViews.Num());

			FVulkanView& ImageView = Viewport->TextureViews[AcquiredImageIndex];

			Image = ImageView.GetTextureView().Image;
			DefaultView = &ImageView;
			PartialView = &ImageView;

			// Wait for semaphore signal before writing to backbuffer image
			Context.AddWaitSemaphore(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, Viewport->AcquiredSemaphore);

			// :todo-jn: transition from unknown the first time
		}
		else
		{
			// fallback to a 'dummy' backbuffer
			check(Viewport->RenderingBackBuffer);
			FVulkanView* DummyView = Viewport->RenderingBackBuffer->DefaultView;
			Image = DummyView->GetTextureView().Image;
			DefaultView = DummyView;
			PartialView = DummyView;
		}
	}
}

FVulkanBackBuffer::~FVulkanBackBuffer()
{
	check(IsImageOwner() == false);
	// Clear ImageOwnerType so ~FVulkanTexture2D() doesn't try to re-destroy it
	ImageOwnerType = EImageOwnerType::None;
	ReleaseAcquiredImage();
}

FVulkanViewport::FVulkanViewport(FVulkanDevice* InDevice, void* InWindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat)
	: VulkanRHI::FDeviceChild(InDevice)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, bIsFullscreen(bInIsFullscreen)
	, PixelFormat(InPreferredPixelFormat)
	, AcquiredImageIndex(-1)
	, SwapChain(nullptr)
	, WindowHandle(InWindowHandle)
	, PresentCount(0)
	, bRenderOffscreen(false)
	, AcquiredSemaphore(nullptr)
{
	check(IsInGameThread());

	static IConsoleVariable* CVarVsync = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	LockToVsync = CVarVsync->GetInt() != 0;

	FVulkanDynamicRHI::Get().Viewports.Add(this);

	// Make sure Instance is created
	FVulkanDynamicRHI::Get().InitInstance();

	bRenderOffscreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"));

	FVulkanPlatformWindowContext WindowContext(InWindowHandle);

	ENQUEUE_RENDER_COMMAND(CreateSwapchain)(
		[&WindowContext, VulkanViewport = this](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.EnqueueLambda([&WindowContext, VulkanViewport](FRHICommandListBase& ExecutingCmdList)
				{
					VulkanViewport->CreateSwapchain(FVulkanCommandListContext::Get(ExecutingCmdList), nullptr, WindowContext);
				});
			RHICmdList.SubmitAndBlockUntilGPUIdle();
		});
	FlushRenderingCommands();

	if (SupportsStandardSwapchain())
	{
		FCoreDelegates::OnSystemResolutionChanged.AddRaw(this, &FVulkanViewport::OnSystemResolutionChanged);
	}
}

FVulkanViewport::~FVulkanViewport()
{
	RenderingBackBuffer = nullptr;
	
	if (RHIBackBuffer)
	{
		RHIBackBuffer->ReleaseViewport();
		RHIBackBuffer = nullptr;
	}
	
	if (SupportsStandardSwapchain())
	{
		TextureViews.Empty();

		for (int32 Index = 0, NumBuffers = RenderingDoneSemaphores.Num(); Index < NumBuffers; ++Index)
		{
			RenderingDoneSemaphores[Index]->Release();

			// FIXME: race condition on TransitionAndLayoutManager, could this be called from RT while RHIT is active?
			Device->NotifyDeletedImage(BackBufferImages[Index]->Image, true);
			BackBufferImages[Index] = nullptr;
		}

		SwapChain->Destroy(nullptr);
		delete SwapChain;
		SwapChain = nullptr;

		FCoreDelegates::OnSystemResolutionChanged.RemoveAll(this);
	}

	FVulkanDynamicRHI::Get().Viewports.Remove(this);
}

bool FVulkanViewport::DoCheckedSwapChainJob(FVulkanCommandListContext& Context, TFunction<int32(FVulkanViewport*)> SwapChainJob)
{
	int32 AttemptsPending = FVulkanPlatform::RecreateSwapchainOnFail() ? 4 : 0;
	int32 Status = SwapChainJob(this);

	while (FVulkanPlatformWindowContext::CanCreateSwapchainOnDemand() && Status < 0 && AttemptsPending > 0)
	{
		if (Status == (int32)FVulkanSwapChain::EStatus::OutOfDate)
		{
			UE_LOG(LogVulkanRHI, Verbose, TEXT("Swapchain is out of date! Trying to recreate the swapchain."));
		}
		else if (Status == (int32)FVulkanSwapChain::EStatus::SurfaceLost)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Swapchain surface lost! Trying to recreate the swapchain."));
		}
		else
		{
			check(0);
		}

		FVulkanPlatformWindowContext WindowContext(WindowHandle);
		RecreateSwapchain(Context, WindowContext);

		// Swapchain creation pushes some commands - flush the command buffers now to begin with a fresh state
		Context.FlushCommands(EVulkanFlushFlags::WaitForCompletion);

		Status = SwapChainJob(this);

		--AttemptsPending;
	}

	return Status >= 0;
}

bool FVulkanViewport::TryAcquireImageIndex()
{
	if (SwapChain)
	{
		int32 Result = SwapChain->AcquireImageIndex(&AcquiredSemaphore);
		if (Result >= 0)
		{
			AcquiredImageIndex = Result;
			return true;
		}
	}
	return false;
}

FTextureRHIRef FVulkanViewport::GetBackBuffer(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	// make sure we aren't in the middle of swapchain recreation (which can happen on e.g. RHI thread)
	FScopeLock LockSwapchain(&RecreatingSwapchain);

	if (SupportsStandardSwapchain() && GVulkanDelayAcquireImage != EDelayAcquireImageType::DelayAcquire)
	{
		check(RHICmdList.IsImmediate());
		check(RHIBackBuffer);
		
		RHICmdList.EnqueueLambda([this](FRHICommandListImmediate& CmdList)
		{
			this->RHIBackBuffer->OnGetBackBufferImage(CmdList);
		});

		return RHIBackBuffer.GetReference();
	}
	
	return RenderingBackBuffer.GetReference();
}

void FVulkanViewport::AdvanceBackBufferFrame(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	if (SupportsStandardSwapchain() && GVulkanDelayAcquireImage != EDelayAcquireImageType::DelayAcquire)
	{
		check(RHIBackBuffer);
		
		RHICmdList.EnqueueLambda([this](FRHICommandListImmediate& CmdList)
		{
			this->RHIBackBuffer->OnAdvanceBackBufferFrame(CmdList);
		});
	}
}

void FVulkanViewport::WaitForFrameEventCompletion()
{
	if (FVulkanPlatform::RequiresWaitingForFrameCompletionEvent())
	{
		static FCriticalSection CS;
		FScopeLock ScopeLock(&CS);
		if (LastFrameSyncPoint.IsValid())
		{
			// If last frame's fence hasn't been signaled already, wait for it here
			if (!LastFrameSyncPoint->IsComplete())
			{
				FVulkanDynamicRHI::Get().ProcessInterruptQueueUntil(LastFrameSyncPoint);
			}
		}
	}
}

void FVulkanViewport::IssueFrameEvent()
{
	if (FVulkanPlatform::RequiresWaitingForFrameCompletionEvent())
	{
		FVulkanCommandListContextImmediate& ImmediateContext = Device->GetImmediateContext();
		LastFrameSyncPoint = ImmediateContext.GetContextSyncPoint();
		ImmediateContext.FlushCommands();
	}
}


FVulkanFramebuffer::FVulkanFramebuffer(FVulkanDevice& Device, const FRHISetRenderTargetsInfo& InRTInfo, const FVulkanRenderTargetLayout& RTLayout, const FVulkanRenderPass& RenderPass)
	: Framebuffer(VK_NULL_HANDLE)
	, NumColorRenderTargets(InRTInfo.NumColorRenderTargets)
	, NumColorAttachments(0)
	, DepthStencilRenderTargetImage(VK_NULL_HANDLE)
	, FragmentDensityImage(VK_NULL_HANDLE)
{
	FMemory::Memzero(ColorRenderTargetImages);
	FMemory::Memzero(ColorResolveTargetImages);

	AttachmentTextureViews.Empty(RTLayout.GetNumAttachmentDescriptions());

	auto CreateOwnedView = [&]()
	{
		const VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		FVulkanView* View = new FVulkanView(Device, DescriptorType);
		AttachmentTextureViews.Add(View);
		OwnedTextureViews.Add(View);
		return View;
	};

	auto AddExternalView = [&](FVulkanView const* View)
	{
		AttachmentTextureViews.Add(View);
	};

	uint32 MipIndex = 0;

	const VkExtent3D& RTExtents = RTLayout.GetExtent3D();
	// Adreno does not like zero size RTs
	check(RTExtents.width != 0 && RTExtents.height != 0);
	uint32 NumLayers = RTExtents.depth;

	for (int32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
	{
		FRHITexture* RHITexture = InRTInfo.ColorRenderTarget[Index].Texture;
		if (!RHITexture)
		{
			continue;
		}

		FVulkanTexture* Texture = ResourceCast(RHITexture);
		const FRHITextureDesc& Desc = Texture->GetDesc();

		// this could fire in case one of the textures is FVulkanBackBuffer and it has not acquired an image
		// with EDelayAcquireImageType::LazyAcquire acquire happens when texture transition to Writeable state
		// make sure you call TransitionResource(Writable, Tex) before using this texture as a render-target
		check(Texture->Image != VK_NULL_HANDLE);

		ColorRenderTargetImages[Index] = Texture->Image;
		MipIndex = InRTInfo.ColorRenderTarget[Index].MipIndex;

		if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		{
			uint32 ArraySliceIndex = 0;
			uint32 NumArraySlices = 1;
			if (InRTInfo.ColorRenderTarget[Index].ArraySliceIndex == -1)
			{
				ArraySliceIndex = 0;
				NumArraySlices = Texture->GetNumberOfArrayLevels();
			}
			else
			{
				ArraySliceIndex = InRTInfo.ColorRenderTarget[Index].ArraySliceIndex;
				NumArraySlices = 1;
				check(ArraySliceIndex < Texture->GetNumberOfArrayLevels());
			}

			// About !RTLayout.GetIsMultiView(), from https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkFramebufferCreateInfo.html: 
			// If the render pass uses multiview, then layers must be one
			if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY && !RTLayout.GetIsMultiView())
			{
				NumLayers = NumArraySlices;
			}

			CreateOwnedView()->InitAsTextureView(
				  Texture->Image
				, Texture->GetViewType()
				, Texture->GetFullAspectMask()
				, Desc.Format
				, Texture->ViewFormat
				, MipIndex
				, 1
				, ArraySliceIndex
				, NumArraySlices
				, true
				, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (Texture->ImageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
			);
		}
		else if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
		{
			// Cube always renders one face at a time
			INC_DWORD_STAT(STAT_VulkanNumImageViews);

			CreateOwnedView()->InitAsTextureView(
				  Texture->Image
				, VK_IMAGE_VIEW_TYPE_2D
				, Texture->GetFullAspectMask()
				, Desc.Format
				, Texture->ViewFormat
				, MipIndex
				, 1
				, InRTInfo.ColorRenderTarget[Index].ArraySliceIndex
				, 1
				, true
				, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (Texture->ImageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
			);
		}
		else if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_3D)
		{
			CreateOwnedView()->InitAsTextureView(
				  Texture->Image
				, VK_IMAGE_VIEW_TYPE_2D_ARRAY
				, Texture->GetFullAspectMask()
				, Desc.Format
				, Texture->ViewFormat
				, MipIndex
				, 1
				, 0
				, Desc.Depth
				, true
				, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (Texture->ImageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
			);
		}
		else
		{
			ensure(0);
		}

		++NumColorAttachments;

		// Check the RTLayout as well to make sure the resolve attachment is needed (Vulkan and Feature level specific)
		// See: FVulkanRenderTargetLayout constructor with FRHIRenderPassInfo
		if (InRTInfo.bHasResolveAttachments && RTLayout.GetHasResolveAttachments() && RTLayout.GetResolveAttachmentReferences()[Index].layout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			FRHITexture* ResolveRHITexture = InRTInfo.ColorResolveRenderTarget[Index].Texture;
			FVulkanTexture* ResolveTexture = ResourceCast(ResolveRHITexture);
			ColorResolveTargetImages[Index] = ResolveTexture->Image;

			//resolve attachments only supported for 2d/2d array textures
			if (ResolveTexture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || ResolveTexture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
			{
				CreateOwnedView()->InitAsTextureView(
					  ResolveTexture->Image
					, ResolveTexture->GetViewType()
					, ResolveTexture->GetFullAspectMask()
					, ResolveTexture->GetDesc().Format
					, ResolveTexture->ViewFormat
					, MipIndex
					, 1
					, FMath::Max(0, (int32)InRTInfo.ColorRenderTarget[Index].ArraySliceIndex)
					, ResolveTexture->GetNumberOfArrayLevels()
					, true
				);
			}
		}
	}

	if (RTLayout.GetHasDepthStencil())
	{
		FVulkanTexture* Texture = ResourceCast(InRTInfo.DepthStencilRenderTarget.Texture);
		const FRHITextureDesc& Desc = Texture->GetDesc();
		DepthStencilRenderTargetImage = Texture->Image;
		bool bHasStencil = (Texture->GetDesc().Format == PF_DepthStencil || Texture->GetDesc().Format == PF_X24_G8);

		check(Texture->PartialView);
		PartialDepthTextureView = Texture->PartialView;

		ensure(Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE);
		if (NumColorAttachments == 0 && Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE)
		{
			CreateOwnedView()->InitAsTextureView(
				  Texture->Image
				, VK_IMAGE_VIEW_TYPE_2D_ARRAY
				, Texture->GetFullAspectMask()
				, Texture->GetDesc().Format
				, Texture->ViewFormat
				, MipIndex
				, 1
				, 0
				, 6
				, true
			);

			NumLayers = 6;
		}
		else if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D  || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		{
			// depth attachments need a separate view to have no swizzle components, for validation correctness
			CreateOwnedView()->InitAsTextureView(
				  Texture->Image
				, Texture->GetViewType()
				, Texture->GetFullAspectMask()
				, Texture->GetDesc().Format
				, Texture->ViewFormat
				, MipIndex
				, 1
				, 0
				, Texture->GetNumberOfArrayLevels()
				, true
			);
		}
		else
		{
			AddExternalView(Texture->DefaultView);
		}

		if (RTLayout.GetHasDepthStencilResolve() && RTLayout.GetDepthStencilResolveAttachmentReference()->layout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			FRHITexture* ResolveRHITexture = InRTInfo.DepthStencilResolveRenderTarget.Texture;
			FVulkanTexture* ResolveTexture = ResourceCast(ResolveRHITexture);
			DepthStencilResolveRenderTargetImage = ResolveTexture->Image;

			// Resolve attachments only supported for 2d/2d array textures
			if (ResolveTexture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || ResolveTexture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
			{
				CreateOwnedView()->InitAsTextureView(
					ResolveTexture->Image
					, ResolveTexture->GetViewType()
					, ResolveTexture->GetFullAspectMask()
					, ResolveTexture->GetDesc().Format
					, ResolveTexture->ViewFormat
					, MipIndex
					, 1
					, 0
					, ResolveTexture->GetNumberOfArrayLevels()
					, true
				);
			}
		}
	}

	if (GRHISupportsAttachmentVariableRateShading && RTLayout.GetHasFragmentDensityAttachment())
	{
		FVulkanTexture* Texture = ResourceCast(InRTInfo.ShadingRateTexture);
		FragmentDensityImage = Texture->Image;

		ensure(Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY);

		CreateOwnedView()->InitAsTextureView(
			Texture->Image
			, Texture->GetViewType()
			, Texture->GetFullAspectMask()
			, Texture->GetDesc().Format
			, Texture->ViewFormat
			, MipIndex
			, 1
			, 0
			, Texture->GetNumberOfArrayLevels()
			, true
		);
	}

	TArray<VkImageView> AttachmentViews;
	AttachmentViews.Reserve(AttachmentTextureViews.Num());
	for (FVulkanView const* View : AttachmentTextureViews)
	{
		AttachmentViews.Add(View->GetTextureView().View);
	}

	VkFramebufferCreateInfo CreateInfo;
	ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
	CreateInfo.renderPass = RenderPass.GetHandle();
	CreateInfo.attachmentCount = AttachmentViews.Num();
	CreateInfo.pAttachments = AttachmentViews.GetData();
	CreateInfo.width  = RTExtents.width;
	CreateInfo.height = RTExtents.height;
	CreateInfo.layers = NumLayers;

	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateFramebuffer(Device.GetInstanceHandle(), &CreateInfo, VULKAN_CPU_ALLOCATOR, &Framebuffer));

	RenderArea.offset.x = 0;
	RenderArea.offset.y = 0;
	RenderArea.extent.width = RTExtents.width;
	RenderArea.extent.height = RTExtents.height;

	INC_DWORD_STAT(STAT_VulkanNumFrameBuffers);
}

FVulkanFramebuffer::~FVulkanFramebuffer()
{
	ensure(Framebuffer == VK_NULL_HANDLE);
}

void FVulkanFramebuffer::Destroy(FVulkanDevice& Device)
{
	VulkanRHI::FDeferredDeletionQueue2& Queue = Device.GetDeferredDeletionQueue();
	
	// will be deleted in reverse order
	Queue.EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::Framebuffer, Framebuffer);
	Framebuffer = VK_NULL_HANDLE;

	DEC_DWORD_STAT(STAT_VulkanNumFrameBuffers);
}

bool FVulkanFramebuffer::Matches(const FRHISetRenderTargetsInfo& InRTInfo) const
{
	if (NumColorRenderTargets != InRTInfo.NumColorRenderTargets)
	{
		return false;
	}

	{
		const FRHIDepthRenderTargetView& B = InRTInfo.DepthStencilRenderTarget;
		if (B.Texture)
		{
			VkImage AImage = DepthStencilRenderTargetImage;
			VkImage BImage = ResourceCast(B.Texture)->Image;
			if (AImage != BImage)
			{
				return false;
			}
		}
	}

	{
		const FRHIDepthRenderTargetView& R = InRTInfo.DepthStencilResolveRenderTarget;
		if (R.Texture)
		{
			VkImage AImage = DepthStencilResolveRenderTargetImage;
			VkImage BImage = ResourceCast(R.Texture)->Image;
			if (AImage != BImage)
			{
				return false;
			}
		}
	}

	{
		FRHITexture* Texture = InRTInfo.ShadingRateTexture;
		if (Texture)
		{
			VkImage AImage = FragmentDensityImage;
			VkImage BImage = ResourceCast(Texture)->Image;
			if (AImage != BImage)
			{
				return false;
			}
		}
	}

	int32 AttachementIndex = 0;
	for (int32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
	{
		if (InRTInfo.bHasResolveAttachments)
		{
			const FRHIRenderTargetView& R = InRTInfo.ColorResolveRenderTarget[Index];
			if (R.Texture)
			{
				VkImage AImage = ColorResolveTargetImages[AttachementIndex];
				VkImage BImage = ResourceCast(R.Texture)->Image;
				if (AImage != BImage)
				{
					return false;
				}
			}
		}

		const FRHIRenderTargetView& B = InRTInfo.ColorRenderTarget[Index];
		if (B.Texture)
		{
			VkImage AImage = ColorRenderTargetImages[AttachementIndex];
			VkImage BImage = ResourceCast(B.Texture)->Image;
			if (AImage != BImage)
			{
				return false;
			}
			AttachementIndex++;
		}
	}

	return true;
}

// Tear down and recreate swapchain and related resources.
void FVulkanViewport::RecreateSwapchain(FVulkanCommandListContext& Context, FVulkanPlatformWindowContext& WindowContext)
{
	// Make sure everything is submitted and submission queue is idle
	Context.FlushCommands(EVulkanFlushFlags::WaitForCompletion);

	FScopeLock LockSwapchain(&RecreatingSwapchain);

	FVulkanSwapChainRecreateInfo RecreateInfo = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	DestroySwapchain(&RecreateInfo);
	CreateSwapchain(Context, &RecreateInfo, WindowContext);
	check(RecreateInfo.Surface == VK_NULL_HANDLE);
	check(RecreateInfo.SwapChain == VK_NULL_HANDLE);
}

void FVulkanViewport::Tick(float DeltaTime)
{
	check(IsInGameThread());

	if (SwapChain && FPlatformAtomics::AtomicRead(&LockToVsync) != SwapChain->DoesLockToVsync())
	{
		FVulkanPlatformWindowContext WindowContext(WindowHandle);

		ENQUEUE_RENDER_COMMAND(UpdateVsync)(
			[this, &WindowContext](FRHICommandListImmediate& RHICmdList)
		{
			RecreateSwapchainFromRT(RHICmdList, PixelFormat, WindowContext);
		});
		FlushRenderingCommands();
	}
}

void FVulkanViewport::Resize(FRHICommandListImmediate& RHICmdList, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat PreferredPixelFormat, FVulkanPlatformWindowContext& WindowContext)
{
	check(IsInRenderingThread());

	RHICmdList.EnqueueLambda([this, InSizeX, InSizeY, bInIsFullscreen, PreferredPixelFormat](FRHICommandListBase& ExecutingCmdList)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
		bIsFullscreen = bInIsFullscreen;
		PixelFormat = PreferredPixelFormat;
	});
	RecreateSwapchainFromRT(RHICmdList, PreferredPixelFormat, WindowContext);
}

void FVulkanViewport::RecreateSwapchainFromRT(FRHICommandListImmediate& RHICmdList, EPixelFormat PreferredPixelFormat, FVulkanPlatformWindowContext& WindowContext)
{
	check(IsInRenderingThread());

	RHICmdList.EnqueueLambda([this, PreferredPixelFormat, &WindowContext](FRHICommandListBase& ExecutingCmdList)
	{
		FVulkanSwapChainRecreateInfo RecreateInfo = { VK_NULL_HANDLE, VK_NULL_HANDLE };
		DestroySwapchain(&RecreateInfo);
		PixelFormat = PreferredPixelFormat;
		CreateSwapchain(FVulkanCommandListContext::Get(ExecutingCmdList), &RecreateInfo, WindowContext);
		check(RecreateInfo.Surface == VK_NULL_HANDLE);
		check(RecreateInfo.SwapChain == VK_NULL_HANDLE);
	});

	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
}

void FVulkanViewport::InitImages(FVulkanContextCommon& Context, TConstArrayView<VkImage> Images)
{
	FVulkanCommandBuffer& CommandBuffer = Context.GetCommandBuffer();

	VkClearColorValue ClearColor;
	FMemory::Memzero(ClearColor);

	const VkImageSubresourceRange Range = FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
	for (int32 Index = 0; Index < Images.Num(); ++Index)
	{
		uint32 ImageSizeX = SizeX;
		uint32 ImageSizeY = SizeY;
		VkSurfaceTransformFlagBitsKHR CachedSurfaceTransform = SwapChain->GetCachedSurfaceTransform();
		if (CachedSurfaceTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR || CachedSurfaceTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
		{
			Swap(ImageSizeX, ImageSizeY);
		}
		BackBufferImages[Index] = (FVulkanTexture*)FVulkanDynamicRHI::Get().RHICreateTexture2DFromResource(PixelFormat, ImageSizeX, ImageSizeY, 1, 1, Images[Index], ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::Presentable).GetReference();
		const VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		TextureViews.Add((new FVulkanView(*Device, DescriptorType))->InitAsTextureView(
			Images[Index], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, PixelFormat, UEToVkTextureFormat(PixelFormat, false), 0, 1, 0, 1, false));

		// Clear the swapchain to avoid a validation warning, and transition to PresentSrc
		{
			VulkanSetImageLayout(&CommandBuffer, Images[Index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Range);
			VulkanRHI::vkCmdClearColorImage(CommandBuffer.GetHandle(), Images[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColor, 1, &Range);
			VulkanSetImageLayout(&CommandBuffer, Images[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, Range);
		}

#if VULKAN_ENABLE_DRAW_MARKERS
		if (Device->GetSetDebugName())
		{
			VulkanRHI::SetDebugName(Device->GetSetDebugName(), Device->GetInstanceHandle(), BackBufferImages[Index]->Image, "VulkanBackBuffer");
		}
#endif
	}
}

void FVulkanViewport::CreateSwapchain(FVulkanCommandListContext& Context, FVulkanSwapChainRecreateInfo* RecreateInfo, FVulkanPlatformWindowContext& WindowContext)
{
	// Release a previous swapchain 'dummy' and a real backbuffer if any
	RenderingBackBuffer = nullptr;
	RHIBackBuffer = nullptr;

	if (SupportsStandardSwapchain())
	{
		check(SwapChain == nullptr);

		if (WindowContext.IsValid())
		{
			uint32 DesiredNumBackBuffers = NUM_BUFFERS;
			TArray<VkImage> Images;
			SwapChain = new FVulkanSwapChain(
				FVulkanDynamicRHI::Get().Instance, *Device,
				PixelFormat, SizeX, SizeY, bIsFullscreen,
				&DesiredNumBackBuffers,
				Images,
				LockToVsync,
				WindowContext,
				RecreateInfo
			);

			checkf(Images.Num() >= NUM_BUFFERS, TEXT("We wanted at least %i images, actual Num: %i"), NUM_BUFFERS, Images.Num());

			bool bCreateSemaphores = RenderingDoneSemaphores.IsEmpty();
			checkf(bCreateSemaphores || RenderingDoneSemaphores.Num() == Images.Num(), TEXT("CreateSwapchain, image count is not expected to change"));

			BackBufferImages.SetNum(Images.Num());
			RenderingDoneSemaphores.SetNum(Images.Num());
			InitImages(Context, Images);

			if (bCreateSemaphores)
			{
				for (int32 Index = 0, NumBuffers = RenderingDoneSemaphores.Num(); Index < NumBuffers; ++Index)
				{
					RenderingDoneSemaphores[Index] = new VulkanRHI::FSemaphore(*Device);
					RenderingDoneSemaphores[Index]->AddRef();
				}
			}
		}

		RHIBackBuffer = new FVulkanBackBuffer(*Device, this, PixelFormat, SizeX, SizeY, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_ResolveTargetable);
	}
	else
	{
		PixelFormat = GetPixelFormatForNonDefaultSwapchain();
		if (RecreateInfo != nullptr)
		{
			if (RecreateInfo->SwapChain)
			{
				FVulkanPlatform::DestroySwapchainKHR(Device->GetInstanceHandle(), RecreateInfo->SwapChain, VULKAN_CPU_ALLOCATOR);
				RecreateInfo->SwapChain = VK_NULL_HANDLE;
			}
			if (RecreateInfo->Surface)
			{
				VulkanRHI::vkDestroySurfaceKHR(FVulkanDynamicRHI::Get().Instance, RecreateInfo->Surface, VULKAN_CPU_ALLOCATOR);
				RecreateInfo->Surface = VK_NULL_HANDLE;
			}
		}
	}

	// We always create a 'dummy' backbuffer to gracefully handle SurfaceLost cases
	{
		uint32 BackBufferSizeX = RequiresRenderingBackBuffer() ? SizeX : 1;
		uint32 BackBufferSizeY = RequiresRenderingBackBuffer() ? SizeY : 1;

		const UE::StereoRenderUtils::FStereoShaderAspects Aspects(GMaxRHIShaderPlatform);

		const int kMultiViewCount = 2; // TODO: number of subresources may change in the future
		const FRHITextureCreateDesc CreateDesc = (Aspects.IsMobileMultiViewEnabled() ?			
			FRHITextureCreateDesc::Create2DArray(TEXT("RenderingBackBufferArr"), BackBufferSizeX, BackBufferSizeY, kMultiViewCount, PixelFormat) :
			FRHITextureCreateDesc::Create2D(TEXT("RenderingBackBuffer"), BackBufferSizeX, BackBufferSizeY, PixelFormat))
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource | ETextureCreateFlags::ResolveTargetable)
			.SetInitialState(ERHIAccess::Present);


		RenderingBackBuffer = FVulkanDynamicRHI::Get().CreateTextureInternal(CreateDesc, {});

		FVulkanPipelineBarrier Barrier;

		Barrier.AddImageLayoutTransition(RenderingBackBuffer->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));

		Barrier.Execute(&Context.GetCommandBuffer());

#if VULKAN_ENABLE_DRAW_MARKERS
		if (Device->GetSetDebugName())
		{
			VulkanRHI::SetDebugName(Device->GetSetDebugName(), Device->GetInstanceHandle(), RenderingBackBuffer->Image, "RenderingBackBuffer");
		}
#endif
	}

	AcquiredImageIndex = -1;
}

void FVulkanViewport::DestroySwapchain(FVulkanSwapChainRecreateInfo* RecreateInfo)
{
	FVulkanDynamicRHI::Get().RHIBlockUntilGPUIdle();

	// Intentionally leave RenderingBackBuffer alive, so it can be used a dummy backbuffer while we don't have swapchain images
	// RenderingBackBuffer = nullptr;
	
	if (RHIBackBuffer)
	{
		RHIBackBuffer->ReleaseAcquiredImage();
		// We release this RHIBackBuffer when we create a new swapchain
	}
		
	if (SupportsStandardSwapchain() && SwapChain)
	{
		TextureViews.Empty();
		for (int32 Index = 0, NumBuffers = BackBufferImages.Num(); Index < NumBuffers; ++Index)
		{
			Device->NotifyDeletedImage(BackBufferImages[Index]->Image, true);
			BackBufferImages[Index] = nullptr;
		}
		
		Device->GetDeferredDeletionQueue().ReleaseResources(true);

		SwapChain->Destroy(RecreateInfo);
		delete SwapChain;
		SwapChain = nullptr;

		Device->GetDeferredDeletionQueue().ReleaseResources(true);
	}

	AcquiredImageIndex = -1;
}

inline static void CopyImageToBackBuffer(FVulkanCommandListContext& Context, FVulkanTexture& SrcSurface, FVulkanTexture& DstSurface, int32 SizeX, int32 SizeY, int32 WindowSizeX, int32 WindowSizeY, VkSurfaceTransformFlagBitsKHR CachedSurfaceTransform)
{
	const bool bNeedsVulkanPreTransform = CachedSurfaceTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	FVulkanCommandBuffer* CmdBuffer = &Context.GetCommandBuffer();
	check(CmdBuffer->IsOutsideRenderPass());

	const VkImageLayout SrcSurfaceLayout = bNeedsVulkanPreTransform ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	const VkImageLayout DstSurfaceLayout = bNeedsVulkanPreTransform ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	{
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(SrcSurface.Image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SrcSurfaceLayout, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
		Barrier.AddImageLayoutTransition(DstSurface.Image, VK_IMAGE_LAYOUT_UNDEFINED, DstSurfaceLayout, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
		Barrier.Execute(CmdBuffer);
	}

	VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer->GetHandle(), 32);

	// Copy and rotate the intermediate image to the BackBuffer with a pixel shader
	if (bNeedsVulkanPreTransform)
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		// No alpha blending, no depth tests or writes, no stencil tests or writes, no backface culling.
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TRHICommandList_RecursiveHazardous<FVulkanCommandListContext> RHICmdList(&Context);

		RHICmdList.BeginRenderPass(FRHIRenderPassInfo(&DstSurface, ERenderTargetActions::DontLoad_Store), TEXT("SurfaceTransform"));

		auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FImagePreTransformVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		FImagePreTransformVS::FParameters VSParameters;
		FMatrix44f RenderPassTransformMatrix = FRotationMatrix44f(FRotator3f(0.0f, -180.0f * (FMath::Log2(static_cast<float>(CachedSurfaceTransform))) / 2, 0.0f));
		VSParameters.PreTransform.X = RenderPassTransformMatrix.M[0][0];
		VSParameters.PreTransform.Y = RenderPassTransformMatrix.M[0][1];
		VSParameters.PreTransform.Z = RenderPassTransformMatrix.M[1][0];
		VSParameters.PreTransform.W = RenderPassTransformMatrix.M[1][1];

		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VSParameters);
		SetShaderParametersLegacyPS(RHICmdList, PixelShader, TStaticSamplerState<SF_Point>::GetRHI(), &SrcSurface);

		RHICmdList.DrawPrimitive(0, 2, 1);

		RHICmdList.EndRenderPass();
	}
	else
	{
		if (SizeX != WindowSizeX || SizeY != WindowSizeY)
		{
			VkImageBlit Region;
			FMemory::Memzero(Region);
			Region.srcOffsets[0].x = 0;
			Region.srcOffsets[0].y = 0;
			Region.srcOffsets[0].z = 0;
			Region.srcOffsets[1].x = SizeX;
			Region.srcOffsets[1].y = SizeY;
			Region.srcOffsets[1].z = 1;
			Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			Region.srcSubresource.layerCount = 1;
			Region.dstOffsets[0].x = 0;
			Region.dstOffsets[0].y = 0;
			Region.dstOffsets[0].z = 0;
			Region.dstOffsets[1].x = WindowSizeX;
			Region.dstOffsets[1].y = WindowSizeY;
			Region.dstOffsets[1].z = 1;
			Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			Region.dstSubresource.baseArrayLayer = 0;
			Region.dstSubresource.layerCount = 1;
			VulkanRHI::vkCmdBlitImage(CmdBuffer->GetHandle(),
				SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &Region, VK_FILTER_LINEAR);
		}
		else
		{
			VkImageCopy Region;
			FMemory::Memzero(Region);
			Region.extent.width = SizeX;
			Region.extent.height = SizeY;
			Region.extent.depth = 1;
			Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//Region.srcSubresource.baseArrayLayer = 0;
			Region.srcSubresource.layerCount = 1;
			//Region.srcSubresource.mipLevel = 0;
			Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//Region.dstSubresource.baseArrayLayer = 0;
			Region.dstSubresource.layerCount = 1;
			//Region.dstSubresource.mipLevel = 0;
			VulkanRHI::vkCmdCopyImage(CmdBuffer->GetHandle(),
				SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &Region);
		}
	}

	{
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(SrcSurface.Image, SrcSurfaceLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
		Barrier.AddImageLayoutTransition(DstSurface.Image, DstSurfaceLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
		Barrier.Execute(CmdBuffer);
	}
}

bool FVulkanViewport::Present(FVulkanCommandListContext& Context, FVulkanQueue* PresentQueue, bool bLockToVsync)
{
	check(Context.IsImmediate());

	FPlatformAtomics::AtomicStore(&LockToVsync, bLockToVsync ? 1 : 0);

	//Transition back buffer to presentable and submit that command
	if (SupportsStandardSwapchain())
	{
		bool bFailedToDelayAcquireBackbuffer = false;

		if (GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire && RenderingBackBuffer)
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanAcquireBackBuffer);
			// swapchain can go out of date, do not crash at this point
			if (LIKELY(TryAcquireImageIndex()))
			{
				// Wait for semaphore signal before writing to backbuffer image
				Context.AddWaitSemaphore(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, AcquiredSemaphore);
				
				uint32 WindowSizeX = FMath::Min(SizeX, SwapChain->InternalWidth);
				uint32 WindowSizeY = FMath::Min(SizeY, SwapChain->InternalHeight);

				CopyImageToBackBuffer(Context, *RenderingBackBuffer.GetReference(), *BackBufferImages[AcquiredImageIndex].GetReference(), SizeX, SizeY, WindowSizeX, WindowSizeY, SwapChain->GetCachedSurfaceTransform());
			}
			else
			{
				bFailedToDelayAcquireBackbuffer = true;
			}
		}
		else
		{
			if (AcquiredImageIndex != -1)
			{
				FVulkanCommandBuffer& CommandBuffer = Context.GetCommandBuffer();
				check(CommandBuffer.IsOutsideRenderPass());
				check(RHIBackBuffer != nullptr && RHIBackBuffer->Image == BackBufferImages[AcquiredImageIndex]->Image);
				VulkanSetImageLayout(&CommandBuffer, BackBufferImages[AcquiredImageIndex]->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
			}
			else
			{
				// When we have failed to acquire backbuffer image we fallback to using 'dummy' backbuffer
				check(RHIBackBuffer != nullptr && RHIBackBuffer->Image == RenderingBackBuffer->Image);
			}
		}

		if (LIKELY(!bFailedToDelayAcquireBackbuffer))
		{
			if (AcquiredImageIndex >= 0)
			{
				Context.AddSignalSemaphore(RenderingDoneSemaphores[AcquiredImageIndex]);
			}
		}
		else
		{
			if(FVulkanPlatformWindowContext::CanCreateSwapchainOnDemand()) // on android we dont want to attempt to recreate the window when we dont have the window lock...
			{
				// failing to do the delayacquire can only happen if we were in this mode to begin with
				check(GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire);

				UE_LOG(LogVulkanRHI, Log, TEXT("AcquireNextImage() failed due to the outdated swapchain, not even attempting to present."));

				FVulkanPlatformWindowContext WindowContext(GetWindowHandle());
				RecreateSwapchain(Context, WindowContext);

				// Swapchain creation pushes some commands - flush the command buffers now to begin with a fresh state
				Context.FlushCommands(EVulkanFlushFlags::WaitForCompletion);
			}

			// early exit
			return (int32)FVulkanSwapChain::EStatus::Healthy;
		}
	}

	// Submit any accumulated commands or syncs, wait until they hit the queue so that we can present
	Context.FlushCommands(EVulkanFlushFlags::WaitForSubmission);

	//#todo-rco: Proper SyncInterval bLockToVsync ? RHIConsoleVariables::SyncInterval : 0
	int32 SyncInterval = 0;
	bool bNeedNativePresent = true;

	const bool bHasCustomPresent = IsValidRef(CustomPresent);
	if (bHasCustomPresent)
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanCustomPresentTime);
		bNeedNativePresent = CustomPresent->Present(Context, SyncInterval);
	}

	bool bResult = false;
	if (bNeedNativePresent && (!SupportsStandardSwapchain() || GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire || RHIBackBuffer != nullptr))
	{
		// Present the back buffer to the viewport window.
		auto SwapChainJob = [PresentQueue](FVulkanViewport* Viewport)
		{
			// May happend if swapchain was recreated in DoCheckedSwapChainJob()
			if (Viewport->AcquiredImageIndex == -1)
			{
				// Skip present silently if image has not been acquired
				return (int32)FVulkanSwapChain::EStatus::Healthy;
			}
		
			return (int32)Viewport->SwapChain->Present(PresentQueue, Viewport->RenderingDoneSemaphores[Viewport->AcquiredImageIndex]);
		};
		if (SupportsStandardSwapchain() && !DoCheckedSwapChainJob(Context, SwapChainJob))
		{
			UE_LOG(LogVulkanRHI, Error, TEXT("Swapchain present failed!"));
			bResult = false;
		}
		else
		{
			bResult = true;
		}

		if (bHasCustomPresent)
		{
			CustomPresent->PostPresent();
		}
	}

	if (FVulkanPlatform::RequiresWaitingForFrameCompletionEvent() && !bHasCustomPresent)
	{
		// Wait for the GPU to finish rendering the previous frame before finishing this frame.
		WaitForFrameEventCompletion();
		IssueFrameEvent();
	}

	// If the input latency timer has been triggered, block until the GPU is completely
	// finished displaying this frame and calculate the delta time.
	//if (GInputLatencyTimer.RenderThreadTrigger)
	//{
	//	WaitForFrameEventCompletion();
	//	uint32 EndTime = FPlatformTime::Cycles();
	//	GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
	//	GInputLatencyTimer.RenderThreadTrigger = false;
	//}

	AcquiredImageIndex = -1;

	++PresentCount;
	++GVulkanRHI->TotalPresentCount;

	return bResult;
}

VkFormat FVulkanViewport::GetSwapchainImageFormat() const
{
	return SwapChain->ImageFormat;
}

bool FVulkanViewport::SupportsStandardSwapchain()
{
	return !bRenderOffscreen && !FVulkanDynamicRHI::Get().bIsStandaloneStereoDevice;
}

bool FVulkanViewport::RequiresRenderingBackBuffer()
{
	return !FVulkanDynamicRHI::Get().bIsStandaloneStereoDevice;
}

EPixelFormat FVulkanViewport::GetPixelFormatForNonDefaultSwapchain()
{
	if (bRenderOffscreen || FVulkanDynamicRHI::Get().bIsStandaloneStereoDevice)
	{
		return PF_R8G8B8A8;
	}
	else
	{
		checkf(0, TEXT("Platform Requires Standard Swapchain!"));
		return PF_Unknown;
	}
}

void FVulkanViewport::OnSystemResolutionChanged(uint32 ResX, uint32 ResY)
{
	EDeviceScreenOrientation CurrentOrientation = FPlatformMisc::GetDeviceOrientation();

	// The swap chain needs to be recreated after a rotation
	// Only 180-degree rotations need to be handled here because 90-degree rotations will resize the viewport and recreate the swap chain.
	if ((CachedOrientation == EDeviceScreenOrientation::Portrait && CurrentOrientation == EDeviceScreenOrientation::PortraitUpsideDown)
		|| (CachedOrientation == EDeviceScreenOrientation::PortraitUpsideDown && CurrentOrientation == EDeviceScreenOrientation::Portrait)
		|| (CachedOrientation == EDeviceScreenOrientation::LandscapeRight && CurrentOrientation == EDeviceScreenOrientation::LandscapeLeft)
		|| (CachedOrientation == EDeviceScreenOrientation::LandscapeLeft && CurrentOrientation == EDeviceScreenOrientation::LandscapeRight))
	{
		check(IsInGameThread());
		FVulkanPlatformWindowContext WindowContext(GetWindowHandle());

		ENQUEUE_RENDER_COMMAND(RecreateSwapchain)(
			[this,&WindowContext](FRHICommandListImmediate& RHICmdList)
			{
				RecreateSwapchainFromRT(RHICmdList, WindowContext);
			});
		FlushRenderingCommands();
	}
	CachedOrientation = CurrentOrientation;
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FVulkanDynamicRHI::RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	check( IsInGameThread() );

	// Use a default pixel format if none was specified	
	if (PreferredPixelFormat == PF_Unknown)
	{
		static const auto* CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
		PreferredPixelFormat = EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnAnyThread()));
	}

	return new FVulkanViewport(Device, WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
}

void FVulkanDynamicRHI::RHIResizeViewport(FRHIViewport* ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	check(IsInGameThread());
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);

	// Use a default pixel format if none was specified	
	if (PreferredPixelFormat == PF_Unknown)
	{
		static const auto* CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
		PreferredPixelFormat = EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnAnyThread()));
	}

	if (Viewport->GetSizeXY() != FIntPoint(SizeX, SizeY) || Viewport->IsFullscreen() != bIsFullscreen)
	{
		FVulkanPlatformWindowContext WindowContext(Viewport->WindowHandle);

		ENQUEUE_RENDER_COMMAND(ResizeViewport)(
			[Viewport, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat,&WindowContext](FRHICommandListImmediate& RHICmdList)
			{
				Viewport->Resize(RHICmdList, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat, WindowContext);
			});
		FlushRenderingCommands();
	}
}

void FVulkanDynamicRHI::RHIResizeViewport(FRHIViewport* ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
{
	check(IsInGameThread());
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);

	if (Viewport->GetSizeXY() != FIntPoint(SizeX, SizeY))
	{
		FVulkanPlatformWindowContext WindowContext(Viewport->WindowHandle);

		ENQUEUE_RENDER_COMMAND(ResizeViewport)(
			[Viewport, SizeX, SizeY, bIsFullscreen, &WindowContext](FRHICommandListImmediate& RHICmdList)
			{
				Viewport->Resize(RHICmdList, SizeX, SizeY, bIsFullscreen, PF_Unknown, WindowContext);
			});
		FlushRenderingCommands();
	}
}

void FVulkanDynamicRHI::RHITick(float DeltaTime)
{
	check(IsInGameThread());
}

FTextureRHIRef FVulkanDynamicRHI::RHIGetViewportBackBuffer(FRHIViewport* ViewportRHI)
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListImmediate::Get();

	check(ViewportRHI);
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);

	if (Viewport->SwapChain)
	{
		Viewport->SwapChain->RenderThreadPacing();
	}

	return Viewport->GetBackBuffer(RHICmdList);
}

void FVulkanDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* ViewportRHI)
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListImmediate::Get();

	check(ViewportRHI);
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);

	Viewport->AdvanceBackBufferFrame(RHICmdList);
}

void FVulkanCommandListContext::RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
{
	PendingGfxState->SetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FVulkanCommandListContext::RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ)
{
	TStaticArray<VkViewport, 2> Viewports;

	Viewports[0].x = FMath::FloorToInt(LeftMinX);
	Viewports[0].y = FMath::FloorToInt(LeftMinY);
	Viewports[0].width = FMath::CeilToInt(LeftMaxX - LeftMinX);
	Viewports[0].height = FMath::CeilToInt(LeftMaxY - LeftMinY);
	Viewports[0].minDepth = MinZ;
	Viewports[0].maxDepth = MaxZ;

	Viewports[1].x = FMath::FloorToInt(RightMinX);
	Viewports[1].y = FMath::FloorToInt(RightMinY);
	Viewports[1].width = FMath::CeilToInt(RightMaxX - RightMinX);
	Viewports[1].height = FMath::CeilToInt(RightMaxY - RightMinY);
	Viewports[1].minDepth = MinZ;
	Viewports[1].maxDepth = MaxZ;

	PendingGfxState->SetMultiViewport(Viewports);
}

void FVulkanCommandListContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	PendingGfxState->SetScissor(bEnable, MinX, MinY, MaxX, MaxY);
}
