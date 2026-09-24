// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyEnum.h"

#include "ControlRig.h"
#include "DetailLayoutBuilder.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDetailsProxyEnum)

FName UAnimDetailsProxyEnum::GetCategoryName() const
{
	return "Enum";
}

FName UAnimDetailsProxyEnum::GetDetailRowID() const
{
	if (!bIsIndividual)
	{
		return Enum.EnumType ? Enum.EnumType->GetFName() : NAME_None;
	}

	return UAnimDetailsProxyBase::GetDetailRowID();
}

void UAnimDetailsProxyEnum::UpdatePropertyDisplayNames(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> ValuePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyEnum, Enum), GetClass());
	if (ValuePropertyHandle.IsValid())
	{
		ValuePropertyHandle->SetPropertyDisplayName(GetDisplayNameText());
	}

	if (SequencerItem.IsValid())
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UAnimDetailsProxyEnum, Enum), GET_MEMBER_NAME_STRING_CHECKED(FAnimDetailsEnum, EnumIndex));
		ValuePropertyHandle = DetailBuilder.GetProperty(FName(*PropertyPath), GetClass());
		if (ValuePropertyHandle.IsValid())
		{
			ValuePropertyHandle->SetPropertyDisplayName(FText::FromName(SequencerItem.GetBinding()->GetPropertyName()));
		}
	}
	else if (bIsIndividual)
	{
		const FString PropertyPath = FString::Printf(TEXT("%s.%s"), GET_MEMBER_NAME_STRING_CHECKED(UAnimDetailsProxyEnum, Enum), GET_MEMBER_NAME_STRING_CHECKED(FAnimDetailsEnum, EnumIndex));
		ValuePropertyHandle = DetailBuilder.GetProperty(FName(*PropertyPath), GetClass());
		if (ValuePropertyHandle.IsValid())
		{
			ValuePropertyHandle->SetPropertyDisplayName(GetDisplayNameText());
		}
	}
}

TArray<FName> UAnimDetailsProxyEnum::GetPropertyNames() const
{
	return
	{
		GET_MEMBER_NAME_CHECKED(FAnimDetailsEnum, EnumIndex)
	};
}


bool UAnimDetailsProxyEnum::PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty)
{
	return ((Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimDetailsEnum, EnumIndex)) ||
		(MemberProperty && MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyEnum, Enum)));
}

void UAnimDetailsProxyEnum::UpdateProxyValues()
{
	UControlRig* ControlRig = GetControlRig();
	FRigControlElement* ControlElement = GetControlElement();
	if (!ControlRig || !ControlElement)
	{
		return;
	}

	TObjectPtr<UEnum> EnumType = nullptr;

	int32 Value = 0;
	if (ControlElement->Settings.ControlType == ERigControlType::Integer &&
		ControlElement->Settings.ControlEnum)
	{
		EnumType = ControlElement->Settings.ControlEnum;
		const FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);

		Value = ControlValue.Get<int32>();
	}

	if (EnumType)
	{
		FAnimDetailsEnum EnumValue;
		EnumValue.EnumType = EnumType;
		EnumValue.EnumIndex = Value;

		const FName PropertyName("Enum");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		Binding.CallFunction<FAnimDetailsEnum>(*this, EnumValue);
	}
}

EControlRigContextChannelToKey UAnimDetailsProxyEnum::GetChannelToKeyFromPropertyName(const FName& PropertyName) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsEnum, EnumIndex))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

EControlRigContextChannelToKey UAnimDetailsProxyEnum::GetChannelToKeyFromChannelName(const FString& InChannelName) const
{
	if (InChannelName == TEXT("EnumIndex"))
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

void UAnimDetailsProxyEnum::SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	if (ControlRig && 
		ControlElement &&
		ControlElement->Settings.ControlType == ERigControlType::Integer &&
		ControlElement->Settings.ControlEnum)
	{
		int32 Val = Enum.EnumIndex;

		constexpr bool bNotify = true;
		constexpr bool bSetupUndo = false;
		ControlRig->SetControlValue<int32>(ControlElement->GetKey().Name, Val, bNotify, Context, bSetupUndo);

		ControlRig->Evaluate_AnyThread();
	}
}
