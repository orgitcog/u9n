// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCommon.h"

namespace unsync {

struct FCmdVerifyOptions
{
	FPath			   Input;
};

int32 CmdVerify(const FCmdVerifyOptions& Options);

}  // namespace unsync
