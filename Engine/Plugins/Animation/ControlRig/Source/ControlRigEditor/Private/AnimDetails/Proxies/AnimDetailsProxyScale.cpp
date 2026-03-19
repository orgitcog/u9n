// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyScale.h"

#include "ControlRig.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDetailsProxyScale)

namespace UE::ControlRigEditor::ScaleUtils
{
	static void SetScaleValuesFromContext(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context, FVector3f& TScale)
	{
		FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
		FVector3f Value = ControlValue.Get<FVector3f>();

		EControlRigContextChannelToKey ChannelsToKey = (EControlRigContextChannelToKey)Context.KeyMask;

		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleX))
		{
			TScale.X = Value.X;
		}
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleY))
		{
			TScale.Y = Value.Y;
		}
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::ScaleZ))
		{
			TScale.Z = Value.Z;
		}
	}
}

FAnimDetailsScale::FAnimDetailsScale(const FVector& InVector)
	: SX(InVector.X)
	, SY(InVector.Y)
	, SZ(InVector.Z)
{}

FAnimDetailsScale::FAnimDetailsScale(const FVector3f& InVector)
	: SX(InVector.X)
	, SY(InVector.Y)
	, SZ(InVector.Z)
{}

FVector FAnimDetailsScale::ToVector() const
{ 
	return FVector(SX, SY, SZ); 
}

FVector3f FAnimDetailsScale::ToVector3f() const
{
	return FVector3f(SX, SX, SZ); 
}

FName UAnimDetailsProxyScale::GetCategoryName() const
{
	return "Scale";
}

TArray<FName> UAnimDetailsProxyScale::GetPropertyNames() const
{
	return
	{
		GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SX),
		GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SY),
		GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SZ),
	};
}

void UAnimDetailsProxyScale::GetLocalizedPropertyName(const FName& InPropertyName, FText& OutPropertyDisplayName, TOptional<FText>& OutOptionalStructDisplayName) const
{
	OutOptionalStructDisplayName = UAnimDetailsProxyScale::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyScale, Scale))->GetDisplayNameText();

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SX))
	{
		OutPropertyDisplayName = FAnimDetailsScale::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SX))->GetDisplayNameText();
	}
	else if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SY))
	{
		OutPropertyDisplayName = FAnimDetailsScale::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SY))->GetDisplayNameText();
	}
	else if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SZ))
	{
		OutPropertyDisplayName = FAnimDetailsScale::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SZ))->GetDisplayNameText();
	}
	else
	{
		ensureMsgf(0, TEXT("Cannot find member property for anim details proxy, cannot get property name text"));
	}
}

bool UAnimDetailsProxyScale::PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty)
{
	return ((Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyScale, Scale)) ||
		(MemberProperty && MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyScale, Scale)));
}

void UAnimDetailsProxyScale::UpdateProxyValues()
{
	UControlRig* ControlRig = GetControlRig();
	FRigControlElement* ControlElement = GetControlElement();
	if (!ControlRig || !ControlElement)
	{
		return;
	}

	FVector3f Value = FVector3f::ZeroVector;
	if (ControlElement->Settings.ControlType == ERigControlType::Scale)
	{
		FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
		Value = ControlValue.Get<FVector3f>();
	}

	//set from values, note that the fact if they were multiples or not was already set so we need to set them on these values.
	const FAnimDetailsScale AnimDetailProxyScale(Value);

	const FName PropName("Scale");
	FTrackInstancePropertyBindings Binding(PropName, PropName.ToString());
	Binding.CallFunction<FAnimDetailsScale>(*this, AnimDetailProxyScale);
}

EControlRigContextChannelToKey UAnimDetailsProxyScale::GetChannelToKeyFromPropertyName(const FName& PropertyName) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SX))
	{
		return EControlRigContextChannelToKey::ScaleX;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SY))
	{
		return EControlRigContextChannelToKey::ScaleY;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsScale, SZ))
	{
		return EControlRigContextChannelToKey::ScaleZ;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

EControlRigContextChannelToKey UAnimDetailsProxyScale::GetChannelToKeyFromChannelName(const FString& InChannelName) const
{
	if (InChannelName == TEXT("X"))
	{
		return EControlRigContextChannelToKey::ScaleX;
	}
	else if (InChannelName == TEXT("Y"))
	{
		return EControlRigContextChannelToKey::ScaleY;
	}
	else if (InChannelName == TEXT("Z"))
	{
		return EControlRigContextChannelToKey::ScaleZ;
	}
	return EControlRigContextChannelToKey::AllTransform;
}

void UAnimDetailsProxyScale::SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	using namespace UE::ControlRigEditor;

	if (ControlRig && 
		ControlElement &&		
		ControlElement->Settings.ControlType == ERigControlType::Scale)
	{
		FVector3f Val = Scale.ToVector3f();
		ScaleUtils::SetScaleValuesFromContext(ControlRig, ControlElement, Context, Val);

		constexpr bool bNotify = true;
		constexpr bool bSetupUndo = false;
		ControlRig->SetControlValue<FVector3f>(ControlElement->GetKey().Name, Val, bNotify, Context, bSetupUndo);

		ControlRig->Evaluate_AnyThread();
	}
}
