// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IDetailLayoutBuilder;
class IAudioPropertiesDetailsInjector;

class FSoundBaseDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual ~FSoundBaseDetails();

	virtual void PendingDelete() override;

private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	void InjectPropertySheetView(IDetailLayoutBuilder& DetailBuilder);

	TSharedPtr<IAudioPropertiesDetailsInjector> AudioPropertiesInjector;
};
