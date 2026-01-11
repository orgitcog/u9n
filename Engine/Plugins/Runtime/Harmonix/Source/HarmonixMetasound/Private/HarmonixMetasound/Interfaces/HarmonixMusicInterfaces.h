// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"

namespace HarmonixMetasound
{
	namespace MusicAssetInterface
	{
		const FMetasoundFrontendVersion& GetVersion();
		Audio::FParameterInterfacePtr CreateInterface();
	}

	void RegisterHarmonixMetasoundMusicInterfaces();
}
