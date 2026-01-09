// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyRotation.h"

#include "ControlRig.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDetailsProxyRotation)

namespace UE::ControlRigEditor::RotationUtils
{
	static void SetRotationValuesFromContext(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context, FVector3f& Val)
	{
		FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
		FVector3f Value = ControlValue.Get<FVector3f>();

		EControlRigContextChannelToKey ChannelsToKey = (EControlRigContextChannelToKey)Context.KeyMask;
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationX))
		{
			Val.X = Value.X;
		}
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationZ))
		{
			Val.Y = Value.Y;
		}
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::RotationY))
		{
			Val.Z = Value.Z;
		}
	}
}

FAnimDetailsRotation::FAnimDetailsRotation(const FRotator& InRotator)
{ 
	FromRotator(InRotator); 
}

FAnimDetailsRotation::FAnimDetailsRotation(const FVector3f& InVector)
	: RX(InVector.X)
	, RY(InVector.Y)
	, RZ(InVector.Z)
{}

FVector FAnimDetailsRotation::ToVector() const
{
	return FVector(RX, RY, RZ);
}

FVector3f FAnimDetailsRotation::ToVector3f() const
{ 
	return FVector3f(RX, RY, RZ);
}

FRotator FAnimDetailsRotation::ToRotator() const
{ 
	FRotator Rot; 
	Rot = Rot.MakeFromEuler(ToVector()); 
	
	return Rot; 
}

void FAnimDetailsRotation::FromRotator(const FRotator& InRotator)
{ 
	FVector Vec(InRotator.Euler()); 
	RX = Vec.X; 
	RY = Vec.Y; 
	RZ = Vec.Z; 
}

FName UAnimDetailsProxyRotation::GetCategoryName() const
{
	return "Rotation";
}

TArray<FName> UAnimDetailsProxyRotation::GetPropertyNames() const
{
	return
	{
		GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RX),
		GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RY), 
		GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RZ),
	};
}

void UAnimDetailsProxyRotation::GetLocalizedPropertyName(const FName& InPropertyName, FText& OutPropertyDisplayName, TOptional<FText>& OutOptionalStructDisplayName) const
{
	OutOptionalStructDisplayName = UAnimDetailsProxyRotation::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyRotation, Rotation))->GetDisplayNameText();

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RX))
	{
		OutPropertyDisplayName = FAnimDetailsRotation::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RX))->GetDisplayNameText();
	}
	else if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RY))
	{
		OutPropertyDisplayName = FAnimDetailsRotation::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RY))->GetDisplayNameText();
	}
	else if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RZ))
	{
		OutPropertyDisplayName = FAnimDetailsRotation::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RZ))->GetDisplayNameText();
	}
	else
	{
		ensureMsgf(0, TEXT("Cannot find member property for anim details proxy, cannot get property name text"));
	}
}

bool UAnimDetailsProxyRotation::PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty)
{
	return ((Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyRotation, Rotation)) ||
		(MemberProperty && MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyRotation, Rotation)));
}

void UAnimDetailsProxyRotation::UpdateProxyValues()
{
	UControlRig* ControlRig = GetControlRig();
	FRigControlElement* ControlElement = GetControlElement();
	if (!ControlRig || !ControlElement)
	{
		return;
	}
	
	FVector3f Value = FVector3f::ZeroVector;
	if (ControlElement->Settings.ControlType == ERigControlType::Rotator)
	{
		FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
		Value = ControlValue.Get<FVector3f>();
	}

	const FAnimDetailsRotation AnimDetailProxyRotation(Value);

	const FName PropName("Rotation");
	FTrackInstancePropertyBindings Binding(PropName, PropName.ToString());
	Binding.CallFunction<FAnimDetailsRotation>(*this, AnimDetailProxyRotation);
}

EControlRigContextChannelToKey UAnimDetailsProxyRotation::GetChannelToKeyFromPropertyName(const FName& PropertyName) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RX))
	{
		return EControlRigContextChannelToKey::RotationX;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RY))
	{
		return EControlRigContextChannelToKey::RotationY;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsRotation, RZ))
	{
		return EControlRigContextChannelToKey::RotationZ;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

EControlRigContextChannelToKey UAnimDetailsProxyRotation::GetChannelToKeyFromChannelName(const FString& InChannelName) const
{
	if (InChannelName == TEXT("X"))
	{
		return EControlRigContextChannelToKey::RotationX;
	}
	else if (InChannelName == TEXT("Y"))
	{
		return EControlRigContextChannelToKey::RotationY;
	}
	else if (InChannelName == TEXT("Z"))
	{
		return EControlRigContextChannelToKey::RotationZ;
	}
	return EControlRigContextChannelToKey::AllTransform;
}

void UAnimDetailsProxyRotation::SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	using namespace UE::ControlRigEditor;

	URigHierarchy* Hierarchy = ControlRig ? ControlRig->GetHierarchy() : nullptr;
	if (ControlRig && 
		ControlElement &&		
		ControlElement->Settings.ControlType == ERigControlType::Rotator &&
		Hierarchy)
	{
		FVector3f Value = Rotation.ToVector3f();
		RotationUtils::SetRotationValuesFromContext(ControlRig, ControlElement, Context, Value);

		const FVector EulerAngle(Rotation.ToRotator().Roll, Rotation.ToRotator().Pitch, Rotation.ToRotator().Yaw);
		const FRotator Rotator(Hierarchy->GetControlQuaternion(ControlElement, EulerAngle));

		Hierarchy->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);

		constexpr bool bNotify = true;
		constexpr bool bSetupUndo = false;
		ControlRig->SetControlValue<FRotator>(ControlElement->GetKey().Name, Rotator, bNotify, Context, bSetupUndo);

		ControlRig->Evaluate_AnyThread();
	}
}
