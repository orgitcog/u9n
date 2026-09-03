// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AssetAccessRestrictions.h"


TDelegate<bool(FStringView)> UE::AssetAccessRestrictions::IsPathAllowedToReferenceEpicInternalAssets;
