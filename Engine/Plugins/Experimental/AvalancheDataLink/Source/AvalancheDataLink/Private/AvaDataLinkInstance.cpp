// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaDataLinkInstance.h"
#include "AvaDataLinkLog.h"
#include "AvaSceneSubsystem.h"
#include "Avalanche/Public/IAvaSceneInterface.h"
#include "Controller/RCController.h"
#include "DataLinkEnums.h"
#include "DataLinkExecutor.h"
#include "DataLinkExecutorArguments.h"
#include "DataLinkJsonUtils.h"
#include "DataLinkUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "JsonObjectWrapper.h"
#include "PropertyBindingTypes.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlPreset.h"
#include "StructUtils/StructView.h"
#include "UObject/PropertyAccessUtil.h"

namespace UE::AvaDataLink::Private
{
	struct FCopyInfo
	{
		const FProperty* SourceProperty;
		const uint8* SourceMemory;
		FProperty* TargetProperty;
		uint8* TargetMemory;
	};

	/**
	 * Tries promoting a given source property value to match the target property type and copies it to the target memory
	 * Derived from FPropertyBindingBindingCollection::PerformCopy
	 */
	template<typename InSourcePropertyType>
	class FPromotionCopy
	{
		using FSourceCppType = typename InSourcePropertyType::TCppType;

		template<typename InTargetPropertyType>
		bool CopySingle(const FCopyInfo& InCopyInfo, const InSourcePropertyType& InSourceProperty)
		{
			using FTargetCppType = typename InTargetPropertyType::TCppType;

			static_assert(std::is_convertible_v<FSourceCppType, FTargetCppType>);

			if (InTargetPropertyType* TargetProperty = CastField<InTargetPropertyType>(InCopyInfo.TargetProperty))
			{
				TargetProperty->SetPropertyValue(InCopyInfo.TargetMemory, static_cast<FTargetCppType>(InSourceProperty.GetPropertyValue(InCopyInfo.SourceMemory)));
				return true;
			}
			return false;
		}

		template<typename>
		bool CopyInternal(const FCopyInfo& InCopyInfo, const InSourcePropertyType& InSourceProperty)
		{
			return false;
		}

		template<typename, typename InTargetPropertyType, typename ...InOtherTargetPropertyTypes>
		bool CopyInternal(const FCopyInfo& InCopyInfo, const InSourcePropertyType& InSourceProperty)
		{
			return this->CopySingle<InTargetPropertyType>(InCopyInfo, InSourceProperty) || this->CopyInternal<void, InOtherTargetPropertyTypes...>(InCopyInfo, InSourceProperty);
		}

	public:
		/** Tries promoting from a source property type to any of the given target property types */
		template<typename ...InTargetPropertyTypes>
		bool Copy(const FCopyInfo& InCopyInfo)
		{
			if (const InSourcePropertyType* SourceProperty = CastField<InSourcePropertyType>(InCopyInfo.SourceProperty))
			{
				return this->CopyInternal<void, InTargetPropertyTypes...>(InCopyInfo, *SourceProperty);
			}
			return false;
		}
	};

	bool PromoteCopy(const FCopyInfo& InCopyInfo)
	{
		// Bool Promotions
		return FPromotionCopy<FBoolProperty>().Copy<FByteProperty, FIntProperty, FUInt32Property, FInt64Property, FFloatProperty, FDoubleProperty>(InCopyInfo)
			// Byte Promotion
			|| FPromotionCopy<FByteProperty>().Copy<FIntProperty, FUInt32Property, FInt64Property, FFloatProperty, FDoubleProperty>(InCopyInfo)
			// Int32 Promotions
			|| FPromotionCopy<FIntProperty>().Copy<FInt64Property, FFloatProperty, FDoubleProperty>(InCopyInfo)
			// Uint32 Promotions
			|| FPromotionCopy<FUInt32Property>().Copy<FInt64Property, FFloatProperty, FDoubleProperty>(InCopyInfo)
			// Float Promotions
			|| FPromotionCopy<FFloatProperty>().Copy<FIntProperty, FInt64Property, FDoubleProperty>(InCopyInfo)
			// Double Promotions
			|| FPromotionCopy<FDoubleProperty>().Copy<FIntProperty, FInt64Property, FFloatProperty>(InCopyInfo)
		;
	}
} // UE::AvaDataLink::Private

void UAvaDataLinkInstance::Execute()
{
	if (Executor.IsValid())
	{
		UE_LOG(LogAvaDataLink, Error, TEXT("[%s] Data Link execution is in progress!"), Executor->GetContextName().GetData());
		return;
	}

	Executor = FDataLinkExecutor::Create(FDataLinkExecutorArguments(DataLinkInstance)
#if WITH_DATALINK_CONTEXT
		.SetContextName(BuildContextName())
#endif
		.SetContextObject(this)
		.SetOnFinish(FOnDataLinkExecutionFinished::CreateUObject(this, &UAvaDataLinkInstance::OnExecutionFinished)));

	Executor->Run();
}

#if WITH_DATALINK_CONTEXT
FString UAvaDataLinkInstance::BuildContextName() const
{
	return FString::Printf(TEXT("Motion Design Data Link. World: '%s'"), *GetNameSafe(GetTypedOuter<UWorld>()));
}
#endif

void UAvaDataLinkInstance::OnExecutionFinished(const FDataLinkExecutor& InExecutor, EDataLinkExecutionResult InResult, FConstStructView InOutputDataView)
{
	Executor.Reset();

	if (InResult == EDataLinkExecutionResult::Failed || !InOutputDataView.IsValid())
	{
		return;
	}

	URemoteControlPreset* const Preset = GetRemoteControlPreset();
	if (!Preset)
	{
		UE_LOG(LogAvaDataLink, Error, TEXT("[%s] Data Link execution finished, but Remote Control is invalid!"), InExecutor.GetContextName().GetData());
		return;
	}

	URCVirtualPropertyContainerBase* const ControllerContainer = Preset->GetControllerContainer();
	if (!ControllerContainer)
	{
		UE_LOG(LogAvaDataLink, Error, TEXT("[%s] Data Link execution finished, but Remote Control '%s' has invalid Controller Container!")
			, InExecutor.GetContextName().GetData()
			, *Preset->GetName());
		return;
	}

	const FStructView TargetDataView = ControllerContainer->GetPropertyBagMutableValue();
	if (!TargetDataView.IsValid())
	{
		UE_LOG(LogAvaDataLink, Error, TEXT("[%s] Data Link execution finished, but Remote Control '%s' has invalid Controller Container Data View!")
			, InExecutor.GetContextName().GetData()
			, *Preset->GetName());
		return;
	}

	TArray<URCController*> ModifiedControllers;
	ModifiedControllers.Reserve(ControllerMappings.Num());

	if (const FJsonObjectWrapper* OutputJson = InOutputDataView.GetPtr<const FJsonObjectWrapper>())
	{
		if (!OutputJson->JsonObject.IsValid())
		{
			UE_LOG(LogAvaDataLink, Warning, TEXT("[%s] Data Link output could not be applied to controllers. Json Object was not valid!"), InExecutor.GetContextName().GetData());
			return;
		}

		TSharedRef<FJsonObject> JsonObject = OutputJson->JsonObject.ToSharedRef();

		ForEachResolvedController(InExecutor, Preset, TargetDataView,
			[&JsonObject, &ModifiedControllers](const FResolvedController& InResolvedController)
			{
				TSharedPtr<FJsonValue> SourceJsonValue = UE::DataLinkJson::FindJsonValue(JsonObject, InResolvedController.Mapping->OutputFieldName);

				// If property was set, return true to signal that the controller value was modified
				if (FJsonObjectConverter::JsonValueToUProperty(SourceJsonValue, InResolvedController.TargetProperty, InResolvedController.TargetMemory))
				{
					ModifiedControllers.AddUnique(InResolvedController.Controller);
				}
			});
	}
	else
	{
		ForEachResolvedController(InExecutor, Preset, TargetDataView,
			[&InOutputDataView, &ModifiedControllers, &InExecutor](const FResolvedController& InResolvedController)
			{
				const FProperty* SourceProperty = PropertyAccessUtil::FindPropertyByName(*InResolvedController.Mapping->OutputFieldName, InOutputDataView.GetScriptStruct());
				if (!SourceProperty)
				{
					UE_LOG(LogAvaDataLink, Warning, TEXT("[%s] Data Link output field name '%s' could not be applied as it was not found in output data struct '%s'")
						, InExecutor.GetContextName().GetData()
						, *InResolvedController.Mapping->OutputFieldName
						, *GetNameSafe(InOutputDataView.GetScriptStruct()));
					return;
				}

				const UE::PropertyBinding::EPropertyCompatibility Compatibility = UE::PropertyBinding::GetPropertyCompatibility(SourceProperty, InResolvedController.TargetProperty);
				if (Compatibility == UE::PropertyBinding::EPropertyCompatibility::Incompatible)
				{
					UE_LOG(LogAvaDataLink, Warning, TEXT("[%s] Data Link output '%s' could not be applied to controller '%s' as types are incompatible")
						, InExecutor.GetContextName().GetData()
						, *InResolvedController.Mapping->OutputFieldName
						, *InResolvedController.Mapping->TargetController.Name.ToString());
					return;
				}

				const uint8* SourceMemory = SourceProperty->ContainerPtrToValuePtr<uint8>(InOutputDataView.GetMemory());
				switch (Compatibility)
				{
				case UE::PropertyBinding::EPropertyCompatibility::Compatible:
					InResolvedController.TargetProperty->CopyCompleteValue(InResolvedController.TargetMemory, SourceMemory);
					ModifiedControllers.AddUnique(InResolvedController.Controller);
					break;

				case UE::PropertyBinding::EPropertyCompatibility::Promotable:
					ensureMsgf(UE::AvaDataLink::Private::PromoteCopy({
						.SourceProperty = SourceProperty,
						.SourceMemory = SourceMemory,
						.TargetProperty = InResolvedController.TargetProperty,
						.TargetMemory = InResolvedController.TargetMemory
					}), TEXT("Promotion failed even though compatibility was deemed as 'promotable'."));
					ModifiedControllers.AddUnique(InResolvedController.Controller);
					break;
				}
			});
	}

	TSet<FGuid> ModifiedControllerIds;
	ModifiedControllerIds.Reserve(ModifiedControllers.Num());

	for (URCController* Controller : ModifiedControllers)
	{
		Controller->OnModifyPropertyValue();
		ModifiedControllerIds.Add(Controller->Id);
	}

	Preset->OnControllerModified().Broadcast(Preset, ModifiedControllerIds);
}

IAvaSceneInterface* UAvaDataLinkInstance::GetSceneInterface() const
{
	ULevel* Level = GetTypedOuter<ULevel>();
	if (!Level || !Level->OwningWorld)
	{
		return nullptr;
	}

	const UAvaSceneSubsystem* SceneSubsystem = Level->OwningWorld->GetSubsystem<UAvaSceneSubsystem>();
	if (!SceneSubsystem)
	{
		// No subsystem yet available, try finding the scene interface by iterating the actors
		return UAvaSceneSubsystem::FindSceneInterface(Level);
	}

	return SceneSubsystem->GetSceneInterface(Level);
}

URemoteControlPreset* UAvaDataLinkInstance::GetRemoteControlPreset() const
{
	if (IAvaSceneInterface* SceneInterface = GetSceneInterface())
	{
		return SceneInterface->GetRemoteControlPreset();
	}
	return nullptr;
}

void UAvaDataLinkInstance::ForEachResolvedController(const FDataLinkExecutor& InExecutor, URemoteControlPreset* InPreset, FStructView InTargetDataView, TFunctionRef<void(const FResolvedController&)> InFunction)
{
	for (const FAvaDataLinkControllerMapping& Mapping : ControllerMappings)
	{
		FResolvedController ResolvedMapping;
		ResolvedMapping.Mapping = &Mapping;

		// Get the controller to retrieve the underlying Property FName of the controller within the Controller Container Property Bag
		ResolvedMapping.Controller = Cast<URCController>(Mapping.TargetController.FindController(InPreset));
		if (!ResolvedMapping.Controller)
		{
			UE_LOG(LogAvaDataLink, Warning, TEXT("[%s] Data Link output '%s' could not be applied to controller '%s'. Controller was not found in preset '%s'!")
				, InExecutor.GetContextName().GetData()
				, *Mapping.OutputFieldName
				, *Mapping.TargetController.Name.ToString()
				, *InPreset->GetName());
			continue;
		}

		ResolvedMapping.TargetProperty = PropertyAccessUtil::FindPropertyByName(ResolvedMapping.Controller->PropertyName, InTargetDataView.GetScriptStruct());
		if (!ResolvedMapping.TargetProperty)
		{
			UE_LOG(LogAvaDataLink, Warning, TEXT("[%s] Data Link output '%s' could not be applied to controller '%s'. Controller property '%s' in preset '%s' was not found!")
				, InExecutor.GetContextName().GetData()
				, *Mapping.OutputFieldName
				, *Mapping.TargetController.Name.ToString()
				, *ResolvedMapping.Controller->PropertyName.ToString()
				, *InPreset->GetName());
			continue;
		}

		ResolvedMapping.TargetMemory = ResolvedMapping.TargetProperty->ContainerPtrToValuePtr<uint8>(InTargetDataView.GetMemory());
		InFunction(ResolvedMapping);
	}
}
