// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#include "Net/Core/Connection/NetEnums.h"
#include "ProfilingDebugging/CsvProfilerConfig.h"

namespace UE::Net
{

/** Returns if the preferred replication system should be Iris. */
IRISCORE_API bool ShouldUseIrisReplication();

/** Set if the preferred replication system should be Iris or not. */
IRISCORE_API void SetUseIrisReplication(bool EnableIrisReplication);

/** Returns what replication sytem was set to be used by the cmdline. Returns Default when the command line was not set. */
IRISCORE_API EReplicationSystem GetUseIrisReplicationCmdlineValue();

}

/* NetBitStreamReader/Writer validation support */
#ifndef UE_NETBITSTREAMWRITER_VALIDATE
#define UE_NETBITSTREAMWRITER_VALIDATE !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif

#ifndef UE_NETBITSTREAMREADER_VALIDATE
#define UE_NETBITSTREAMREADER_VALIDATE !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif

/** CSV stats. */
#ifndef UE_NET_IRIS_CSV_STATS
#	define UE_NET_IRIS_CSV_STATS CSV_PROFILER_STATS
#endif

/** Enables code that detects non-thread safe access to network data */
#ifndef UE_NET_THREAD_SAFETY_CHECK
#	define UE_NET_THREAD_SAFETY_CHECK DO_CHECK
#endif
