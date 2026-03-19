// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyBool.h"

#include "ControlRig.h"
#include "DetailLayoutBuilder.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDetailsProxyBool)

FName UAnimDetailsProxyBool::GetCategoryName() const
{
	return "Bool";
}

void UAnimDetailsProxyBool::UpdatePropertyDisplayNames(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> ValuePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyBool, Bool), GetClass());
	if (ValuePropertyHandle.IsValid())
	{
		ValuePropertyHandle->SetPropertyDisplayName(GetDisplayNameText());
	}

	if (bIsIndividual)
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UAnimDetailsProxyBool, Bool), GET_MEMBER_NAME_STRING_CHECKED(FAnimDetailsBool, Bool));
		ValuePropertyHandle = DetailBuilder.GetProperty(FName(*PropertyPath), GetClass());
		if (ValuePropertyHandle.IsValid())
		{
			ValuePropertyHandle->SetPropertyDisplayName(GetDisplayNameText());
		}
	}
	else
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UAnimDetailsProxyBool, Bool), GET_MEMBER_NAME_STRING_CHECKED(FAnimDetailsBool, Bool));
		ValuePropertyHandle = DetailBuilder.GetProperty(FName(*PropertyPath), GetClass());
		if (ValuePropertyHandle.IsValid() && SequencerItem.IsValid())
		{
			ValuePropertyHandle->SetPropertyDisplayName(FText::FromName(SequencerItem.GetBinding()->GetPropertyName()));
		}
	}
}

TArray<FName> UAnimDetailsProxyBool::GetPropertyNames() const
{
	return
	{
		GET_MEMBER_NAME_CHECKED(FAnimDetailsBool, Bool)
	};
}

bool UAnimDetailsProxyBool::PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty)
{
	return ((Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimDetailsBool, Bool)) ||
		(MemberProperty && MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyBool, Bool)));
}

void UAnimDetailsProxyBool::UpdateProxyValues()
{
	bool Value = false;

	UControlRig* ControlRig = GetControlRig();
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlRig && ControlElement)
	{
		if (ControlElement->Settings.ControlType == ERigControlType::Bool)
		{
			FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
			Value = ControlValue.Get<bool>();
		}
	}
	else if (SequencerItem.IsValid())
	{
		const TOptional<bool> OptionalValue = SequencerItem.GetBinding()->GetOptionalValue<bool>(*SequencerItem.GetBoundObject());
		Value = OptionalValue.IsSet() ? OptionalValue.GetValue() : false;
	}

	const FAnimDetailsBool AnimDetailProxyBool(Value);

	const FName PropName("Bool");
	FTrackInstancePropertyBindings Binding(PropName, PropName.ToString());
	Binding.CallFunction<FAnimDetailsBool>(*this, AnimDetailProxyBool);
}

EControlRigContextChannelToKey UAnimDetailsProxyBool::GetChannelToKeyFromPropertyName(const FName& PropertyName) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsBool, Bool))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

EControlRigContextChannelToKey UAnimDetailsProxyBool::GetChannelToKeyFromChannelName(const FString& InChannelName) const
{
	if (InChannelName == TEXT("Bool"))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}

	const UControlRig* ControlRig = GetControlRig();
	const FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlElement->GetDisplayName() == InChannelName)
	{
		return EControlRigContextChannelToKey::TranslationX;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

void UAnimDetailsProxyBool::SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	if (ControlElement && 
		ControlRig && 
		ControlElement->Settings.ControlType == ERigControlType::Bool)
	{
		const bool Value = Bool.Bool;

		constexpr bool bNotify = true;
		constexpr bool bSetupUndo = false;
		ControlRig->SetControlValue<bool>(ControlElement->GetKey().Name, Value, bNotify, Context, bSetupUndo);

		ControlRig->Evaluate_AnyThread();
	}
}

void UAnimDetailsProxyBool::SetBindingValueFromCurrent(UObject* InObject, const TSharedPtr<FTrackInstancePropertyBindings>& Binding, const FRigControlModifiedContext& Context, bool bInteractive)
{
	if (InObject && Binding.IsValid())
	{
		Binding->SetCurrentValue<bool>(*InObject, Bool.Bool);
	}
}
