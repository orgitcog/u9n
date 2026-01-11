// Copyright Epic Games, Inc. All Rights Reserved.

#include "GPUTextureTransferModule.h"

#if DVP_SUPPORTED_PLATFORM
#include "D3D11TextureTransfer.h"
#include "D3D12TextureTransfer.h"
#include "RenderingThread.h"
#include "VulkanTextureTransfer.h"
#include "TextureTransferBase.h"
#endif

#if DVP_SUPPORTED_PLATFORM || PLATFORM_LINUX
#define VULKAN_PLATFORM 1
#else
#define VULKAN_PLATFORM 0
#endif

#if VULKAN_PLATFORM
#include "IVulkanDynamicRHI.h"
#endif

#include "CoreMinimal.h"
#include "GPUTextureTransfer.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#include <unordered_map>

DEFINE_LOG_CATEGORY(LogGPUTextureTransfer);

static TAutoConsoleVariable<bool> CVarMediaIOEnableGPUDirect(
	TEXT("MediaIO.EnableGPUDirect"), false,
	TEXT("Whether to enable GPU direct for faster video frame copies. (Experimental)"),
	ECVF_RenderThreadSafe);

namespace 
{
	auto ConvertRHI = [](ERHIInterfaceType RHI)
	{
		switch (RHI)
		{
		case ERHIInterfaceType::D3D11: return UE::GPUTextureTransfer::ERHI::D3D11;
		case ERHIInterfaceType::D3D12: return UE::GPUTextureTransfer::ERHI::D3D12;
		case ERHIInterfaceType::Vulkan: return UE::GPUTextureTransfer::ERHI::Vulkan;
		default: return UE::GPUTextureTransfer::ERHI::Invalid;
		}
	};
}

bool FGPUTextureTransferModule::IsAvailable()
{
#if DVP_SUPPORTED_PLATFORM
	return FGPUTextureTransferModule::Get().IsInitialized() && FGPUTextureTransferModule::Get().IsEnabled();
#else
	return false;
#endif
}

void FGPUTextureTransferModule::StartupModule()
{
	if (CVarMediaIOEnableGPUDirect.GetValueOnAnyThread())
	{
		FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FGPUTextureTransferModule::Initialize);
	}

	CVarMediaIOEnableGPUDirect.AsVariable()->OnChangedDelegate().AddRaw(this, &FGPUTextureTransferModule::OnEnableGPUDirectCVarChange);

	// Cache this information since GetGPUDriverInfo has to be called on game thread because of a call to GetValueOnGameThread.
	CachedDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
}

void FGPUTextureTransferModule::ShutdownModule()
{
	if (bInitialized)
	{
		UninitializeTextureTransfer();
	}
}

void FGPUTextureTransferModule::Initialize()
{
	if (FApp::CanEverRender())
	{
		if (LoadGPUDirectBinary())
		{
			// Always provide the necessary Vulkan extensions (it will just get ignored if a different RHI is used)
			{
#if DVP_SUPPORTED_PLATFORM
				const TArray<const ANSICHAR*> ExtentionsToAdd{ VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
#elif PLATFORM_LINUX
				const TArray<const ANSICHAR*> ExtentionsToAdd{ VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME };
#endif

#if VULKAN_PLATFORM
				IVulkanDynamicRHI::AddEnabledDeviceExtensionsAndLayers(ExtentionsToAdd, TArray<const ANSICHAR*>());
#endif
			}

			TransferObjects.AddDefaulted(RHI_MAX);

			InitializeTextureTransfer();
		}
	}
}

UE::GPUTextureTransfer::TextureTransferPtr FGPUTextureTransferModule::GetTextureTransfer()
{
#if DVP_SUPPORTED_PLATFORM
	if (!IsInitialized())
	{
		UE_LOG(LogGPUTextureTransfer, Warning, TEXT("GetTextureTransfer was called without initializing the library. This will cause a hitch since we have to block while waiting for the library to finish initializing."));
		Initialize();

		// Initialization is done on the rendering thread.
		FlushRenderingCommands();
	}
#endif


	if (FApp::CanEverRender())
	{
#if DVP_SUPPORTED_PLATFORM
		UE::GPUTextureTransfer::ERHI SupportedRHI = ConvertRHI(RHIGetInterfaceType());
		if (SupportedRHI == UE::GPUTextureTransfer::ERHI::Invalid) 
		{
			UE_LOG(LogGPUTextureTransfer, Error, TEXT("The current RHI is not supported with GPU Texture Transfer."));
			return nullptr;
		}
	
		const uint8 RHIIndex = static_cast<uint8>(SupportedRHI);
		if (TransferObjects[RHIIndex])
		{
			return TransferObjects[RHIIndex];
		}
#endif
	}
	return nullptr;
}

bool FGPUTextureTransferModule::IsInitialized() const
{
#if DVP_SUPPORTED_PLATFORM
	return bInitialized;
#else
	return false;
#endif
}

bool FGPUTextureTransferModule::IsEnabled() const
{
	return CVarMediaIOEnableGPUDirect.GetValueOnAnyThread();
}

FGPUTextureTransferModule& FGPUTextureTransferModule::Get()
{
	return FModuleManager::LoadModuleChecked<FGPUTextureTransferModule>("GPUTextureTransfer");
}

bool FGPUTextureTransferModule::LoadGPUDirectBinary()
{
#if DVP_SUPPORTED_PLATFORM
	FString GPUDirectPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/NVIDIA/GPUDirect"), FPlatformProcess::GetBinariesSubdirectory());
	FPlatformProcess::PushDllDirectory(*GPUDirectPath);

	FString DVPDll;

	DVPDll = TEXT("dvp.dll");

	DVPDll = FPaths::Combine(GPUDirectPath, DVPDll);

	TextureTransferHandle = FPlatformProcess::GetDllHandle(*DVPDll);
	if (TextureTransferHandle == nullptr)
	{
		UE_LOG(LogGPUTextureTransfer, Display, TEXT("Failed to load required library %s. GPU Texture transfer will not be functional."), *DVPDll);
	}

	FPlatformProcess::PopDllDirectory(*GPUDirectPath);

#endif
	return !!TextureTransferHandle;
}


void FGPUTextureTransferModule::InitializeTextureTransfer()
{
#if DVP_SUPPORTED_PLATFORM

	bool bInitializationSuccess = true;

	static const TArray<FString> SupportedGPUPrefixes = {
		TEXT("RTX A4"),
		TEXT("RTX A5"),
		TEXT("RTX A6"),
		TEXT("Quadro")
	};

	bInitializationSuccess = CachedDriverInfo.IsNVIDIA() && !CachedDriverInfo.DeviceDescription.Contains(TEXT("Tesla"));

	const bool bRenderDocAttached = FParse::Param(FCommandLine::Get(), TEXT("AttachRenderDoc"));
	if (bRenderDocAttached)
	{
		bInitializationSuccess = false;
		// Render doc clashes with GPU Direct.
		UE_LOG(LogGPUTextureTransfer, Display, TEXT("GPU Texture Transfer disabled because render is attached."))
	}

	if (bInitializationSuccess)
	{
		bInitializationSuccess = false;
		for (const FString& GPUPrefix : SupportedGPUPrefixes)
		{
			if (CachedDriverInfo.DeviceDescription.Contains(GPUPrefix))
			{
				bInitializationSuccess = true;
				break;
			}
		}
	}
	if (!bInitializationSuccess)
	{
		return;
	}

	if (!GDynamicRHI)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(InitializeTextureTransfer)([this](FRHICommandListImmediate&)
	{
		UE::GPUTextureTransfer::TextureTransferPtr TextureTransfer;

		UE::GPUTextureTransfer::ERHI RHI = ConvertRHI(RHIGetInterfaceType());

		switch (RHI)
		{
		case UE::GPUTextureTransfer::ERHI::D3D11:
			TextureTransfer = MakeShared<UE::GPUTextureTransfer::Private::FD3D11TextureTransfer>();
			break;
		case UE::GPUTextureTransfer::ERHI::D3D12:
			TextureTransfer = MakeShared<UE::GPUTextureTransfer::Private::FD3D12TextureTransfer>();
			break;
		case UE::GPUTextureTransfer::ERHI::Vulkan:
			TextureTransfer = MakeShared<UE::GPUTextureTransfer::Private::FVulkanTextureTransfer>();
			break;
		default:
			ensureAlways(false);
			break;
		}

		UE::GPUTextureTransfer::FInitializeDMAArgs InitializeArgs;
		InitializeArgs.RHI = RHI;
		InitializeArgs.RHIDevice = GDynamicRHI->RHIGetNativeDevice();
		InitializeArgs.RHICommandQueue = GDynamicRHI->RHIGetNativeGraphicsQueue();
#if VULKAN_PLATFORM
		if (RHI == UE::GPUTextureTransfer::ERHI::Vulkan)
		{
			IVulkanDynamicRHI* DynRHI = GetIVulkanDynamicRHI();
			InitializeArgs.VulkanInstance = DynRHI->RHIGetVkInstance();
			FMemory::Memcpy(InitializeArgs.RHIDeviceUUID, DynRHI->RHIGetVulkanDeviceUUID(), 16);
		}
#endif

		const uint8 RHIIndex = static_cast<uint8>(RHI);
		UE_LOG(LogGPUTextureTransfer, Display, TEXT("Initializing GPU Texture transfer"));
		if (TextureTransfer->Initialize(InitializeArgs))
		{
			TransferObjects[RHIIndex] = TextureTransfer;
		}

		bInitialized = true;
	});
#endif // DVP_SUPPORTED_PLATFORM
}

void FGPUTextureTransferModule::UninitializeTextureTransfer()
{
#if DVP_SUPPORTED_PLATFORM
	for (uint8 RhiIt = 1; RhiIt < RHI_MAX; RhiIt++)
	{
		if (const UE::GPUTextureTransfer::TextureTransferPtr& TextureTransfer = TransferObjects[RhiIt])
		{
			TextureTransfer->Uninitialize();
		}
	}
#endif
}

void FGPUTextureTransferModule::OnEnableGPUDirectCVarChange(IConsoleVariable* ConsoleVariable)
{
	if (ConsoleVariable->GetBool() && !IsInitialized())
	{
		Initialize();
	}
}

namespace UE::GPUTextureTransfer
{
	struct FTextureTransfersWrapper
	{
		static constexpr uint8_t RHI_MAX = static_cast<uint8_t>(ERHI::RHI_MAX);
		TArray<ITextureTransfer*> Transfers;

		FTextureTransfersWrapper()
		{
			Transfers.SetNumUninitialized(RHI_MAX);

			for (uint8_t RhiIt = 0; RhiIt < RHI_MAX; RhiIt++)
			{
				Transfers[RhiIt] = nullptr;
			}
		}

		~FTextureTransfersWrapper()
		{
			// 0 is Invalid RHI
			for (uint8_t RhiIt = 1; RhiIt < RHI_MAX; RhiIt++)
			{
				ITextureTransfer* TextureTransfer = Transfers[RhiIt];
				if (TextureTransfer)
				{
					TextureTransfer->Uninitialize();
					delete TextureTransfer;
					Transfers[RhiIt] = nullptr;
				}
			}
		}

		void CleanupTextureTransfer(ITextureTransfer* TextureTransfer)
		{
			if (TextureTransfer)
			{
				for (uint8_t RhiIt = 1; RhiIt < RHI_MAX; RhiIt++)
				{
					if (Transfers[RhiIt] && Transfers[RhiIt] == TextureTransfer)
					{
						Transfers[RhiIt]->Uninitialize();
						delete Transfers[RhiIt];
						Transfers[RhiIt] = nullptr;
					}
				}
			}
		}

	} TextureTransfersWrapper;

	ITextureTransfer* GetTextureTransfer(const UE::GPUTextureTransfer::FInitializeDMAArgs& Args)
	{
#if DVP_SUPPORTED_PLATFORM
		const uint8_t RHIIndex = static_cast<uint8_t>(Args.RHI);

		if (TextureTransfersWrapper.Transfers[RHIIndex])
		{
			return TextureTransfersWrapper.Transfers[RHIIndex];
		}

		ITextureTransfer* TextureTransfer = nullptr;
		switch (Args.RHI)
		{
		case ERHI::D3D11:
			TextureTransfer = new Private::FD3D11TextureTransfer();
			break;
		case ERHI::D3D12:
			TextureTransfer = new Private::FD3D12TextureTransfer();
			break;
		default:
			return nullptr;
		}

		if ((Private::FTextureTransferBase*)(TextureTransfer)->Initialize(Args))
		{
			TextureTransfersWrapper.Transfers[RHIIndex] = TextureTransfer;
		}
		else
		{
			delete TextureTransfer;
		}

		return TextureTransfersWrapper.Transfers[RHIIndex];
#else
		return nullptr;
#endif
	}

	void CleanupTextureTransfer(ITextureTransfer* TextureTransfer)
	{
		TextureTransfersWrapper.CleanupTextureTransfer(TextureTransfer);
	}
}


IMPLEMENT_MODULE(FGPUTextureTransferModule, GPUTextureTransfer);
