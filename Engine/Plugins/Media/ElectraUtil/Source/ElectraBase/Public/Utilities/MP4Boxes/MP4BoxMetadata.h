// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utilities/MP4Boxes/MP4Boxes.h"

namespace Electra
{
	namespace UtilitiesMP4
	{

		/****************************************************************************************************************************************************/

		struct FMP4TrackMetadataCommon
		{
			BCP47::FLanguageTag LanguageTag;
			FString Name;
			FString HandlerName;
		};

	} // namespace UtilitiesMP4

} // namespace Electra
