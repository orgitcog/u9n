// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChannelAgnostic/ChannelAgnosticType.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "DSP/MultiMono.h"

namespace Metasound
{
	class FChannelAgnosticType : public Audio::FChannelAgnosticType
	{
	public:
		using Super = Audio::FChannelAgnosticType;
		using Super::Super;

		/**
		 * Returns the default (i.e. default constructed) format of a CAT.
		 * @return Name to Pass to Registry. 
		 */
		static FName GetDefaultCatFormat();

		explicit FChannelAgnosticType(const FOperatorSettings& InSettings, const FName& InChannelTypeName = GetDefaultCatFormat());
		
		explicit FChannelAgnosticType(const int32 InNumFramesPerBlock, const FName& InChannelTypeName = GetDefaultCatFormat());
				
		/* FLiteral compatible Constructor
		 * TODO: Literals only support FString and not FNames, so there is a conversion here.
		 */
		explicit FChannelAgnosticType(const FOperatorSettings& InSettings, const FString& InChannelTypeName)
			: FChannelAgnosticType(InSettings, FName(*InChannelTypeName))
		{}
	};
	

	
}

// Declare it.
DECLARE_METASOUND_DATA_REFERENCE_TYPES(Metasound::FChannelAgnosticType, METASOUNDEXPERIMENTALRUNTIME_API,
	FChannelAgnosticTypeTypeInfo, FChannelAgnosticTypeReadRef, FChannelAgnosticTypeWriteRef);
