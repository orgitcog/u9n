// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "IPropertyTypeCustomization.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class SWidget;
class STextBlock;
template<typename OptionType> class SComboBox;

namespace UE::TakeRecorder
{
class FAudioInputChannelPropertyCustomization : public IPropertyTypeCustomization
{
public:

	//~ Begin IPropertyTypeCustomization Interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& ) {}
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils);
	//~ End IPropertyTypeCustomization Interface

private:

	TSharedPtr<IPropertyHandle> InputDeviceChannelHandle;
	TArray<TSharedPtr<int32>> InputDeviceChannelArray;
	TSharedPtr<STextBlock> InputChannelTitleBlock;
	TSharedPtr<SComboBox<TSharedPtr<int32>>> ChannelComboBox;
		
	void BuildInputChannelArray();
	void RebuildInputChannelArray();

	TSharedRef<SWidget> MakeInputChannelSelectorWidget();
};
}