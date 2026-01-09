// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyInteger.h"

#include "ControlRig.h"
#include "DetailLayoutBuilder.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDetailsProxyInteger)

FName UAnimDetailsProxyInteger::GetCategoryName() const
{
	return "Integer";
}

void UAnimDetailsProxyInteger::UpdatePropertyDisplayNames(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> ValuePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyInteger, Integer), GetClass());
	if (ValuePropertyHandle.IsValid())
	{
		ValuePropertyHandle->SetPropertyDisplayName(GetDisplayNameText());
	}

	if (SequencerItem.IsValid())
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UAnimDetailsProxyInteger, Integer), GET_MEMBER_NAME_STRING_CHECKED(FAnimDetailsInteger, Integer));
		ValuePropertyHandle = DetailBuilder.GetProperty(FName(*PropertyPath), GetClass());
		if (ValuePropertyHandle.IsValid())
		{
			ValuePropertyHandle->SetPropertyDisplayName(FText::FromName(SequencerItem.GetBinding()->GetPropertyName()));
		}
	}
	else if (bIsIndividual)
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UAnimDetailsProxyInteger, Integer), GET_MEMBER_NAME_STRING_CHECKED(FAnimDetailsInteger, Integer));
		ValuePropertyHandle = DetailBuilder.GetProperty(FName(*PropertyPath), GetClass());
		if (ValuePropertyHandle.IsValid())
		{
			ValuePropertyHandle->SetPropertyDisplayName(GetDisplayNameText());
		}
	}
}

TArray<FName> UAnimDetailsProxyInteger::GetPropertyNames() const
{
	return 
	{ 
		GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyInteger, Integer)
	};
}

bool UAnimDetailsProxyInteger::PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty)
{
	return ((Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimDetailsInteger, Integer)) ||
		(MemberProperty && MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyInteger, Integer)));
}

void UAnimDetailsProxyInteger::UpdateProxyValues()
{
	int64 Value = 0;

	UControlRig* ControlRig = GetControlRig();
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlRig && ControlElement)
	{
		if (ControlElement->Settings.ControlType == ERigControlType::Integer)
		{
			FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
			Value = ControlValue.Get<int32>();
		}
	}
	else if (SequencerItem.IsValid())
	{
		if (const TOptional<int64> OptionalValue = SequencerItem.GetBinding()->GetOptionalValue<int64>(*SequencerItem.GetBoundObject()))
		{
			if (OptionalValue.IsSet())
			{
				Value = OptionalValue.GetValue();
			}
		}
	}

	const FAnimDetailsInteger AnimDetailProxyInteger(Value);

	const FName PropName("Integer");
	FTrackInstancePropertyBindings Binding(PropName, PropName.ToString());
	Binding.CallFunction<FAnimDetailsInteger>(*this, AnimDetailProxyInteger);
}

EControlRigContextChannelToKey UAnimDetailsProxyInteger::GetChannelToKeyFromPropertyName(const FName& PropertyName) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyInteger, Integer))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

EControlRigContextChannelToKey UAnimDetailsProxyInteger::GetChannelToKeyFromChannelName(const FString& InChannelName) const
{
	if (InChannelName == TEXT("Integer"))
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

void UAnimDetailsProxyInteger::SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	if (ControlRig && 
		ControlElement &&		
		ControlElement->Settings.ControlType == ERigControlType::Integer &&
		!ControlElement->Settings.ControlEnum)
	{
		int32 Val = (int32)Integer.Integer;

		constexpr bool bNotify = true;
		constexpr bool bSetupUndo = false;
		ControlRig->SetControlValue<int32>(ControlElement->GetKey().Name, Val, bNotify, Context, bSetupUndo);

		ControlRig->Evaluate_AnyThread();
	}
}

void UAnimDetailsProxyInteger::SetBindingValueFromCurrent(UObject* InObject, const TSharedPtr<FTrackInstancePropertyBindings>& Binding, const FRigControlModifiedContext& Context, bool bInteractive)
{
	if (InObject && Binding.IsValid())
	{
		Binding->SetCurrentValue<int64>(*InObject, Integer.Integer);
	}
}