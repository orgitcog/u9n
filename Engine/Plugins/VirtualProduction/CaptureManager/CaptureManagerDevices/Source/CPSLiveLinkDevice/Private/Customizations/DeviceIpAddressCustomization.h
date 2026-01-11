// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"

#include "DeviceIpAddressCustomization.generated.h"

USTRUCT(BlueprintType)
struct FDeviceIpAddress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DeviceAddress, DisplayName="Ip Address")
	FString IpAddressString;
};

class FDeviceIpAddressCustomization : public IPropertyTypeCustomization
{
public:
	FDeviceIpAddressCustomization();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	static TOptional<FText> VerifyIpAddress(const FString& InIpAddress, TRange<int32> InIpAddressRange);

private:
	FText OnGetDeviceIpAddress() const;
	bool OnDeviceIpAddressVerify(const FText& InText, FText& OutErrorText);
	void OnDeviceIpAddressCommited(const FText& InText, ETextCommit::Type CommitInfo);
	bool IsReadOnly() const;

	TSharedPtr<IPropertyHandle> DeviceIpAddressProperty;
	TRange<int32> IpAddressRange;
};
