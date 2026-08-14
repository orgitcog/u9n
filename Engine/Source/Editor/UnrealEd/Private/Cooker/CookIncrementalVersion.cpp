// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookTypes.h"

#include "Misc/Guid.h"

namespace UE::Cook
{

// Change CookIncrementalVersion to a new guid when all packages in an incremental cook need to be invalidated
FGuid CookIncrementalVersion( 0xD6B3741E, 0x72CD49EF, 0xBEDEFBA9, 0xDB7C61CF );
}
