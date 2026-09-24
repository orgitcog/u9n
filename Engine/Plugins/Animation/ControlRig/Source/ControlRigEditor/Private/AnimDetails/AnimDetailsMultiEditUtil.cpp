// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsMultiEditUtil.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/RemoveIf.h"
#include "Algo/Transform.h"
#include "AnimDetails/AnimDetailsProxyManager.h"
#include "AnimDetails/AnimDetailsSelection.h"
#include "AnimDetails/Proxies/AnimDetailsProxyBase.h"
#include "AnimDetailsMathOperation.h"
#include "Editor.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AnimDetailsMultiEditUtil"

namespace UE::ControlRigEditor
{
	namespace PropertyUtils
	{
		/** Adjusts the property values of a property by delta */
		template <typename ValueType>
		static void Adjust(const TSharedRef<IPropertyHandle>& PropertyHandle, const ValueType Delta, const bool bInteractive)
		{
			TArray<FString> PerObjectValues;
			if (PropertyHandle->GetPerObjectValues(PerObjectValues) == FPropertyAccess::Success)
			{
				for (int32 ValueIndex = 0; ValueIndex < PerObjectValues.Num(); ++ValueIndex)
				{
					ValueType OldValue;
					TTypeFromString<ValueType>::FromString(OldValue, *PerObjectValues[ValueIndex]);

					const ValueType NewValue = OldValue + Delta;
					PerObjectValues[ValueIndex] = TTypeToString<ValueType>::ToSanitizedString(NewValue);
				}

				PropertyHandle->NotifyPreChange();

				// Mind this still will modify the outer objects, work around exists in UAnimDetailsProxyBase::Modify
				const EPropertyValueSetFlags::Type ValueSetFlags = bInteractive ? 
					EPropertyValueSetFlags::InteractiveChange | EPropertyValueSetFlags::NotTransactable : 
					EPropertyValueSetFlags::DefaultFlags;

				PropertyHandle->SetPerObjectValues(PerObjectValues, ValueSetFlags);

				const EPropertyChangeType::Type ChangeType = bInteractive ? 
					EPropertyChangeType::Interactive : 
					EPropertyChangeType::ValueSet;

				 PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			}
		}

		/** Applies a math operation to the property */
		template <typename ValueType>
		static void ApplyMathOperation(const TSharedRef<IPropertyHandle>& PropertyHandle, FAnimDetailsMathOperation<ValueType> MathOperation)
		{
			TArray<FString> PerObjectValues;
			if (PropertyHandle->GetPerObjectValues(PerObjectValues) == FPropertyAccess::Success)
			{
				for (int32 ValueIndex = 0; ValueIndex < PerObjectValues.Num(); ++ValueIndex)
				{
					ValueType LHSValue;
					TTypeFromString<ValueType>::FromString(LHSValue, *PerObjectValues[ValueIndex]);

					switch (MathOperation.MathOperator)
					{
					case EAnimDetailsMathOperator::Add:
						LHSValue += MathOperation.RHSValue;
						break;

					case EAnimDetailsMathOperator::Substract:
						LHSValue -= MathOperation.RHSValue;
						break;

					case EAnimDetailsMathOperator::Multiply:
						LHSValue *= MathOperation.RHSValue;
						break;

					case EAnimDetailsMathOperator::Divide:

						// Avoid divisions by zero
						if constexpr (std::is_floating_point_v<ValueType>)
						{
							if (FMath::IsNearlyEqual(MathOperation.RHSValue, 0.f))
							{
								return;
							}
						}
						else if (MathOperation.RHSValue == 0)
						{
							return;
						}

						LHSValue /= MathOperation.RHSValue;
						break;

					default:
						checkf(0, TEXT("Unhandled enum value"));
					}

					PerObjectValues[ValueIndex] = TTypeToString<ValueType>::ToSanitizedString(LHSValue);
				}

				PropertyHandle->NotifyPreChange();
				PropertyHandle->SetPerObjectValues(PerObjectValues);
				PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			}
		}
	}

	TUniquePtr<FAnimDetailsMultiEditUtil> FAnimDetailsMultiEditUtil::Instance;

	FAnimDetailsMultiEditUtil& FAnimDetailsMultiEditUtil::Get()
	{
		if (!Instance.IsValid())
		{
			Instance = MakeUnique<FAnimDetailsMultiEditUtil>();
		}

		return *Instance;
	}

	void FAnimDetailsMultiEditUtil::Join(UAnimDetailsProxyManager* ProxyManager, const TSharedRef<IPropertyHandle>& PropertyHandle)
	{
		if (!ProxyManager)
		{
			return;
		}

		TArray<TWeakPtr<IPropertyHandle>>& Properties = ProxyManagerToPropertiesMap.FindOrAdd(ProxyManager);

		const bool bAlreadyJoined = Algo::AnyOf(Properties,
			[&PropertyHandle](const TWeakPtr<IPropertyHandle>& WeakPropertyHandle)
			{
				return PropertyHandle == WeakPropertyHandle;
			});

		if (!bAlreadyJoined)
		{
			Properties.Add(PropertyHandle);
		}
	}

	void FAnimDetailsMultiEditUtil::Leave(const TWeakPtr<IPropertyHandle>& WeakPropertyHandle)
	{
		for (auto It = ProxyManagerToPropertiesMap.CreateIterator(); It; ++It)
		{
			// Remove invalid proxy managers
			if (!(*It).Key.IsValid())
			{
				It.RemoveCurrent();
				continue;
			}

			TArray<TWeakPtr<IPropertyHandle>>& Properties = (*It).Value;

			// Remove Invalid properties
			Properties.SetNum(
				Algo::RemoveIf(Properties,
				[&WeakPropertyHandle](const TWeakPtr<IPropertyHandle>& OtherWeakProperty)
				{
					return
						!OtherWeakProperty.IsValid() ||
						!OtherWeakProperty.Pin()->IsValidHandle() ||
						OtherWeakProperty == WeakPropertyHandle;
				})
			);

			// If there are no properties for this proxy manager, remove the proxy manager
			if (ProxyManagerToPropertiesMap.IsEmpty())
			{
				It.RemoveCurrent();
				continue;
			}
		}

		// Destroy self if there are no proxy managers left
		if (ProxyManagerToPropertiesMap.IsEmpty())
		{
			Instance.Reset();
		}
	}

	template <typename ValueType>
	void FAnimDetailsMultiEditUtil::MultiEditSet(
		const UAnimDetailsProxyManager& ProxyManager, 
		const ValueType Value, 
		const TSharedRef<IPropertyHandle>& InstigatorProperty)
	{
		// Don't set the same value again. This avoids issues where clearing focus on a property value widget
		// would multi set its value to other selected properties with possibly different values.
		ValueType CurrentValue;
		if (InstigatorProperty->GetValue(CurrentValue) == FPropertyAccess::Success &&
			Value == CurrentValue)
		{
			return;
		}

		const FScopedTransaction ScopedTransaction(LOCTEXT("MultiEditSetTransaction", "Set Property Value"));

		for (const TSharedRef<IPropertyHandle>& PropertyHandle : GetPropertiesBeingEdited<ValueType>(ProxyManager, InstigatorProperty))
		{
			PropertyHandle->NotifyPreChange();
			PropertyHandle->SetValue(Value);
			PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
	}

	template void FAnimDetailsMultiEditUtil::MultiEditSet(const UAnimDetailsProxyManager& ProxyManager, const double Value, const TSharedRef<IPropertyHandle>& InstigatorProperty);
	template void FAnimDetailsMultiEditUtil::MultiEditSet(const UAnimDetailsProxyManager& ProxyManager, const int64 Value, const TSharedRef<IPropertyHandle>& InstigatorProperty);
	template void FAnimDetailsMultiEditUtil::MultiEditSet(const UAnimDetailsProxyManager& ProxyManager, const bool Value, const TSharedRef<IPropertyHandle>& InstigatorProperty);

	template <typename ValueType>
	void FAnimDetailsMultiEditUtil::MultiEditMath(
		const UAnimDetailsProxyManager& ProxyManager, 
		const FAnimDetailsMathOperation<ValueType>& MathOperation, 
		const TSharedRef<IPropertyHandle>& InstigatorProperty)
	{
		if (!ensureMsgf(MathOperation.MathOperator != EAnimDetailsMathOperator::None, TEXT("Unexpected trying to perform a math operation but no operator is defined")))
		{
			return;
		}

		const FScopedTransaction ScopedTransaction(LOCTEXT("MultiEditMathTransaction", "Set Property Value"));

		for (const TSharedRef<IPropertyHandle>& PropertyHandle : GetPropertiesBeingEdited<ValueType>(ProxyManager, InstigatorProperty))
		{
			const FFieldClass* PropertyClass = PropertyHandle->GetPropertyClass();

			if constexpr (std::is_floating_point_v<ValueType>)
			{
				if (PropertyClass == FDoubleProperty::StaticClass())
				{
					PropertyUtils::ApplyMathOperation<double>(PropertyHandle, MathOperation);
				}
			}
			else if constexpr (std::is_integral_v<ValueType>)
			{
				if (PropertyClass == FInt64Property::StaticClass())
				{
					PropertyUtils::ApplyMathOperation<int64>(PropertyHandle, MathOperation);
				}
			}
			else
			{
				[] <bool SupportedType = false>()
				{
					static_assert(SupportedType, "Unsupported type in FAnimDetailsMultiEditUtil.");
				}();
			}
		}
	}

	template void FAnimDetailsMultiEditUtil::MultiEditMath(const UAnimDetailsProxyManager& ProxyManager, const FAnimDetailsMathOperation<double>& MathOperation, const TSharedRef<IPropertyHandle>& InstigatorProperty);
	template void FAnimDetailsMultiEditUtil::MultiEditMath(const UAnimDetailsProxyManager& ProxyManager, const FAnimDetailsMathOperation<int64>& MathOperation, const TSharedRef<IPropertyHandle>& InstigatorProperty);

	template <typename ValueType>
	void FAnimDetailsMultiEditUtil::MultiEditChange(
		const UAnimDetailsProxyManager& ProxyManager, 
		const ValueType DesiredDelta, 
		const TSharedRef<IPropertyHandle>& InstigatorProperty, 
		const bool bInteractive)
	{
		if (bInteractive && !IsInteractive())
		{
			GEditor->BeginTransaction(LOCTEXT("MultiEditSetPropertyValues", "Set Property Value"));
		}

		PropertiesBeingEditedInteractively = GetPropertiesBeingEdited<ValueType>(ProxyManager, InstigatorProperty);

		const ValueType Delta = DesiredDelta;
		for (const TSharedRef<IPropertyHandle>& PropertyHandle : PropertiesBeingEditedInteractively)
		{
			const FFieldClass* PropertyClass = PropertyHandle->GetPropertyClass();

			if constexpr (std::is_floating_point_v<ValueType>)
			{
				if (PropertyClass == FDoubleProperty::StaticClass())
				{
					PropertyUtils::Adjust<double>(PropertyHandle, Delta, bInteractive);
				}
			}
			else if constexpr (std::is_integral_v<ValueType>)
			{
				if (PropertyClass == FInt64Property::StaticClass())
				{
					PropertyUtils::Adjust<int64>(PropertyHandle, Delta, bInteractive);
				}
			}
			else
			{
				[] <bool SupportedType = false>()
				{
					static_assert(SupportedType, "Unsupported type in FAnimDetailsMultiEditUtil.");
				}();
			}
		}

		// Only remember the interactive state now, the first change is not considered interactive. 
		bIsInteractiveChangeOngoing = bInteractive;
		if (IsInteractive())
		{
			AccumulatedDelta = AccumulatedDelta.Get<ValueType>() + Delta;
		}
		else
		{
			GEditor->EndTransaction();

			// Don't remember edited properties and accumulated delta if this is not an interactive change
			PropertiesBeingEditedInteractively.Reset();
			AccumulatedDelta.Set<ValueType>(0);
		}
	}

	template void FAnimDetailsMultiEditUtil::MultiEditChange<double>(const UAnimDetailsProxyManager& ProxyManager, double DesiredDelta, const TSharedRef<IPropertyHandle>& InstigatorProperty, bool bInteractive);
	template void FAnimDetailsMultiEditUtil::MultiEditChange<int64>(const UAnimDetailsProxyManager& ProxyManager, int64 DesiredDelta,const TSharedRef<IPropertyHandle>& InstigatorProperty, bool bInteractive);

	template <typename ValueType>
	bool FAnimDetailsMultiEditUtil::GetInteractiveDelta(const TSharedRef<IPropertyHandle>& Property, ValueType& OutValue) const
	{
		if (PropertiesBeingEditedInteractively.Contains(Property))
		{
			OutValue = AccumulatedDelta.Get<ValueType>();
			return true;
		}

		return false;
	}

	template bool FAnimDetailsMultiEditUtil::GetInteractiveDelta<double>(const TSharedRef<IPropertyHandle>& Property, double& OutValue) const;
	template bool FAnimDetailsMultiEditUtil::GetInteractiveDelta<int64>(const TSharedRef<IPropertyHandle>& Property, int64& OutValue) const;

	template <typename ValueType>
	TArray<TSharedRef<IPropertyHandle>> FAnimDetailsMultiEditUtil::GetPropertiesBeingEdited(const UAnimDetailsProxyManager& ProxyManager, const TSharedRef<IPropertyHandle>& InstigatorProperty)
	{
		const TArray<TWeakPtr<IPropertyHandle>>* PropertyHandlesPtr = ProxyManagerToPropertiesMap.Find(&ProxyManager);
		if (!PropertyHandlesPtr)
		{
			return {};
		}

		TArray<TSharedRef<IPropertyHandle>> PropertiesBeingEdited;

		Algo::TransformIf(*PropertyHandlesPtr, PropertiesBeingEdited,
			[&ProxyManager](const TWeakPtr<IPropertyHandle>& WeakPropertyHandle)
			{
				const UAnimDetailsSelection* Selection = ProxyManager.GetAnimDetailsSelection();
				const TSharedPtr<IPropertyHandle> PropertyHandle = WeakPropertyHandle.Pin();
				if (!Selection ||
					!PropertyHandle.IsValid() ||
					!PropertyHandle->IsValidHandle())
				{
					return false;
				}

				const bool bIsSameType = [&PropertyHandle]()
					{
						if constexpr (std::is_same_v<ValueType, bool>)
						{
							return PropertyHandle->GetPropertyClass() == FBoolProperty::StaticClass();
						}
						else if constexpr (std::is_floating_point_v<ValueType>)
						{
							return PropertyHandle->GetPropertyClass() == FDoubleProperty::StaticClass();
						}
						else if constexpr (std::is_integral_v<ValueType>)
						{
							return PropertyHandle->GetPropertyClass() == FInt64Property::StaticClass();
						}
						else
						{
							return false;
						}
					}();

				return bIsSameType && Selection->IsPropertySelected(PropertyHandle.ToSharedRef());
			},
			[](const TWeakPtr<IPropertyHandle>& WeakPropertyHandle)
			{
				return WeakPropertyHandle.Pin().ToSharedRef();
			});

		// If the instigator is not selected, edit the instigator instead of the currently selected properties.
		const bool bInstigatorIsSelected = Algo::Find(PropertiesBeingEdited, InstigatorProperty) != nullptr;
		if (!bInstigatorIsSelected)
		{
			PropertiesBeingEdited = { InstigatorProperty };
		}

		return PropertiesBeingEdited;
	}

	template <typename ValueType>
	FAnimDetailsMultiEditUtil::FAnimDetailsVariantValue::FAnimDetailsVariantValue(const ValueType Value)
	{
		Set(Value);
	}

	template <typename ValueType>
	void FAnimDetailsMultiEditUtil::FAnimDetailsVariantValue::Set(ValueType Value)
	{
		VariantValue.Set<ValueType>(Value);
	}

	template <typename ValueType>
	ValueType FAnimDetailsMultiEditUtil::FAnimDetailsVariantValue::Get() const
	{
		const ValueType* ValuePtr = VariantValue.TryGet<ValueType>();
		return ValuePtr ? *ValuePtr : 0;
	}
}

#undef LOCTEXT_NAMESPACE
