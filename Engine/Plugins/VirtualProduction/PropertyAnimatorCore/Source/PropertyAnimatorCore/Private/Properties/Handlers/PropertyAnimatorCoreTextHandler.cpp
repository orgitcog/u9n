// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/Handlers/PropertyAnimatorCoreTextHandler.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/TextProperty.h"

bool UPropertyAnimatorCoreTextHandler::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (InPropertyData.IsA<FTextProperty>())
	{
		return true;
	}

	return Super::IsPropertySupported(InPropertyData);
}

bool UPropertyAnimatorCoreTextHandler::GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	OutValue.AddProperty(PropertyHash, EPropertyBagPropertyType::Text);

	FText Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	OutValue.SetValueText(PropertyHash, Value);

	return true;
}

bool UPropertyAnimatorCoreTextHandler::SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	TValueOrError<FText, EPropertyBagResult> ValueResult = InValue.GetValueText(PropertyHash);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	FText& NewValue = ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreTextHandler::IsAdditiveSupported() const
{
	return true;
}

bool UPropertyAnimatorCoreTextHandler::AddValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	const TValueOrError<FText, EPropertyBagResult> ValueResult = InValue.GetValueText(PropertyHash);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	FText Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	// Append
	FText NewValue = FText::FromString(Value.ToString() + ValueResult.GetValue().ToString());
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreTextHandler::SubtractValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	const TValueOrError<FText, EPropertyBagResult> ValueResult = InValue.GetValueText(PropertyHash);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	FText Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	// Trim end
	FString StringValue = Value.ToString();
	const FString StringValueResult = ValueResult.GetValue().ToString();
	if (StringValue.EndsWith(StringValueResult))
	{
		StringValue.LeftChopInline(StringValueResult.Len());
	}

	FText NewValue = FText::FromString(StringValue);
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreTextHandler::GetDefaultValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	OutValue.AddProperty(PropertyHash, EPropertyBagPropertyType::Text);
	OutValue.SetValueText(PropertyHash, FText::GetEmpty());
	return true;
}
