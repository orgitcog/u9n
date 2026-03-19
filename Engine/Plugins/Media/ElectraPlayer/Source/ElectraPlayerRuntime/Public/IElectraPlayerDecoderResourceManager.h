// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "IElectraDecoderResourceDelegate.h"
#include "MediaRendererBase.h"
#include "ParameterDictionary.h"
#include "MediaVideoDecoderOutput.h"

class IElectraPlayerAdapterDelegate;
class IElectraDecoderVideoOutput;

namespace Electra
{
class IVideoDecoderResourceDelegate;
}

class FElectraPlayerDecoderResourceManager
{
public:
	ELECTRAPLAYERRUNTIME_API static TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> GetDelegate();
	ELECTRAPLAYERRUNTIME_API static TSharedPtr<Electra::IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> CreatePlatformVideoDecoderResourceDelegate(TSharedPtr<IElectraPlayerAdapterDelegate, ESPMode::ThreadSafe> InElectraPlayerAdapterDelegate);
	ELECTRAPLAYERRUNTIME_API static bool SetupRenderBufferFromDecoderOutput(FString& OutErrorMessage, Electra::IMediaRenderer::IBuffer* InOutBufferToSetup, TSharedPtr<Electra::FParamDict, ESPMode::ThreadSafe> InOutBufferPropertes, TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, IElectraDecoderResourceDelegateBase::IDecoderPlatformResource* InPlatformSpecificResource);
	class FVideo
	{
	public:
		ELECTRAPLAYERRUNTIME_API static FVideoDecoderOutput* Create();
	};
};
