// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGData.h"
#include "PCGParamData.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGPropertyHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/IPCGAttributeAccessorTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"
#include "Metadata/Accessors/PCGPropertyAccessor.h"
#include "Metadata/Accessors/PCGAttributeExtractor.h"

#include "StructUtils/UserDefinedStruct.h"
#include "UObject/EnumProperty.h"

namespace PCGAttributeAccessorHelpers
{
	TUniquePtr<IPCGAttributeAccessor> CreateChainAccessor(TUniquePtr<IPCGAttributeAccessor> InAccessor, FName Name, bool& bOutSuccess)
	{
		if (!InAccessor.IsValid())
		{
			bOutSuccess = false;
			return TUniquePtr<IPCGAttributeAccessor>();
		}

		bOutSuccess = true;

		auto Chain = [&Accessor = InAccessor, Name, &bOutSuccess](auto Dummy) -> TUniquePtr<IPCGAttributeAccessor>
		{
			using AccessorType = decltype(Dummy);

			if constexpr (PCG::Private::IsOfTypes<AccessorType, FVector2D, FVector, FVector4>())
			{
				return PCGAttributeExtractor::CreateVectorExtractor<AccessorType>(std::move(Accessor), Name, bOutSuccess);
			}
			else if constexpr (PCG::Private::IsOfTypes<AccessorType, FTransform>())
			{
				return PCGAttributeExtractor::CreateTransformExtractor(std::move(Accessor), Name, bOutSuccess);
			}
			else if constexpr (PCG::Private::IsOfTypes<AccessorType, FQuat>())
			{
				return PCGAttributeExtractor::CreateQuatExtractor(std::move(Accessor), Name, bOutSuccess);
			}
			else if constexpr (PCG::Private::IsOfTypes<AccessorType, FRotator>())
			{
				return PCGAttributeExtractor::CreateRotatorExtractor(std::move(Accessor), Name, bOutSuccess);
			}
			else
			{
				bOutSuccess = false;
				return std::move(Accessor);
			}
		};

		return PCGMetadataAttribute::CallbackWithRightType(InAccessor->GetUnderlyingType(), Chain);
	}
	
	TUniquePtr<IPCGAttributeAccessor> CreateChainAccessor(TUniquePtr<IPCGAttributeAccessor> InAccessor, const FPCGAttributePropertySelector& InSelector, bool& bOutSuccess, bool bQuiet)
	{
		if (!InAccessor.IsValid())
		{
			bOutSuccess = false;
			return {};
		}
		
		bOutSuccess = true;
		for (const FString& ExtraName : InSelector.GetExtraNames())
		{
			InAccessor = CreateChainAccessor(std::move(InAccessor), FName(ExtraName), bOutSuccess);
			if (!bOutSuccess)
			{
				if (!bQuiet)
				{
					UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateChainAccessor] Extra selectors don't match existing properties."));
				}
				
				return {};
			}
		}

		return InAccessor;
	}

	// Empty signature to passthrough types to functors.
	template <typename T>
	struct Signature
	{
		using Type = T;
	};

	template <typename Func>
	decltype(auto) DispatchPropertyTypes(const FProperty* InProperty, Func&& Functor)
	{
		// Use the FPCGPropertyPathAccessor (on soft object path) as dummy type to get the functor return type
		// because this accessor takes a generic FProperty, while others take a more specialized type (like FNumericProperty).
		using ReturnType = decltype(Functor(Signature<FPCGPropertyPathAccessor<FSoftObjectPath>>{}, InProperty));

		if (!InProperty)
		{
			return ReturnType{};
		}

		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(InProperty))
		{
			if (NumericProperty->IsFloatingPoint())
			{
				// As floating properties are mostly all double in UE, convert float to double attributes.
				return Functor(Signature<FPCGNumericPropertyAccessor<double>>{}, NumericProperty);
			}
			else if (NumericProperty->IsInteger())
			{
				// But for int32/int64 we can distinguish between the two. Everything of size 32 or less is will be an int32, 64bits integers will be int64.
				if (NumericProperty->IsA<FInt64Property>() || NumericProperty->IsA<FUInt64Property>())
				{
					return Functor(Signature<FPCGNumericPropertyAccessor<int64>>{}, NumericProperty);
				}
				else
				{
					return Functor(Signature<FPCGNumericPropertyAccessor<int32>>{}, NumericProperty);
				}
			}
		}
		else if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyAccessor<bool, FBoolProperty>>{}, BoolProperty);
		}
		else if (const FStrProperty* StringProperty = CastField<FStrProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyAccessor<FString, FStrProperty>>{}, StringProperty);
		}
		else if (const FNameProperty* NameProperty = CastField<FNameProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyAccessor<FName, FNameProperty>>{}, NameProperty);
		}
		else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
		{
			return Functor(Signature<FPCGEnumPropertyAccessor>{}, EnumProperty);
		}
		else if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertySoftClassPathAccessor>{}, SoftClassProperty);
		}
		else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertySoftObjectPathAccessor>{}, SoftObjectProperty);
		}
		else if (const FClassProperty* ClassProperty = CastField<FClassProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyObjectPtrAccessor<FClassProperty>>{}, ClassProperty);
		}
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyObjectPtrAccessor<FObjectProperty>>{}, ObjectProperty);
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
		{
			if (StructProperty->Struct == TBaseStructure<FVector>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FVector>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FVector4>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FVector4>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FQuat>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FQuat>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FTransform>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FTransform>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FRotator>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FVector2D>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FSoftObjectPath>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FSoftObjectPath>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FSoftClassPath>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FSoftClassPath>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				return Functor(Signature<FPCGLinearColorAccessor>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FColor>::Get())
			{
				return Functor(Signature<FPCGColorAccessor>{}, StructProperty);
			}
			else if (StructProperty->Struct && StructProperty->Struct->IsChildOf(FPCGAttributePropertySelector::StaticStruct()))
			{
				return Functor(Signature<FPCGAttributePropertySelectorAccessor>{}, StructProperty);
			}
		}

		return ReturnType{};
	}

	template <typename Func>
	decltype(auto) DispatchPropertyTypes(const FName InPropertyName, const UStruct* InStruct, Func&& Functor)
	{
		if (InStruct)
		{
			if (const FProperty* Property = InStruct->FindPropertyByName(InPropertyName))
			{
				return DispatchPropertyTypes(Property, std::forward<Func>(Functor));
			}
		}

		using ReturnType = decltype(Functor(static_cast<FPCGPropertyPathAccessor<FSoftObjectPath>*>(nullptr), static_cast<const FProperty*>(nullptr)));
		return ReturnType{};
	}

	bool GetPropertyChain(const TArray<FName>& InPropertyNames, const UStruct* InStruct, TArray<const FProperty*>& OutProperties)
	{
		check(InStruct);

		const UStruct* CurrentStruct = InStruct;
		OutProperties.Reserve(InPropertyNames.Num());

		for (int32 i = 0; i < InPropertyNames.Num(); ++i)
		{
			const FProperty* Property = nullptr;
			const FName PropertyName = InPropertyNames[i];

			// Try to get the property. If it is coming from a user struct, we need to have a special search because the property name might be mangled.
			if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentStruct))
			{
				Property = PCGPropertyHelpers::FindPropertyInUserDefinedStruct(UserDefinedStruct, PropertyName);
			}
			else
			{
				Property = FindFProperty<FProperty>(CurrentStruct, PropertyName);
			}

			if (!Property)
			{
				UE_LOG(LogPCG, Error, TEXT("Property '%s' does not exist in %s."), *PropertyName.ToString(), *CurrentStruct->GetName());
				return false;
			}

			OutProperties.Add(Property);

			// Check for a struct or object for all properties except the last one.
			if (i < InPropertyNames.Num() - 1)
			{
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					CurrentStruct = StructProperty->Struct;
				}
				else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					CurrentStruct = ObjectProperty->PropertyClass;
				}
				else
				{
					UE_LOG(LogPCG, Error, TEXT("Property '%s' does exist in % s, but is not extractable."), *PropertyName.ToString(), *CurrentStruct->GetName());
					return false;
				}
			}
		}

		return true;
	}
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyAccessor(const FProperty* InProperty)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InProperty, [](auto SignatureDummy, const auto* TypedProperty) -> TUniquePtr<IPCGAttributeAccessor>
	{
		using TypedAccessor = typename decltype(SignatureDummy)::Type;
		return MakeUnique<TypedAccessor>(TypedProperty);
	});
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyAccessor(const FName InPropertyName, const UStruct* InStruct)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InPropertyName, InStruct, [](auto SignatureDummy, const auto* TypedProperty) -> TUniquePtr<IPCGAttributeAccessor>
	{
		using TypedAccessor = typename decltype(SignatureDummy)::Type;
		return MakeUnique<TypedAccessor>(TypedProperty);
	});
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyChainAccessor(TArray<const FProperty*>&& InProperties)
{
	if (InProperties.IsEmpty())
	{
		return TUniquePtr<IPCGAttributeAccessor>{};
	}

	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InProperties.Last(), [&InProperties](auto SignatureDummy, const auto* TypedProperty) -> TUniquePtr<IPCGAttributeAccessor>
	{
		using TypedAccessor = typename decltype(SignatureDummy)::Type;
		return MakeUnique<TypedAccessor>(TypedProperty, std::forward<TArray<const FProperty*>>(InProperties));
	});
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyChainAccessor(const TArray<FName>& InPropertyNames, const UStruct* InStruct)
{
	TArray<const FProperty*> PropertyChain;
	if (!PCGAttributeAccessorHelpers::GetPropertyChain(InPropertyNames, InStruct, PropertyChain))
	{
		return TUniquePtr<IPCGAttributeAccessor>{};
	}
	else
	{
		return PCGAttributeAccessorHelpers::CreatePropertyChainAccessor(std::move(PropertyChain));
	}
}

bool PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(const FProperty* InProperty)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InProperty, [](auto, const auto*) -> bool
	{
		return true;
	});
}

bool PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(const FName InPropertyName, const UStruct* InStruct)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InPropertyName, InStruct, [](auto, const auto*) -> bool
	{
		return true;
	});
}

bool PCGAttributeAccessorHelpers::IsPropertyAccessorChainSupported(const TArray<FName>& InPropertyNames, const UStruct* InStruct)
{
	TArray<const FProperty*> PropertyChain;
	return PCGAttributeAccessorHelpers::GetPropertyChain(InPropertyNames, InStruct, PropertyChain) &&
		!PropertyChain.IsEmpty() &&
		PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(PropertyChain.Last());
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateExtraAccessor(EPCGExtraProperties InExtraProperties)
{
	switch (InExtraProperties)
	{
	case EPCGExtraProperties::Index:
		return MakeUnique<FPCGIndexAccessor>();
	default:
		return TUniquePtr<IPCGAttributeAccessor>();
	}
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessorForOverrideParamWithResult(const FPCGDataCollection& InInputData, const FPCGSettingsOverridableParam& InParam, AccessorParamResult* OutResult)
{
	bool bFromGlobalParamsPin = false;
	TArray<FPCGTaggedData> InputParamData = InInputData.GetParamsByPin(InParam.Label);
	if (InputParamData.IsEmpty())
	{
		// If it is empty, try with the Overrides pin (Global Params)
		bFromGlobalParamsPin = true;
		InputParamData = InInputData.GetParamsByPin(PCGPinConstants::DefaultParamsLabel);
	}

	if (OutResult)
	{
		OutResult->bHasMultipleAttributeSetsOnOverridePin = InputParamData.Num() > 1;
	}

	const UPCGParamData* ParamData = !InputParamData.IsEmpty() ? Cast<UPCGParamData>(InputParamData[0].Data) : nullptr;

	if (OutResult && ParamData && !bFromGlobalParamsPin)
	{
		OutResult->bPinConnected = true;
	}

	if (ParamData && ParamData->Metadata && ParamData->Metadata->GetAttributeCount() > 0)
	{
		// If the param only has a single attribute and is not from the global Params pin, use this one. Otherwise we need perfect name matching, either the property name or its full path if there is a name clash.
		const FName AttributeName = (ParamData->Metadata->GetAttributeCount() == 1 && !bFromGlobalParamsPin) 
			? ParamData->Metadata->GetLatestAttributeNameOrNone() 
			: (!InParam.bHasNameClash ? InParam.PropertiesNames.Last() : FName(InParam.GetPropertyPath()));

		if (OutResult)
		{
			OutResult->AttributeName = AttributeName;
		}

		FPCGAttributePropertyInputSelector InputSelector{};
		InputSelector.SetAttributeName(AttributeName);
		TUniquePtr<const IPCGAttributeAccessor> Result = PCGAttributeAccessorHelpers::CreateConstAccessor(ParamData, InputSelector);

		if (!Result)
		{
			// If we didn't find it, try some aliases.
			for (const FName& Alias : InParam.GenerateAllPossibleAliases())
			{
				InputSelector.SetAttributeName(Alias);
				Result = PCGAttributeAccessorHelpers::CreateConstAccessor(ParamData, InputSelector);
				if (Result)
				{
					if (OutResult)
					{
						OutResult->bUsedAliases = true;
						OutResult->AliasUsed = Alias;
					}

					break;
				}
			}
		}

		if (Result && OutResult && ParamData && ParamData->Metadata)
		{
			OutResult->bHasMultipleDataInAttributeSet = ParamData->Metadata->GetLocalItemCount() > 1;
		}

		return Result;
	}

	return TUniquePtr<const IPCGAttributeAccessor>{};
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessor(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector, bool bQuiet)
{
	TUniquePtr<IPCGAttributeAccessor> Accessor;
	
	if (InData)
	{
		// Temporary const_cast for the chain accessor.
		Accessor.Reset(const_cast<IPCGAttributeAccessor*>(FPCGAttributeAccessorFactory::GetInstance().CreateSimpleConstAccessor(InData, InSelector, bQuiet).Release()));
	}
	else
	{
		// For backward compatibility, no data is point data (or extra property)
		if (InSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
		{
			Accessor = CreateExtraAccessor(InSelector.GetExtraProperty());
		}
		else
		{
			Accessor = UPCGPointData::CreateStaticAccessor(InSelector, bQuiet);
		}
	}

	if (!Accessor.IsValid())
	{
		return {};
	}

	return CreateChainAccessor(std::move(Accessor), InSelector, bQuiet);
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessor(const FPCGMetadataAttributeBase* InAttribute, const UPCGMetadata* InMetadata, bool bQuiet)
{
	if (!InMetadata || !InAttribute)
	{
		return {};
	}
	else
	{
		return CreateConstAccessor(InAttribute, InMetadata->GetConstDefaultMetadataDomain(), bQuiet);
	}
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessor(const FPCGMetadataAttributeBase* InAttribute, const FPCGMetadataDomain* InMetadata, bool bQuiet)
{
	if (!InMetadata || !InAttribute)
	{
		return {};
	}
	else
	{
		auto MakeConstAccessor = [InAttribute, InMetadata]<typename T>(T) -> TUniquePtr<const IPCGAttributeAccessor>
		{
			return MakeUnique<const FPCGAttributeAccessor<T>>(static_cast<const FPCGMetadataAttribute<T>*>(InAttribute), InMetadata);
		};
		
		return PCGMetadataAttribute::CallbackWithRightType(InAttribute->GetTypeId(), MakeConstAccessor);
	}
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateAccessor(UPCGData* InData, const FPCGAttributePropertySelector& InSelector, bool bQuiet)
{
	TUniquePtr<IPCGAttributeAccessor> Accessor;
	
	if (InData)
	{
		// Temporary const_cast for the chain accessor.
		Accessor = FPCGAttributeAccessorFactory::GetInstance().CreateSimpleAccessor(InData, InSelector, bQuiet);
	}
	else
	{
		// For backward compatibility, no data is point data (or extra property)
		if (InSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
		{
			Accessor = CreateExtraAccessor(InSelector.GetExtraProperty());
		}
		else
		{
			Accessor = UPCGPointData::CreateStaticAccessor(InSelector, bQuiet);
		}
	}

	if (!Accessor.IsValid())
	{
		return {};
	}

	Accessor = CreateChainAccessor(std::move(Accessor), InSelector, bQuiet);
	
	if (Accessor && Accessor->IsReadOnly())
	{
		if (!bQuiet)
		{
			UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateAccessor] Attribute can not be written into, since it is read-only."));
		}
				
		return {};
	}

	// Set the cached selector
	if (InData)
	{
		InData->SetLastSelector(InSelector);
	}

	return Accessor;
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateAccessor(FPCGMetadataAttributeBase* InAttribute, UPCGMetadata* InMetadata, bool bQuiet)
{
	if (!InMetadata || !InAttribute)
	{
		return {};
	}
	else
	{
		return CreateAccessor(InAttribute, InMetadata->GetDefaultMetadataDomain());
	}
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateAccessor(FPCGMetadataAttributeBase* InAttribute, FPCGMetadataDomain* InMetadata, bool bQuiet)
{
	if (!InMetadata || !InAttribute)
	{
		return {};
	}
	else
	{
		auto MakeAccessor = [InAttribute, InMetadata]<typename T>(T) -> TUniquePtr<IPCGAttributeAccessor>
		{
			return MakeUnique<FPCGAttributeAccessor<T>>(static_cast<FPCGMetadataAttribute<T>*>(InAttribute), InMetadata);
		};
		
		return PCGMetadataAttribute::CallbackWithRightType(InAttribute->GetTypeId(), MakeAccessor);
	}
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateAccessorWithAttributeCreation(UPCGData* InData, const FPCGAttributePropertySelector& InSelector, const IPCGAttributeAccessor* InMatchingAccessor, EPCGAttributeAccessorFlags InTypeMatching, bool bQuiet)
{
	TUniquePtr<IPCGAttributeAccessor> Result = CreateAccessor(InData, InSelector, bQuiet);

	if (!InSelector.IsBasicAttribute() || !InMatchingAccessor)
	{
		return Result;
	}

	bool bValid = !!Result;

	if (bValid && !!(InTypeMatching & EPCGAttributeAccessorFlags::StrictType))
	{
		bValid &= (InMatchingAccessor->GetUnderlyingType() == Result->GetUnderlyingType());
	}

	if (bValid && !!(InTypeMatching & EPCGAttributeAccessorFlags::AllowBroadcast))
	{
		bValid &= PCG::Private::IsBroadcastable(InMatchingAccessor->GetUnderlyingType(), Result->GetUnderlyingType());
	}

	if (bValid && !!(InTypeMatching & EPCGAttributeAccessorFlags::AllowConstructible))
	{
		bValid &= PCG::Private::IsConstructible(InMatchingAccessor->GetUnderlyingType(), Result->GetUnderlyingType());
	}

	if (!bValid)
	{
		Result.Reset();

		// We didn't find the attribute in the data, or we can't Broadcast/Construct, so create a new one
		UPCGMetadata* Metadata = InData->MutableMetadata();
		if (!Metadata)
		{
			return Result;
		}

		const FName AttributeName = InSelector.GetName();
		if (Metadata->HasAttribute(AttributeName))
		{
			Metadata->DeleteAttribute(AttributeName);
		}

		auto CreateAttributeAndAccessor = [InMatchingAccessor, AttributeName, Metadata](auto&& Dummy) -> TUniquePtr<IPCGAttributeAccessor>
		{
			using AttributeType = std::decay_t<decltype(Dummy)>;
			AttributeType DefaultValue = PCG::Private::MetadataTraits<AttributeType>::ZeroValue();
			InMatchingAccessor->Get<AttributeType>(DefaultValue, FPCGAttributeAccessorKeysEntries(PCGInvalidEntryKey));
			FPCGMetadataAttribute<AttributeType>* Attribute = Metadata->CreateAttribute<AttributeType>(AttributeName, DefaultValue, /*bAllowInterpolation=*/true, /*bOverrideParent=*/false);

			return Attribute ? MakeUnique<FPCGAttributeAccessor<AttributeType>>(Attribute, Attribute->GetMetadataDomain()) : nullptr;
		};

		Result = PCGMetadataAttribute::CallbackWithRightType(InMatchingAccessor->GetUnderlyingType(), std::move(CreateAttributeAndAccessor));
	}

	return Result;
}

TUniquePtr<const IPCGAttributeAccessorKeys> PCGAttributeAccessorHelpers::CreateConstKeys(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector)
{
	return FPCGAttributeAccessorFactory::GetInstance().CreateSimpleConstKeys(InData, InSelector, /*bQuiet=*/ false);
}

TUniquePtr<IPCGAttributeAccessorKeys> PCGAttributeAccessorHelpers::CreateKeys(UPCGData* InData, const FPCGAttributePropertySelector& InSelector)
{
	return FPCGAttributeAccessorFactory::GetInstance().CreateSimpleKeys(InData, InSelector, /*bQuiet=*/ false);
}
