// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "GPUTextureTransfer.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

#include <atomic>

DECLARE_LOG_CATEGORY_EXTERN(LogGPUTextureTransfer, Log, All);

namespace UE::GPUTextureTransfer
{
	using TextureTransferPtr = TSharedPtr<ITextureTransfer>;
}

class GPUTEXTURETRANSFER_API FGPUTextureTransferModule : public IModuleInterface
{
public:
	static FGPUTextureTransferModule& Get();

	UE_DEPRECATED(5.6, "Use IsInitialized() and IsEnabled() instead.")
	static bool IsAvailable();

	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	/** Load the DVP DLL and intialize the API.Note: This is a blocking call and could take upwards of 2s. */
	void Initialize();

	/** Get a texture transfer object that acts as a layer above the DVP library. */
	UE::GPUTextureTransfer::TextureTransferPtr GetTextureTransfer();

	/** Returns whether the DVP library was loaded and initialized. If this returns false, GetTextureTransfer will trigger the library to initialize. */
	bool IsInitialized() const;

	/** Returns whether GPUDirect is enabled. This reflects the value of MediaIO.EnableGPUDirect. You should check IsAvailable to know if the library is initialized. */
	bool IsEnabled() const;

private:
	/** Load the DVP dll. */
	bool LoadGPUDirectBinary();

	/** Initialize the DVP library and creates the ITextureTransfer objects. */
	void InitializeTextureTransfer();

	/** Clean up and uninitializes the DVP library. */
	void UninitializeTextureTransfer();

	/** Initializes the DVP library when MediaIO.EnableGPUDirect is set to 1. */
	void OnEnableGPUDirectCVarChange(class IConsoleVariable* ConsoleVariable);
	
private:
	/** Max number of RHIs that GPUTextureTransfer supports. */
	static constexpr uint8 RHI_MAX = static_cast<uint8>(UE::GPUTextureTransfer::ERHI::RHI_MAX);

	/** DVP DLL Handle. */
	void* TextureTransferHandle = nullptr;

	/** Tracks whether the dvp library and texture transfer objects were successfully created. */
	std::atomic<bool> bInitialized = false;
	
	/** Texture transfer objects. */
	TArray<UE::GPUTextureTransfer::TextureTransferPtr> TransferObjects;

	/** Cached information about the GPU Driver. */
	FGPUDriverInfo CachedDriverInfo;
};
