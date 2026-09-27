// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GenericPlatform/ITextToSpeechFactory.h"

class FTextToSpeechBase;
/** The platform default text to speech factory for Apple. */
class FAppleTextToSpeechFactory : public ITextToSpeechFactory
{
public:
	FAppleTextToSpeechFactory() = default;
	virtual ~FAppleTextToSpeechFactory() = default;

	// ITextToSpeechFactory
	virtual TSharedRef<FTextToSpeechBase> Create() override;
	// ~
};
typedef FAppleTextToSpeechFactory FPlatformTextToSpeechFactory;
