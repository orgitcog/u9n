// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"

#include "PropertyVisibilityOverrideSubsystem.generated.h"

class FProperty;
class UObject;

/** Bindable event for external objects to hook into ControlRig-level execution */
DECLARE_DELEGATE_RetVal_OneParam(bool, FShouldHidePropertyDelegate, const FProperty*);

UCLASS(MinimalAPI)
class UPropertyVisibilityOverrideSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UNREALED_API static UPropertyVisibilityOverrideSubsystem* Get();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	UNREALED_API void RegisterShouldHidePropertyDelegate(const FName& DelegateName, const FShouldHidePropertyDelegate& Delegate);
	UNREALED_API void UnregisterShouldHidePropertyDelegate(const FName& DelegateName);

	UNREALED_API virtual bool ShouldHideProperty(const FProperty* Property) const;

private:
	TMap<FName, FShouldHidePropertyDelegate> ShouldHidePropertyDelegates;
};
