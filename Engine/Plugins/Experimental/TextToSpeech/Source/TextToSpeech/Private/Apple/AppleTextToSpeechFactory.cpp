// Copyright Epic Games, Inc. All Rights Reserved.
#include "Apple/AppleTextToSpeechFactory.h"
#include "GenericPlatform/ITextToSpeechFactory.h"
#include "Apple/AppleTextToSpeech.h"

TSharedRef<FTextToSpeechBase> FAppleTextToSpeechFactory::Create()
{
	return MakeShared<FAppleTextToSpeech>();
}
