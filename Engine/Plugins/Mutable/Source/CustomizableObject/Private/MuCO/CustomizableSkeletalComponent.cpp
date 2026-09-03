// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableSkeletalComponent.h"

#include "MuCO/CustomizableObjectInstanceUsagePrivate.h"
#include "MuCO/CustomizableSkeletalComponentPrivate.h"

#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "UObject/UObjectGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "MuCO/CustomizableObject.h"


UCustomizableSkeletalComponent::UCustomizableSkeletalComponent()
{
	Private = CreateDefaultSubobject<UCustomizableSkeletalComponentPrivate>(TEXT("Private"));
}


UCustomizableSkeletalComponentPrivate::UCustomizableSkeletalComponentPrivate()
{
	InstanceUsage = CreateDefaultSubobject<UCustomizableObjectInstanceUsage>(TEXT("InstanceUsage"));
}


void UCustomizableSkeletalComponentPrivate::Callbacks() const
{
	if (InstanceUsage)
	{
		InstanceUsage->GetPrivate()->Callbacks();
	}
}


USkeletalMesh* UCustomizableSkeletalComponentPrivate::GetSkeletalMesh() const
{
	if (InstanceUsage)
	{
		return InstanceUsage->GetPrivate()->GetSkeletalMesh();
	}

	return nullptr;
}


USkeletalMesh* UCustomizableSkeletalComponentPrivate::GetAttachedSkeletalMesh() const
{
	if (InstanceUsage)
	{
		return InstanceUsage->GetPrivate()->GetAttachedSkeletalMesh();
	}

	return nullptr;
}


void UCustomizableSkeletalComponent::SetComponentName(const FName& Name)
{
	ComponentName = Name;
}


FName UCustomizableSkeletalComponent::GetComponentName() const
{
	if (!ComponentName.IsNone())
	{
		return ComponentName;	
	}
	else
	{
		return FName(FString::FromInt(ComponentIndex));
	}
}


UCustomizableObjectInstance* UCustomizableSkeletalComponent::GetCustomizableObjectInstance() const
{
	return CustomizableObjectInstance;
}


void UCustomizableSkeletalComponent::SetCustomizableObjectInstance(UCustomizableObjectInstance* Instance)
{
	CustomizableObjectInstance = Instance;
}


void UCustomizableSkeletalComponent::SetSkipSetReferenceSkeletalMesh(bool bSkip)
{
	bSkipSetReferenceSkeletalMesh = bSkip;
}


bool UCustomizableSkeletalComponent::GetSkipSetReferenceSkeletalMesh() const
{
	return bSkipSetReferenceSkeletalMesh;
}


void UCustomizableSkeletalComponent::SetSkipSetSkeletalMeshOnAttach(bool bSkip)
{
	bSkipSkipSetSkeletalMeshOnAttach = bSkip;
}


bool UCustomizableSkeletalComponent::GetSkipSetSkeletalMeshOnAttach() const
{
	return bSkipSkipSetSkeletalMeshOnAttach;
}


void UCustomizableSkeletalComponent::UpdateSkeletalMeshAsync(bool bNeverSkipUpdate)
{
	if (GetPrivate()->InstanceUsage)
	{
		GetPrivate()->InstanceUsage->UpdateSkeletalMeshAsync(bNeverSkipUpdate);
	}
}


void UCustomizableSkeletalComponent::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	if (GetPrivate()->InstanceUsage)
	{
		GetPrivate()->InstanceUsage->UpdateSkeletalMeshAsyncResult(Callback, bIgnoreCloseDist, bForceHighPriority);
	}
}


UCustomizableSkeletalComponentPrivate* UCustomizableSkeletalComponent::GetPrivate()
{
	check(Private);
	return Private;
}


const UCustomizableSkeletalComponentPrivate* UCustomizableSkeletalComponent::GetPrivate() const
{
	check(Private);
	return Private;
}


UCustomizableSkeletalComponent* UCustomizableSkeletalComponentPrivate::GetPublic()
{
	UCustomizableSkeletalComponent* Public = StaticCast<UCustomizableSkeletalComponent*>(GetOuter());
	check(Public);

	return Public;
}


const UCustomizableSkeletalComponent* UCustomizableSkeletalComponentPrivate::GetPublic() const
{
	UCustomizableSkeletalComponent* Public = StaticCast<UCustomizableSkeletalComponent*>(GetOuter());
	check(Public);

	return Public;	
}


void UCustomizableSkeletalComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();

	GetPrivate()->InstanceUsage->AttachTo(Cast<USkeletalMeshComponent>(GetAttachParent()));
}

