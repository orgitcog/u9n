// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/BrowseToAssetOverrideSubsystem.h"

#include "Editor.h"
#include "GameFramework/Actor.h"

UBrowseToAssetOverrideSubsystem* UBrowseToAssetOverrideSubsystem::Get()
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UBrowseToAssetOverrideSubsystem>();
	}
	return nullptr;
}

FName UBrowseToAssetOverrideSubsystem::GetBrowseToAssetOverride(const UObject* Object)
{
	// Actors also allow this to be overridden per-instance via meta-data
	// If set, that takes priority over any per-class overrides
	if (const AActor* Actor = Cast<AActor>(Object))
	{
		const FString& ActorBrowseToAssetOverride = Actor->GetBrowseToAssetOverride();
		if (!ActorBrowseToAssetOverride.IsEmpty())
		{
			return *ActorBrowseToAssetOverride;
		}
	}

	// Walk the class hierarchy to see if there's a valid per-class override for this instance
	if (PerClassOverrides.Num() > 0)
	{
		for (const UClass* Class = Object->GetClass(); Class; Class = Class->GetSuperClass())
		{
			if (const FBrowseToAssetOverrideDelegate* CallbackPtr = PerClassOverrides.Find(Class->GetClassPathName()))
			{
				FName CallbackBrowseToAssetOverride = CallbackPtr->IsBound() ? CallbackPtr->Execute(Object) : FName();
				if (!CallbackBrowseToAssetOverride.IsNone())
				{
					return CallbackBrowseToAssetOverride;
				}
			}
		}
	}

	// Query all the class interfaces to see if there's a valid per-interface override for this instance
	if (PerInterfaceOverrides.Num() > 0)
	{
		UClass* ObjectClass = Object->GetClass();
		for (const FImplementedInterface& Interface : ObjectClass->Interfaces)
		{
			if (const FBrowseToAssetOverrideDelegate* CallbackPtr = PerInterfaceOverrides.Find(Interface.Class->GetClassPathName()))
			{
				FName CallbackBrowseToAssetOverride = CallbackPtr->IsBound() ? CallbackPtr->Execute(Object) : FName();
				if (!CallbackBrowseToAssetOverride.IsNone())
				{
					return CallbackBrowseToAssetOverride;
				}
			}
		}		
	}


	return FName();
}

void UBrowseToAssetOverrideSubsystem::RegisterBrowseToAssetOverrideForClass(const FTopLevelAssetPath& Class, FBrowseToAssetOverrideDelegate&& Callback)
{
	PerClassOverrides.Add(Class, MoveTemp(Callback));
}
	
void UBrowseToAssetOverrideSubsystem::UnregisterBrowseToAssetOverrideForClass(const FTopLevelAssetPath& Class)
{
	PerClassOverrides.Remove(Class);
}

void UBrowseToAssetOverrideSubsystem::RegisterBrowseToAssetOverrideForInterface(const FTopLevelAssetPath& Interface, FBrowseToAssetOverrideDelegate&& Callback)
{
	PerInterfaceOverrides.Add(Interface, MoveTemp(Callback));
}

void UBrowseToAssetOverrideSubsystem::UnregisterBrowseToAssetOverrideForInterface(const FTopLevelAssetPath& Interface)
{
	PerInterfaceOverrides.Remove(Interface);
}
