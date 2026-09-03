// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundChannelAgnosticType.h"
#include "TypeFamily/TypeFamily.h"
#include "TypeFamily/ChannelTypeFamily.h"

namespace Metasound
{
	// TODO: Pass this in via the operator settings some how. possibly on the environment.
	//static Audio::TFamilyRegistry<Audio::FChannelTypeFamily> GetChannelTypeRegistry()
	//{
	//	using namespace Audio;
	//	return TFamilyRegistry<FChannelTypeFamily>(GetChannelRegistry());
	//}

	/**
	 * Returns the default (i.e. default constructed) format of a CAT. Static.
	 * @return Name to Pass to Registry. 
	 */
	FName FChannelAgnosticType::GetDefaultCatFormat()
	{
		static const FName DefaultFormat(TEXT("Mono")); // 1 channel, Mono buffer.
		check(Audio::GetChannelRegistry().FindConcreteChannel(DefaultFormat) != nullptr);
		return DefaultFormat;
	}

	FChannelAgnosticType::FChannelAgnosticType(const FOperatorSettings& InSettings, const FName& InChannelTypeName)
		: FChannelAgnosticType(InSettings.GetNumFramesPerBlock(), InChannelTypeName)
	{}
	
	FChannelAgnosticType::FChannelAgnosticType(const int32 InNumFramesPerBlock, const FName& InChannelTypeName)
		: Super(*Audio::GetChannelRegistry().FindConcreteChannel(InChannelTypeName), InNumFramesPerBlock)
	{}
}

// Register.
REGISTER_METASOUND_DATATYPE(Metasound::FChannelAgnosticType, "Cat");