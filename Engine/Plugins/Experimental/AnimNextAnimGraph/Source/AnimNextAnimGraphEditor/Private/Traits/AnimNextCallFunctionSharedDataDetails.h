// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

namespace UE::AnimNext::Editor
{

class FCallFunctionSharedDataDetails : public IPropertyTypeCustomization
{
	// IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils) override;

	TSharedPtr<IPropertyHandle> PropertyHandle;
	TSharedPtr<IPropertyHandle> FunctionPropertyHandle;
	TSharedPtr<IPropertyHandle> FunctionHeaderPropertyHandle;
	TSharedPtr<IPropertyHandle> FunctionEventPropertyHandle;
};

}