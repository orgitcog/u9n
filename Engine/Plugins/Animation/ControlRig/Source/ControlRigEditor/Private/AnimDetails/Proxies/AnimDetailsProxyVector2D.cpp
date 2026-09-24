// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimDetailsProxyVector2D.h"

#include "ControlRig.h"
#include "MovieSceneCommonHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimDetailsProxyVector2D)

namespace UE::ControlRigEditor::Vector2DUtils
{
	static void SetVector2DValuesFromContext(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context, FVector2D& Val)
	{
		FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
		FVector3f Value = ControlValue.Get<FVector3f>();

		EControlRigContextChannelToKey ChannelsToKey = (EControlRigContextChannelToKey)Context.KeyMask;
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationX))
		{
			Val.X = Value.X;
		}
		if (!EnumHasAnyFlags(ChannelsToKey, EControlRigContextChannelToKey::TranslationY))
		{
			Val.Y = Value.Y;
		}
	}
}

FAnimDetailsVector2D::FAnimDetailsVector2D(const FVector2D& InVector)
	: X(InVector.X)
	, Y(InVector.Y)
{}

FVector2D FAnimDetailsVector2D::ToVector2D() const
{ 
	return FVector2D(X, Y); 
}

FName UAnimDetailsProxyVector2D::GetCategoryName() const
{
	return "Vector2D";
}

TArray<FName> UAnimDetailsProxyVector2D::GetPropertyNames() const
{
	return
	{
		GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, X),
		GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, Y)
	};
}

void UAnimDetailsProxyVector2D::GetLocalizedPropertyName(const FName& InPropertyName, FText& OutPropertyDisplayName, TOptional<FText>& OutOptionalStructDisplayName) const
{
	OutOptionalStructDisplayName = UAnimDetailsProxyVector2D::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyVector2D, Vector2D))->GetDisplayNameText();

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, X))
	{
		OutPropertyDisplayName = FAnimDetailsVector2D::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, X))->GetDisplayNameText();
	}
	else if (InPropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, Y))
	{
		OutPropertyDisplayName = FAnimDetailsVector2D::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, Y))->GetDisplayNameText();
	}
	else
	{
		ensureMsgf(0, TEXT("Cannot find member property for anim details proxy, cannot get property name text"));
	}
}

bool UAnimDetailsProxyVector2D::PropertyIsOnProxy(const FProperty* Property, const FProperty* MemberProperty)
{
	return ((Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyVector2D, Vector2D)) ||
		(MemberProperty && MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimDetailsProxyVector2D, Vector2D)));
}

void UAnimDetailsProxyVector2D::UpdateProxyValues()
{
	UControlRig* ControlRig = GetControlRig();
	FRigControlElement* ControlElement = GetControlElement();
	if (!ControlRig || !ControlElement)
	{
		return;
	}

	FVector2D Value = FVector2D::ZeroVector;
	if (ControlElement->Settings.ControlType == ERigControlType::Vector2D)
	{
		FRigControlValue ControlValue = ControlRig->GetControlValue(ControlElement, ERigControlValueType::Current);
		//FVector2D version deleted for some reason so need to convert
		FVector3f Val = ControlValue.Get<FVector3f>();
		Value = FVector2D(Val.X, Val.Y);
	}

	const FAnimDetailsVector2D AnimDetailProxyVector2D(Value);

	const FName PropName("Vector2D");
	FTrackInstancePropertyBindings Binding(PropName, PropName.ToString());
	Binding.CallFunction<FAnimDetailsVector2D>(*this, AnimDetailProxyVector2D);
}

EControlRigContextChannelToKey UAnimDetailsProxyVector2D::GetChannelToKeyFromPropertyName(const FName& PropertyName) const
{
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, X))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailsVector2D, Y))
	{
		return EControlRigContextChannelToKey::TranslationY;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

EControlRigContextChannelToKey UAnimDetailsProxyVector2D::GetChannelToKeyFromChannelName(const FString& InChannelName) const
{
	if (InChannelName == TEXT("X"))
	{
		return EControlRigContextChannelToKey::TranslationX;
	}
	else if (InChannelName == TEXT("Y"))
	{
		return EControlRigContextChannelToKey::TranslationY;
	}

	return EControlRigContextChannelToKey::AllTransform;
}

void UAnimDetailsProxyVector2D::SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	using namespace UE::ControlRigEditor;

	if (ControlRig && 
		ControlElement && 
		ControlElement->Settings.ControlType == ERigControlType::Vector2D)
	{
		FVector2D Value = Vector2D.ToVector2D();
		Vector2DUtils::SetVector2DValuesFromContext(ControlRig, ControlElement, Context, Value);

		constexpr bool bNotify = true;
		constexpr bool bSetupUndo = false;
		ControlRig->SetControlValue<FVector2D>(ControlElement->GetKey().Name, Value, bNotify, Context, bSetupUndo);

		ControlRig->Evaluate_AnyThread();
	}
}
