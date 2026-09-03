// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanMaterialPipelineCommon.h"

#include "StructUtils/PropertyBag.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Algo/Find.h"

namespace UE::MetaHuman::MaterialUtils
{
	void SetInstanceParameters(
		const TArray<FMetaHumanMaterialParameter>& InMaterialParameters,
		const TMap<FName, TObjectPtr<class UMaterialInstanceDynamic>>& InMaterialInstanceMapping,
		const TArray<FName>& InAvailableSlots,
		const FInstancedPropertyBag& InPropertyBag)
	{
		const UPropertyBag* PropertyBag = InPropertyBag.GetPropertyBagStruct();
		if (!PropertyBag)
		{
			return;
		}

		for (const FPropertyBagPropertyDesc& PropertyDesc : PropertyBag->GetPropertyDescs())
		{
			const FMetaHumanMaterialParameter* Parameter = Algo::FindBy(InMaterialParameters, PropertyDesc.Name, &FMetaHumanMaterialParameter::InstanceParameterName);
			if (!Parameter)
			{
				// TODO: log error as this suggests InMaterialParameters have changed since assembly
				continue;
			}

			// Slot names are collected either from slot indices or explicit slot names defined on the parameter
			TArray<FName> SlotNames;

			switch (Parameter->SlotTarget)
			{
				case EMetaHumanRuntimeMaterialParameterSlotTarget::SlotNames:
				{
					SlotNames = Parameter->SlotNames;
					break;
				}
				case EMetaHumanRuntimeMaterialParameterSlotTarget::SlotIndices:
				{
					for (int32 SlotIndex : Parameter->SlotIndices)
					{
						if (InAvailableSlots.IsValidIndex(SlotIndex))
						{
							SlotNames.Add(InAvailableSlots[SlotIndex]);
						}
					}
					break;
				}
				default:
					break;
			}

			for (const FName SlotName : SlotNames)
			{
				UMaterialInstanceDynamic* MaterialInstance = nullptr;

				if (const TObjectPtr<UMaterialInstanceDynamic>* FoundMaterialInstance = InMaterialInstanceMapping.Find(SlotName))
				{
					MaterialInstance = *FoundMaterialInstance;
				}

				if (!MaterialInstance)
				{
					// TODO: log error. This shouldn't happen
					continue;
				}

				switch (Parameter->ParameterType)
				{
					case EMetaHumanRuntimeMaterialParameterType::Toggle:
					{
						const TValueOrError<bool, EPropertyBagResult> Result = InPropertyBag.GetValueBool(PropertyDesc);
						if (!Result.HasValue())
						{
							break;
						}

						MaterialInstance->SetScalarParameterValueByInfo(Parameter->MaterialParameter, Result.GetValue() ? 1.0f : 0.0f);
					}
					break;

					case EMetaHumanRuntimeMaterialParameterType::Scalar:
					{
						const TValueOrError<float, EPropertyBagResult> Result = InPropertyBag.GetValueFloat(PropertyDesc);
						if (!Result.HasValue())
						{
							break;
						}

						MaterialInstance->SetScalarParameterValueByInfo(Parameter->MaterialParameter, Result.GetValue());
					}
					break;

					case EMetaHumanRuntimeMaterialParameterType::Vector:
					{
						const TValueOrError<FLinearColor*, EPropertyBagResult> Result = InPropertyBag.GetValueStruct<FLinearColor>(PropertyDesc);
						if (!Result.HasValue()
							&& Result.GetValue() != nullptr)
						{
							break;
						}

						MaterialInstance->SetVectorParameterValueByInfo(Parameter->MaterialParameter, *Result.GetValue());
					}
					break;

					case EMetaHumanRuntimeMaterialParameterType::DoubleVector:

					case EMetaHumanRuntimeMaterialParameterType::Texture:
					{
						const TValueOrError<UObject*, EPropertyBagResult> Result = InPropertyBag.GetValueObject(PropertyDesc, UTexture::StaticClass());
						if (!Result.HasValue())
						{
							break;
						}

						MaterialInstance->SetTextureParameterValueByInfo(Parameter->MaterialParameter, Cast<UTexture>(Result.GetValue()));
						break;
					}

					case EMetaHumanRuntimeMaterialParameterType::TextureCollection:

					case EMetaHumanRuntimeMaterialParameterType::Font:

					case EMetaHumanRuntimeMaterialParameterType::RuntimeVirtualTexture:

					case EMetaHumanRuntimeMaterialParameterType::SparseVolumeTexture:
						break;
				}
			}
		}
	}

	bool ParametersToPropertyBag(
		TNotNull<const UMaterialInstanceDynamic*> InMaterial,
		const TArray<FMetaHumanMaterialParameter>& InMaterialParameters,
		FInstancedPropertyBag& InOutPropertyBag)
	{
		int32 Count = 0;

		for (const FMetaHumanMaterialParameter& MaterialParameter : InMaterialParameters)
		{
			FPropertyBagPropertyDesc PropertyDesc;
			PropertyDesc.Name = MaterialParameter.InstanceParameterName;

	#if WITH_EDITORONLY_DATA
			for (const TPair<FName, FString>& ItPair : MaterialParameter.PropertyMetadata)
			{
				PropertyDesc.MetaData.Add(FPropertyBagPropertyDescMetaData(ItPair.Key, ItPair.Value));
			}
	#endif
			// Get current parameter value and set property value
			switch (MaterialParameter.ParameterType)
			{
				case EMetaHumanRuntimeMaterialParameterType::Toggle:
				{
					PropertyDesc.ValueType = EPropertyBagPropertyType::Bool;
					FMaterialParameterMetadata MaterialValue;

					if (InMaterial->GetParameterValue(EMaterialParameterType::Scalar, MaterialParameter.MaterialParameter, MaterialValue))
					{
						float Value = MaterialValue.Value.AsScalar();
						verify(MaterialValue.Value.Type == EMaterialParameterType::Scalar);
						InOutPropertyBag.AddProperties({ PropertyDesc }); // TODO: Better way to add desc?
						InOutPropertyBag.SetValueBool(MaterialParameter.InstanceParameterName, Value > 0.0f);
					}
				}
				break;

				case EMetaHumanRuntimeMaterialParameterType::Scalar:
				{
					PropertyDesc.ValueType = EPropertyBagPropertyType::Float;
					FMaterialParameterMetadata MaterialValue;

					if (InMaterial->GetParameterValue(EMaterialParameterType::Scalar, MaterialParameter.MaterialParameter, MaterialValue))
					{
						float Value = MaterialValue.Value.AsScalar();
						verify(MaterialValue.Value.Type == EMaterialParameterType::Scalar);
						InOutPropertyBag.AddProperties({ PropertyDesc });
						InOutPropertyBag.SetValueFloat(MaterialParameter.InstanceParameterName, Value);
					}
				}
				break;

				case EMetaHumanRuntimeMaterialParameterType::Vector:
				{
					PropertyDesc.ValueType = EPropertyBagPropertyType::Struct;
					PropertyDesc.ValueTypeObject = TBaseStructure<FLinearColor>::Get();
					FMaterialParameterMetadata MaterialValue;

					if (InMaterial->GetParameterValue(EMaterialParameterType::Vector, MaterialParameter.MaterialParameter, MaterialValue))
					{
						FLinearColor Value = MaterialValue.Value.AsLinearColor();
						verify(MaterialValue.Value.Type == EMaterialParameterType::Vector);
						InOutPropertyBag.AddProperties({ PropertyDesc });
						InOutPropertyBag.SetValueStruct(MaterialParameter.InstanceParameterName, Value);
					}
				}
				break;

				case EMetaHumanRuntimeMaterialParameterType::DoubleVector:

				case EMetaHumanRuntimeMaterialParameterType::Texture:
				{
					PropertyDesc.ValueType = EPropertyBagPropertyType::Object;
					PropertyDesc.ValueTypeObject = UTexture::StaticClass();
					FMaterialParameterMetadata MaterialValue;

					if (InMaterial->GetParameterValue(EMaterialParameterType::Texture, MaterialParameter.MaterialParameter, MaterialValue))
					{
						UTexture* Value = Cast<UTexture>(MaterialValue.Value.AsTextureObject());
						verify(MaterialValue.Value.Type == EMaterialParameterType::Texture);
						InOutPropertyBag.AddProperties({ PropertyDesc });
						InOutPropertyBag.SetValueObject(MaterialParameter.InstanceParameterName, Value);
					}
				}
				break;

				case EMetaHumanRuntimeMaterialParameterType::TextureCollection:

				case EMetaHumanRuntimeMaterialParameterType::Font:

				case EMetaHumanRuntimeMaterialParameterType::RuntimeVirtualTexture:

				case EMetaHumanRuntimeMaterialParameterType::SparseVolumeTexture:
					break;

			}

			if (InOutPropertyBag.FindPropertyDescByName(MaterialParameter.InstanceParameterName))
			{
				Count++;
			}
		}

		return Count > 0;
	}

	EMetaHumanRuntimeMaterialParameterType PropertyToParameterType(TNotNull<FProperty*> InProperty)
	{
		if (InProperty->IsA(FBoolProperty::StaticClass()))
		{
			return EMetaHumanRuntimeMaterialParameterType::Toggle;
		}
		else if (InProperty->IsA(FFloatProperty::StaticClass()))
		{
			return EMetaHumanRuntimeMaterialParameterType::Scalar;
		}
		else if (InProperty->IsA(FStructProperty::StaticClass()))
		{
			if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
			{
				if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get()
					|| StructProperty->Struct == TBaseStructure<FColor>::Get())
				{
					return EMetaHumanRuntimeMaterialParameterType::Vector;
				}
			}
		}
		else if (InProperty->IsA(FSoftObjectProperty::StaticClass()))
		{
			if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(InProperty))
			{
				if (SoftObjectProperty->PropertyClass->IsChildOf<UTexture>())
				{
					return EMetaHumanRuntimeMaterialParameterType::Texture;
				}
			}
		}
		else if (InProperty->IsA(FObjectProperty::StaticClass()))
		{
			if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
			{
				if (ObjectProperty->PropertyClass->IsChildOf<UTexture>())
				{
					return EMetaHumanRuntimeMaterialParameterType::Texture;
				}
			}
		}

		// TODO: Unsupported type
		checkNoEntry();
		return EMetaHumanRuntimeMaterialParameterType::Scalar;
	}

#if WITH_EDITOR
	TMap<FName, FString> CopyMetadataFromProperty(TNotNull<FProperty*> InProperty)
	{
		TMap<FName, FString> Result;

		if (const TMap<FName, FString>* FoundMetaDataMap = InProperty->GetMetaDataMap())
		{
			Result = *FoundMetaDataMap;
			Result.Remove(FName("ModuleRelativePath"));
		}

		return Result;
	}
#endif

} // namespace UE::MetaHuman::MaterialUtils