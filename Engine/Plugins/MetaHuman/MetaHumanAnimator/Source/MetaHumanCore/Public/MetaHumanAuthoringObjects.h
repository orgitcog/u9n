// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"



class METAHUMANCORE_API FMetaHumanAuthoringObjects
{
public:

	static bool ArePresent();

	static bool FindObject(FString& InOutObjectPath);
	static bool FindObject(FString& InOutObjectPath, bool& bOutWasFound, bool& bOutHasMoved);

	template<class T>
	static bool FindObject(TSoftObjectPtr<T>& InSoftObjectPtr)
	{
		bool bWasFound = false;
		bool bHasMoved = false;
		return FindObject(InSoftObjectPtr, bWasFound, bHasMoved);
	}

	template<class T>
	static bool FindObject(TSoftObjectPtr<T>& InSoftObjectPtr, bool& bOutWasFound, bool& bOutHasMoved)
	{
		FSoftObjectPath SoftObjectPath = InSoftObjectPtr.ToSoftObjectPath();
		FString Path = SoftObjectPath.GetAssetPathString();

		bool bFindObjectFailed = FindObject(Path, bOutWasFound, bOutHasMoved);

		SoftObjectPath.SetPath(Path);
		InSoftObjectPtr = SoftObjectPath;

		return bFindObjectFailed;
	}
};
