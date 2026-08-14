// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/Handlers/PropertyAnimatorCoreFloatHandler.h"

bool UPropertyAnimatorCoreFloatHandler::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (InPropertyData.GetMemberPropertyTypeName() == NAME_Rotator)
	{
		return false;	
	}
	
	if (InPropertyData.IsA<FFloatProperty>())
	{
		return true;
	}

	return Super::IsPropertySupported(InPropertyData);
}

bool UPropertyAnimatorCoreFloatHandler::GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	OutValue.AddProperty(PropertyHash, EPropertyBagPropertyType::Float);

	float Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	OutValue.SetValueFloat(PropertyHash, Value);

	return true;
}

bool UPropertyAnimatorCoreFloatHandler::SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	TValueOrError<float, EPropertyBagResult> ValueResult = InValue.GetValueFloat(PropertyHash);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	float& NewValue = ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreFloatHandler::IsAdditiveSupported() const
{
	return true;
}

bool UPropertyAnimatorCoreFloatHandler::AddValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	const TValueOrError<float, EPropertyBagResult> ValueResult = InValue.GetValueFloat(PropertyHash);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	float Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	float NewValue = Value + ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreFloatHandler::SubtractValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	const TValueOrError<float, EPropertyBagResult> ValueResult = InValue.GetValueFloat(PropertyHash);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	float Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	float NewValue = Value - ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreFloatHandler::GetDefaultValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyHash(InPropertyData.GetLocatorPathHash());
	OutValue.AddProperty(PropertyHash, EPropertyBagPropertyType::Float);
	OutValue.SetValueFloat(PropertyHash, 0.f);
	return true;
}
