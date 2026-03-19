// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/LoadUtils.h"

#include "AssetRegistry/AssetData.h"


namespace MutablePrivate
{
	UObject* LoadObject(const FAssetData& DataAsset)
	{
#if	WITH_EDITOR
		FCookLoadScope CookLoadScope(ECookLoadType::EditorOnly);
#endif
	
		return DataAsset.GetAsset();
	}


	UObject* LoadObject(const FSoftObjectPath& Path)
	{
#if	WITH_EDITOR
		FCookLoadScope CookLoadScope(ECookLoadType::EditorOnly);
#endif
	
		return Path.TryLoad();
	}
	

	UObject* LoadObject(const FSoftObjectPtr& SoftObject)
	{
#if	WITH_EDITOR
		FCookLoadScope CookLoadScope(ECookLoadType::EditorOnly);
#endif
	
		return SoftObject.LoadSynchronous();
	}
}
