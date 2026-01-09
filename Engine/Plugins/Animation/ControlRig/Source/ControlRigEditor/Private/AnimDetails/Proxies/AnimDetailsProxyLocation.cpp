// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyLocation.h"

#include "ControlRig.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDetailsProxyLocation)

namespace UE::ControlRigEditor::LocationUtils
{
	static void SetLocationValuesFromContext(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context, FVector3f& TLocation)
	{
		const FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
		const FVector3f Value = ControlValue.Get<FVector3f>();

		const EControlRigContextChannelToKey ChannelsToKey = (EControlRigContextChannelToKey)Context.KeyMask;
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationX))
		{
			TLocation.X = Value.X;
		}
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationY))
		{
			TLocation.Y = Value.Y;
		}
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationZ))
		{
			TLocation.Z = Value.Z;
		}
	}
}

FAnimDetailsLocation::FAnimDetailsLocation(const FVector& InVector)
	: LX(InVector.X)
	, LY(InVector.Y)
	, LZ(InVector.Z)
{}

FAnimDetailsLocation::FAnimDetailsLocation(const FVector3f& InVector)
	: LX(InVector.X)
	, LY(InVector.Y)
	, LZ(InVector.Z)
{}

FName UAnimDetailsProxyLocation::GetCategoryName() const
{
	return "Location";
}

TArray<FName> UAnimDetailsProxyLocation::GetPropertyNames() const
{
	return
	{
		GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LX),
		GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LY),
		GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LZ)
	};
}

void UAnimDetailsProxyLocation::GetLocalizedPropertyName(const FName& InPropertyName, FText& OutPropertyDisplayName, TOptional<FText>& OutOptionalStructDisplayName) const
{
	OutOptionalStructDisplayName = UAnimDetailsProxyLocation::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyLocation, Location))->GetDisplayNameText();

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LX))
	{
		OutPropertyDisplayName = FAnimDetailsLocation::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LX))->GetDisplayNameText();
	}
	else if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LY))
	{
		OutPropertyDisplayName = FAnimDetailsLocation::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LY))->GetDisplayNameText();
	}
	else if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LZ))
	{
		OutPropertyDisplayName = FAnimDetailsLocation::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LZ))->GetDisplayNameText();
	}
	else
	{
		ensureMsgf(0, TEXT("Cannot find member property for anim details proxy, cannot get property name text"));
	}
}

bool UAnimDetailsProxyLocation::PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty)
{
	return ((Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyLocation, Location)) ||
		(MemberProperty && MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyLocation, Location)));
}

void UAnimDetailsProxyLocation::UpdateProxyValues()
{
	UControlRig* ControlRig = GetControlRig();
	FRigControlElement* ControlElement = GetControlElement();
	if (!ControlRig || !ControlElement)
	{
		return;
	}

	FVector3f Value = FVector3f::ZeroVector;
	if (ControlElement->Settings.ControlType == ERigControlType::Position)
	{
		FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
		Value = ControlValue.Get<FVector3f>();
	}

	//set from values, note that the fact if they were multiples or not was already set so we need to set them on these values.
	const FAnimDetailsLocation AnimDetailProxyLocation(Value);

	const FName LocationName("Location");
	FTrackInstancePropertyBindings LocationBinding(LocationName, LocationName.ToString());
	LocationBinding.CallFunction<FAnimDetailsLocation>(*this, AnimDetailProxyLocation);
}

EControlRigContextChannelToKey UAnimDetailsProxyLocation::GetChannelToKeyFromPropertyName(const FName& PropertyName) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LX))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LY))
	{
		return EControlRigContextChannelToKey::TranslationY;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsLocation, LZ))
	{
		return EControlRigContextChannelToKey::TranslationZ;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

EControlRigContextChannelToKey UAnimDetailsProxyLocation::GetChannelToKeyFromChannelName(const FString& InChannelName) const
{
	if (InChannelName == TEXT("X"))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}
	else if (InChannelName == TEXT("Y"))
	{
		return EControlRigContextChannelToKey::TranslationY;
	}
	else if (InChannelName == TEXT("Z"))
	{
		return EControlRigContextChannelToKey::TranslationZ;
	}
	
	return EControlRigContextChannelToKey::AllTransform;
}

void UAnimDetailsProxyLocation::SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	using namespace UE::ControlRigEditor;

	if (ControlRig && 
		ControlElement &&		
		ControlElement->Settings.ControlType == ERigControlType::Position)
	{
		FVector3f TLocation = Location.ToVector3f();
		LocationUtils::SetLocationValuesFromContext(ControlRig, ControlElement, Context, TLocation);

		constexpr bool bNotify = true;
		constexpr bool bSetupUndo = false;
		ControlRig->SetControlValue<FVector3f>(ControlElement->GetKey().Name, TLocation, bNotify, Context, bSetupUndo);

		ControlRig->Evaluate_AnyThread();
	}
}
