// Copyright Epic Games, Inc. All Rights Reserved.

#include "IElectraPlayerDecoderResourceManager.h"
#include "Decoder/ElectraDecoderResourceManager.h"
#include "ElectraPlayerPlatform.h"
#include "Renderer/RendererVideo.h"


TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> FElectraPlayerDecoderResourceManager::GetDelegate()
{
	return Electra::FPlatformElectraDecoderResourceManager::GetDelegate();
}

TSharedPtr<Electra::IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> FElectraPlayerDecoderResourceManager::CreatePlatformVideoDecoderResourceDelegate(TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> InElectraPlayerAdapterDelegate)
{
	return Electra::PlatformCreateVideoDecoderResourceDelegate(InElectraPlayerAdapterDelegate);
}

bool FElectraPlayerDecoderResourceManager::SetupRenderBufferFromDecoderOutput(FString& OutErrorMessage, Electra::IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IElectraDecoderResourceDelegateBase::IDecoderPlatformResource* InPlatformSpecificResource)
{
	return Electra::FPlatformElectraDecoderResourceManager::SetupRenderBufferFromDecoderOutput(OutErrorMessage, InOutBufferToSetup, InOutBufferPropertes, InDecoderOutput, InPlatformSpecificResource);
}

FVideoDecoderOutput* FElectraPlayerDecoderResourceManager::FVideo::Create()
{
	return FElectraPlayerPlatformVideoDecoderOutputFactory::Create();
}
