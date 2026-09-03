// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoRTFM.h"

// We can't use Catch2Includes.h here, because it #includes more Unreal headers.
// Fortunately, we don't even need to run these tests; if this code compiles and links, 
// the test has passed.

namespace 
{

void OnAbort_MacroWorksWithNoAdditionalIncludes()
{
	AutoRTFM::Transact([&]
	{
		UE_AUTORTFM_ONABORT()
		{
		};
	});
}

void OnAbort_FunctionWorksWithNoAdditionalIncludes()
{
	AutoRTFM::Transact([&]
	{
		AutoRTFM::OnAbort([]
		{
		});
	});
}

void OnCommit_MacroWorksWithNoAdditionalIncludes()
{
	AutoRTFM::Commit([&]
	{
		UE_AUTORTFM_ONCOMMIT()
		{
		};
	});
}

void OnCommit_FunctionWorksWithNoAdditionalIncludes()
{
	AutoRTFM::Commit([&]
	{
		AutoRTFM::OnCommit([]
		{
		});
	});
}

}  // namespace
