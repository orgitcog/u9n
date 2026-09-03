// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/PropertyVisibilityOverrideSubsystem.h"

#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Math/NumericLimits.h"

UPropertyVisibilityOverrideSubsystem* UPropertyVisibilityOverrideSubsystem::Get()
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UPropertyVisibilityOverrideSubsystem>();
	}

	return nullptr;
}

void UPropertyVisibilityOverrideSubsystem::RegisterShouldHidePropertyDelegate(const FName& DelegateName, const FShouldHidePropertyDelegate& Delegate)
{
	ShouldHidePropertyDelegates.Add(DelegateName, Delegate);
}

void UPropertyVisibilityOverrideSubsystem::UnregisterShouldHidePropertyDelegate(const FName& DelegateName)
{
	ShouldHidePropertyDelegates.Remove(DelegateName);
}

bool UPropertyVisibilityOverrideSubsystem::ShouldHideProperty(const FProperty* Property) const
{
	for (auto&& DelegatePair : ShouldHidePropertyDelegates)
	{
		const FShouldHidePropertyDelegate& Delegate = DelegatePair.Value;
		if (Delegate.IsBound() && Delegate.Execute(Property))
		{
			return true;
		}
	}

	return false;
}
