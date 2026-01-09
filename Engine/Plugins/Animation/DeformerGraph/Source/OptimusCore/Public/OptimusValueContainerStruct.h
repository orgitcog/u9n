// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5
#include "CoreMinimal.h"
#endif

#include "OptimusDataType.h"
#include "StructUtils/PropertyBag.h"
#include "OptimusValueContainerStruct.generated.h"

USTRUCT()
struct FOptimusValueContainerStruct
{
	GENERATED_BODY()

	static OPTIMUSCORE_API const TCHAR* ValuePropertyName;
	
	UPROPERTY(EditAnywhere, Category="Value", meta=(FixedLayout, ShowOnlyInnerProperties))
	FInstancedPropertyBag Value;

	OPTIMUSCORE_API bool IsInitialized() const;
	OPTIMUSCORE_API void SetType(FOptimusDataTypeRef InDataType);
	OPTIMUSCORE_API void SetValue(FOptimusDataTypeRef InDataType, TArrayView<const uint8> InValue);
	OPTIMUSCORE_API FShaderValueContainer GetShaderValue(FOptimusDataTypeRef InDataType) const;
	OPTIMUSCORE_API FString GetValueAsString() const;
	
private:
	const FProperty* GetValueProperty() const;
	const uint8* GetValueMemory() const;

};
