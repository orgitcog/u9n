// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/StringView.h"
#include "IO/IoStatus.h"

namespace UE::IoStore { struct FOnDemandEndpointConfig; }
namespace UE::IoStore { struct FIasCacheConfig; }
namespace UE::IoStore { struct FOnDemandInstallCacheConfig; }

namespace UE::IoStore::Config
{

int64 ParseSizeParam(FStringView Value);

int64 ParseSizeParam(const TCHAR* CommandLine, const TCHAR* Param);

TIoStatusOr<FOnDemandEndpointConfig> TryParseEndpointConfig(const TCHAR* CommandLine);

FIasCacheConfig GetStreamingCacheConfig(const TCHAR* CommandLine);

TIoStatusOr<FOnDemandInstallCacheConfig> TryParseInstallCacheConfig(const TCHAR* CommandLine);

} // namespace UE::IoStore
