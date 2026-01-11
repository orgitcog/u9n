// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"

#include "IpAddressDetailsCustomization.generated.h"

USTRUCT(BlueprintType)
struct FDeviceAddress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = DeviceAddress)
	FString IpAddress;
};

class FIpAddressDetailsCustomization : public IPropertyTypeCustomization
{
public:
	FIpAddressDetailsCustomization();

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	FText OnGetDeviceAddress() const;
	bool OnDeviceAddressVerify(const FText& InText, FText& OutErrorText);
	void OnDeviceAddressCommited(const FText& InText, ETextCommit::Type CommitInfo);
	bool IsReadOnly() const;

	TSharedPtr<IPropertyHandle> DeviceAddressProperty;
	TRange<int32> IpAddressRange;
};
