// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "SmartObjectSubsystem.h"
#endif
#include "EnvQueryItemType_SmartObject.generated.h"


struct FSmartObjectSlotEQSItem
{
	FVector Location;
	FSmartObjectHandle SmartObjectHandle;
	FSmartObjectSlotHandle SlotHandle;

	FSmartObjectSlotEQSItem(const FVector InLocation, const FSmartObjectHandle InSmartObjectHandle, const FSmartObjectSlotHandle InSlotHandle)
		: Location(InLocation), SmartObjectHandle(InSmartObjectHandle), SlotHandle(InSlotHandle)
	{}

	FORCEINLINE operator FVector() const { return Location; }

	bool operator==(const FSmartObjectSlotEQSItem& Other) const
	{
		return SmartObjectHandle == Other.SmartObjectHandle && SlotHandle == Other.SlotHandle;
	}
};


UCLASS(MinimalAPI)
class UEnvQueryItemType_SmartObject : public UEnvQueryItemType_VectorBase
{
	GENERATED_BODY()
public:
	typedef FSmartObjectSlotEQSItem FValueType;

	SMARTOBJECTSMODULE_API UEnvQueryItemType_SmartObject();

	static SMARTOBJECTSMODULE_API const FSmartObjectSlotEQSItem& GetValue(const uint8* RawData);
	static SMARTOBJECTSMODULE_API void SetValue(uint8* RawData, const FSmartObjectSlotEQSItem& Value);

	SMARTOBJECTSMODULE_API virtual FVector GetItemLocation(const uint8* RawData) const override;
};
