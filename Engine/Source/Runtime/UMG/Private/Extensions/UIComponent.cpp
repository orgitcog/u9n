// Copyright Epic Games, Inc. All Rights Reserved.

#include "Extensions/UIComponent.h"

#include "CoreGlobals.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/BlueprintGeneratedClass.h"

void UUIComponent::Initialize(UWidget* Target)
{
	ensure(Target);
	ensure(Owner.IsExplicitlyNull());
	Owner = Target;
	OnInitialize();
}

void UUIComponent::PreConstruct(bool bIsDesignTime)
{
	OnPreConstruct(bIsDesignTime);
}


void UUIComponent::Construct()
{
	OnConstruct();
}

void UUIComponent::Destruct()
{
	OnDestruct();
}

TWeakObjectPtr<UWidget> UUIComponent::GetOwner() const
{
	return Owner;
}

void UUIComponent::OnInitialize()
{

}

void UUIComponent::OnPreConstruct(bool bIsDesignTime)
{

}

void UUIComponent::OnConstruct()
{

}

void UUIComponent::OnDestruct()
{

}

void UUIComponent::FFieldNotificationClassDescriptor::ForEachField(const UClass* Class, TFunctionRef<bool(::UE::FieldNotification::FFieldId FieldId)> Callback) const
{
	if (const UBlueprintGeneratedClass* BPClass = Cast<const UBlueprintGeneratedClass>(Class))
	{
		BPClass->ForEachFieldNotify(Callback, true);
	}
}

FDelegateHandle UUIComponent::AddFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FFieldValueChangedDelegate InNewDelegate)
{
	return (InFieldId.IsValid()) ? Delegates.Add(this, InFieldId, MoveTemp(InNewDelegate)) : FDelegateHandle();
}

bool UUIComponent::RemoveFieldValueChangedDelegate(UE::FieldNotification::FFieldId InFieldId, FDelegateHandle InHandle)
{
	if (InFieldId.IsValid() && InHandle.IsValid())
	{
		return Delegates.RemoveFrom(this, InFieldId, InHandle).bRemoved;
	}
	return false;
}

int32 UUIComponent::RemoveAllFieldValueChangedDelegates(FDelegateUserObjectConst InUserObject)
{
	if (InUserObject)
	{
		return Delegates.RemoveAll(this, InUserObject).RemoveCount;
	}
	return false;
}

int32 UUIComponent::RemoveAllFieldValueChangedDelegates(UE::FieldNotification::FFieldId InFieldId, FDelegateUserObjectConst InUserObject)
{
	if (InFieldId.IsValid() && InUserObject)
	{
		return Delegates.RemoveAll(this, InFieldId, InUserObject).RemoveCount;
	}
	return false;
}

void UUIComponent::BroadcastFieldValueChanged(UE::FieldNotification::FFieldId InFieldId)
{
	if (InFieldId.IsValid())
	{
		Delegates.Broadcast(this, InFieldId);
	}
}

const UE::FieldNotification::IClassDescriptor& UUIComponent::GetFieldNotificationDescriptor() const
{
	static FFieldNotificationClassDescriptor Local;
	return Local;
}