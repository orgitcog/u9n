// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "Templates/SharedPointerFwd.h"

class FJsonObject;

namespace UE::DataLinkOAuth
{
	DATALINKOAUTH_API TSharedPtr<FJsonObject> ResponseStringToJsonObject(FStringView InResponseString);
}
