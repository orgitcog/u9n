// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class AActor;
class IPropertyHandle;

/** Used to customize spline path modifier properties in details panel */
class FActorModifierEditorSplinePathModifierDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FActorModifierEditorSplinePathModifierDetailCustomization>();
	}

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	//~ End IDetailCustomization

private:
	static bool OnFilterSplineActor(const AActor* InActor);
};
