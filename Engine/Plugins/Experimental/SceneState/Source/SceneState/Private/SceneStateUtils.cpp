// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateUtils.h"
#include "Engine/Engine.h"
#include "SceneStateLog.h"
#include "SceneStateRange.h"
#include "StructUtils/InstancedStructContainer.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

namespace UE::SceneState
{

TArray<FStructView> GetStructViews(FInstancedStructContainer& InStructContainer, FSceneStateRange InRange)
{
	if (InRange.Count == 0)
	{
		return {};
	}

	if (!InStructContainer.IsValidIndex(InRange.Index) || !InStructContainer.IsValidIndex(InRange.GetLastIndex()))
	{
		UE_LOG(LogSceneState, Error, TEXT("GetStructViews failed. Range [%d, %d] out of bounds. Struct Container Num: %d")
			, InRange.Index
			, InRange.GetLastIndex()
			, InStructContainer.Num());
		return {};
	}

	TArray<FStructView> StructViews;
	StructViews.Reserve(InRange.Count);

	for (int32 Index = InRange.Index; Index <= InRange.GetLastIndex(); ++Index)
	{
		StructViews.Add(InStructContainer[Index]);
	}

	return StructViews;
}

TArray<FStructView> GetStructViews(FInstancedStructContainer& InStructContainer)
{
	FSceneStateRange Range;
	Range.Index = 0;
	Range.Count = InStructContainer.Num();
	return GetStructViews(InStructContainer, Range);
}

TArray<FConstStructView> GetConstStructViews(const FInstancedStructContainer& InStructContainer, FSceneStateRange InRange)
{
	FInstancedStructContainer& StructContainer = const_cast<FInstancedStructContainer&>(InStructContainer);
	return TArray<FConstStructView>(GetStructViews(StructContainer, InRange));
}

TArray<FConstStructView> GetConstStructViews(const FInstancedStructContainer& InStructContainer)
{
	FSceneStateRange Range;
	Range.Index = 0;
	Range.Count = InStructContainer.Num();
	return GetConstStructViews(InStructContainer, Range);
}

void DiscardObject(UObject* InObjectToDiscard)
{
	if (InObjectToDiscard)
	{
		UObject* NewOuter = GetTransientPackage();
		FName UniqueName = MakeUniqueObjectName(NewOuter, InObjectToDiscard->GetClass(), *(TEXT("TRASH_") + InObjectToDiscard->GetName()));
		InObjectToDiscard->Rename(*UniqueName.ToString(), NewOuter, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		InObjectToDiscard->MarkAsGarbage();
	}
}

UObject* DiscardObject(UObject* InOuter, const TCHAR* InObjectName, TFunctionRef<void(UObject*)> InOnPreDiscardOldObject)
{
	if (UObject* OldObject = StaticFindObject(UObject::StaticClass(), InOuter, InObjectName))
	{
		InOnPreDiscardOldObject(OldObject);
		DiscardObject(OldObject);
	}
	return nullptr;
}

bool ReplaceObject(UObject*& InOutObject
	, UObject* InOuter
	, UClass* InClass
	, const TCHAR* InObjectName
	, const TCHAR* InContextName
	, TFunctionRef<void(UObject*)> InOnPreDiscardOldObject)
{
	if (!InOuter)
	{
		UE_LOG(LogSceneState, Error, TEXT("ReplaceObjectSafe did not take place (Context: %s). Outer is invalid."), InContextName);
		return false;
	}

	if (!InObjectName)
	{
		UE_LOG(LogSceneState, Error, TEXT("ReplaceObjectSafe did not take place (Context: %s). Object Name is invalid."), InContextName);
		return false;
	}

	if (InOutObject && InOutObject->GetName() != InObjectName)
	{
		UE_LOG(LogSceneState, Error, TEXT("ReplaceObjectSafe did not take place (Context: %s). Object Name '%s' does not match existing object name '%s'.")
			, InContextName
			, InObjectName
			, *InOutObject->GetName());
		return false;
	}

	if (InOutObject && InClass && InOutObject->GetClass() == InClass)
	{
		UE_LOG(LogSceneState, Log, TEXT("ReplaceObjectSafe did not take place (Context: %s). '%s' (%p) as is already of class %s.")
			, InContextName
			, *InOutObject->GetName()
			, InOutObject
			, *InClass->GetName());
		return false;
	}

	EObjectFlags MaskedOuterFlags = InOuter->GetMaskedFlags(RF_PropagateToSubObjects);

	UObject* OldObject = DiscardObject(InOuter, InObjectName, InOnPreDiscardOldObject);

	if (InClass)
	{
		InOutObject = NewObject<UObject>(InOuter, InClass, InObjectName, MaskedOuterFlags);

		if (OldObject && GEngine)
		{
			TMap<UObject*, UObject*> ReplacementMap;
			ReplacementMap.Add(OldObject, InOutObject);
			GEngine->NotifyToolsOfObjectReplacement(ReplacementMap);
		}
	}
	else
	{
		InOutObject = nullptr;
	}

	return true;
}

} // UE::SceneState
