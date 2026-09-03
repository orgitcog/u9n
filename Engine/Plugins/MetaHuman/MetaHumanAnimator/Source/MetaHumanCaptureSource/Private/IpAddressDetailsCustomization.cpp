// Copyright Epic Games, Inc. All Rights Reserved.

#include "IpAddressDetailsCustomization.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "IpAddressDetailsCustomization"

FIpAddressDetailsCustomization::FIpAddressDetailsCustomization()
	: IpAddressRange(TRange<int32>::Inclusive(0, 255))
{
}

TSharedRef<IPropertyTypeCustomization> FIpAddressDetailsCustomization::MakeInstance()
{
	return MakeShared<FIpAddressDetailsCustomization>();
}

void FIpAddressDetailsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FIpAddressDetailsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	DeviceAddressProperty = PropertyHandle;

	ChildBuilder.AddProperty(PropertyHandle)
		.CustomWidget()
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Text(this, &FIpAddressDetailsCustomization::OnGetDeviceAddress)
			.OnVerifyTextChanged(this, &FIpAddressDetailsCustomization::OnDeviceAddressVerify)
			.OnTextCommitted(this, &FIpAddressDetailsCustomization::OnDeviceAddressCommited)
			.SelectAllTextWhenFocused(true)
			.RevertTextOnEscape(true)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsReadOnly(this, &FIpAddressDetailsCustomization::IsReadOnly)
		];
}

FText FIpAddressDetailsCustomization::OnGetDeviceAddress() const
{
	TArray<void*> RawValue;
	DeviceAddressProperty->AccessRawData(RawValue);

	if (RawValue.Num())
	{
		const FDeviceAddress* DeviceAddress = static_cast<const FDeviceAddress*>(RawValue[0]);
		return FText::FromString(DeviceAddress->IpAddress);
	}

	return FText::GetEmpty();
}

bool FIpAddressDetailsCustomization::OnDeviceAddressVerify(const FText& InText, FText& OutErrorText)
{
	FString Value = InText.ToString();

	if (Value.IsEmpty())
	{
		OutErrorText = LOCTEXT("DeviceAddress_Empty", "Device Address property must not be empty");
		return false;
	}

	constexpr uint16 IPv4AddressMaxSize = 15;
	if (Value.Len() > IPv4AddressMaxSize)
	{
		OutErrorText = LOCTEXT("DeviceAddress_InvalidSize", "Device Address contains too many characters");
		return false;
	}

	TArray<FString> Components;
	Value.ParseIntoArray(Components, TEXT("."), false);

	if (Components.Num() != 4)
	{
		OutErrorText = LOCTEXT("DeviceAddress_InvalidFormat", "Invalid format for Device Address property");
		return false;
	}

	for (const FString& Component : Components)
	{
		if (!Component.IsNumeric())
		{
			OutErrorText = LOCTEXT("DeviceAddress_Numeric", "Device Address property must only contain numbers");
			return false;
		}

		const int32 ComponentNum = FCString::Atoi(*Component);
		if (!IpAddressRange.Contains(ComponentNum))
		{
			OutErrorText = LOCTEXT("DeviceAddress_Numeric_NumbersInRange", "Device Address property must only contain numbers in range [0-255]");
			return false;
		}
	}

	return true;
}

void FIpAddressDetailsCustomization::OnDeviceAddressCommited(const FText& InText, ETextCommit::Type CommitInfo)
{
	TArray<void*> RawValue;
	DeviceAddressProperty->AccessRawData(RawValue);

	FString Value = InText.ToString();

	if (RawValue.Num())
	{
		GEditor->BeginTransaction(FText::Format(LOCTEXT("SetIpAddressProperty", "Edit {0}"), DeviceAddressProperty->GetPropertyDisplayName()));

		DeviceAddressProperty->NotifyPreChange();

		FDeviceAddress* DeviceAddress = static_cast<FDeviceAddress*>(RawValue[0]);
		DeviceAddress->IpAddress = MoveTemp(Value);

		DeviceAddressProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		DeviceAddressProperty->NotifyFinishedChangingProperties();

		GEditor->EndTransaction();
	}
}

bool FIpAddressDetailsCustomization::IsReadOnly() const
{
	return !DeviceAddressProperty->IsEditable();
}

#undef LOCTEXT_NAMESPACE