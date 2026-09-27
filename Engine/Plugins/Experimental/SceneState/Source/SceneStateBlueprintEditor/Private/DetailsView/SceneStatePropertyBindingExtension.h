// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingExtension.h"

namespace UE::SceneState::Editor
{

class FBindingExtension : public FPropertyBindingExtension
{
public:
	//~ Begin IDetailPropertyExtensionHandler
	virtual void ExtendWidgetRow(FDetailWidgetRow& InWidgetRow, const IDetailLayoutBuilder& InDetailBuilder, const UClass* InObjectClass, TSharedPtr<IPropertyHandle> InPropertyHandle) override;
	//~ End IDetailPropertyExtensionHandler
};

} // UE::SceneState::Editor
