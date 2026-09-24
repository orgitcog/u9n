// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyFloat.h"

#include "ControlRig.h"
#include "DetailLayoutBuilder.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDetailsProxyFloat)

FName UAnimDetailsProxyFloat::GetCategoryName() const
{
	return "Float";
}

void UAnimDetailsProxyFloat::UpdatePropertyDisplayNames(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> ValuePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyFloat, Float), GetClass());
	if (ValuePropertyHandle.IsValid())
	{
		ValuePropertyHandle->SetPropertyDisplayName(GetDisplayNameText());
	}

	if (bIsIndividual)
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UAnimDetailsProxyFloat, Float), GET_MEMBER_NAME_STRING_CHECKED(FAnimDetailsFloat, Float));
		ValuePropertyHandle = DetailBuilder.GetProperty(FName(*PropertyPath), GetClass());
		if (ValuePropertyHandle.IsValid())
		{
			ValuePropertyHandle->SetPropertyDisplayName(GetDisplayNameText());
		}
	}
	else if (SequencerItem.IsValid())
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UAnimDetailsProxyFloat, Float), GET_MEMBER_NAME_STRING_CHECKED(FAnimDetailsFloat, Float));
		ValuePropertyHandle = DetailBuilder.GetProperty(FName(*PropertyPath), GetClass());
		if (ValuePropertyHandle.IsValid())
		{
			ValuePropertyHandle->SetPropertyDisplayName(FText::FromName(SequencerItem.GetBinding()->GetPropertyName()));
		}
	}
}

TArray<FName> UAnimDetailsProxyFloat::GetPropertyNames() const
{
	return
	{
		GET_MEMBER_NAME_CHECKED(FAnimDetailsFloat, Float)
	};
}

bool UAnimDetailsProxyFloat::PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty)
{
	return ((Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimDetailsFloat, Float)) ||
		(MemberProperty && MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyFloat, Float)));
}

void UAnimDetailsProxyFloat::UpdateProxyValues()
{
	UControlRig* ControlRig = GetControlRig();
	FRigControlElement* ControlElement = GetControlElement();

	double Value = 0.f;
	if (ControlRig && ControlElement)
	{
		if (ControlElement->Settings.ControlType == ERigControlType::Float ||
			(ControlElement->Settings.ControlType == ERigControlType::ScaleFloat))
		{
			FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
			Value = ControlValue.Get<float>();
		}
	}
	else if (SequencerItem.IsValid())
	{
		if (FProperty* Property = SequencerItem.GetProperty())
		{
			if (Property->IsA(FDoubleProperty::StaticClass()))
			{
				const TOptional<double> OptionalValue = SequencerItem.GetBinding()->GetOptionalValue<double>(*SequencerItem.GetBoundObject());
				if (OptionalValue.IsSet())
				{
					Value = OptionalValue.GetValue();
				}
			}
			else if (Property->IsA(FFloatProperty::StaticClass()))
			{
				const TOptional<float> OptionalValue = SequencerItem.GetBinding()->GetOptionalValue<float>(*SequencerItem.GetBoundObject());
				if (OptionalValue.IsSet())
				{
					Value = OptionalValue.GetValue();
				}
			}
		}
	}

	const FAnimDetailsFloat AnimDetailProxyFloat(Value);

	const FName PropName("Float");
	FTrackInstancePropertyBindings Binding(PropName, PropName.ToString());
	Binding.CallFunction<FAnimDetailsFloat>(*this, AnimDetailProxyFloat);
}

EControlRigContextChannelToKey UAnimDetailsProxyFloat::GetChannelToKeyFromPropertyName(const FName& PropertyName) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsFloat, Float))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

EControlRigContextChannelToKey UAnimDetailsProxyFloat::GetChannelToKeyFromChannelName(const FString& InChannelName) const
{
	if (InChannelName == TEXT("Float"))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}

	const UControlRig* ControlRig = GetControlRig();
	const FRigElementKey ElementKey = GetControlElementKey();

	const FRigControlElement* ControlElement = ControlRig ? ControlRig->FindControl(ElementKey.Name) : nullptr;
	if (ControlElement && ControlElement->GetDisplayName() == InChannelName)
	{
		return EControlRigContextChannelToKey::TranslationX;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

void UAnimDetailsProxyFloat::SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	if (ControlRig && 
		ControlElement &&
		(ControlElement->Settings.ControlType == ERigControlType::Float || ControlElement->Settings.ControlType == ERigControlType::ScaleFloat))
	{
		float Value = Float.Float;

		constexpr bool bNotify = true;
		constexpr bool bSetupUndo = false;
		ControlRig->SetControlValue<float>(ControlElement->GetKey().Name, Value, bNotify, Context, bSetupUndo);

		ControlRig->Evaluate_AnyThread();
	}
}

void UAnimDetailsProxyFloat::SetBindingValueFromCurrent(UObject* InObject, const TSharedPtr<FTrackInstancePropertyBindings>& Binding, const FRigControlModifiedContext& Context, bool bInteractive)
{
	if (InObject && Binding.IsValid())
	{
		if (FProperty* Property = Binding->GetProperty((*(InObject))))
		{
			if (Property->IsA(FDoubleProperty::StaticClass()))
			{
				Binding->SetCurrentValue<double>(*InObject, Float.Float);
			}
			else if (Property->IsA(FFloatProperty::StaticClass()))
			{
				float FVal = (float)Float.Float;
				Binding->SetCurrentValue<float>(*InObject, FVal);
			}
		}
	}
}
