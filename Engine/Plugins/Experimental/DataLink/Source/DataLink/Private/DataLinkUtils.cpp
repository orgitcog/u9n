// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkUtils.h"
#include "DataLinkLog.h"
#include "DataLinkSinkObject.h"
#include "Engine/Engine.h"
#include "IDataLinkSinkProvider.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

bool UE::DataLink::ReplaceObject(UObject*& InOutObject, UObject* InOuter, UClass* InClass)
{
	// Check if the object class already matches the new class
	if (InOutObject && InOutObject->GetClass() == InClass)
	{
		UE_LOG(LogDataLink, Log, TEXT("ReplaceObject did not take place as '%s' is already of class %s.")
			, *InOutObject->GetName()
			, *GetNameSafe(InClass));
		return false;
	}

	UObject* const OldObject = InOutObject;
	InOutObject = nullptr;

	// Save the current object name before renaming it
	const FName ObjectName = InOutObject ? InOutObject->GetFName() : NAME_None;

	// Discard current object
	if (InOutObject)
	{
		UObject* const NewOuter = GetTransientPackage();
		const FName UniqueName = MakeUniqueObjectName(NewOuter, InOutObject->GetClass(), *(TEXT("TRASH_") + InOutObject->GetName()));
		InOutObject->Rename(*UniqueName.ToString(), NewOuter, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		InOutObject->MarkAsGarbage();
		InOutObject = nullptr;
	}

	// Set the new class (only if new class is valid)
	// The operation is still considered valid if InClass is null. This 
	if (InClass)
	{
		const EObjectFlags ObjectFlags = InOuter ? InOuter->GetMaskedFlags(RF_PropagateToSubObjects) : RF_NoFlags;

		InOutObject = NewObject<UObject>(InOuter, InClass, ObjectName, ObjectFlags);

		if (OldObject && GEngine)
		{
			TMap<UObject*, UObject*> ReplacementMap;
			ReplacementMap.Add(OldObject, InOutObject);
			GEngine->NotifyToolsOfObjectReplacement(ReplacementMap);
		}
	}

	return true;
}

TSharedPtr<FDataLinkSink> UE::DataLink::TryGetSink(TScriptInterface<IDataLinkSinkProvider> InSinkProvider)
{
	if (IDataLinkSinkProvider* SinkProvider = InSinkProvider.GetInterface())
	{
		if (TSharedPtr<FDataLinkSink> Sink = SinkProvider->GetSink())
		{
			return Sink;
		}

		if (const UDataLinkSinkObject* SinkObject = SinkProvider->GetSinkObject())
		{
			return SinkObject->GetSink();
		}
	}
	else if (UObject* Object = InSinkProvider.GetObject())
	{
		if (Object->GetClass()->ImplementsInterface(UDataLinkSinkProvider::StaticClass()))
		{
			if (const UDataLinkSinkObject* SinkObject = IDataLinkSinkProvider::Execute_GetSinkObject(Object))
			{
				return SinkObject->GetSink();
			}
		}
	}

	return nullptr;
}
