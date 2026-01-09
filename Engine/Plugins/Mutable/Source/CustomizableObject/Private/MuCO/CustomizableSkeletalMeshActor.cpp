// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableSkeletalMeshActor.h"
#include "MuCO/CustomizableSkeletalMeshActorPrivate.h"

#include "Components/SkeletalMeshComponent.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableSkeletalComponentPrivate.h"
#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/UnrealPortabilityHelpers.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableSkeletalMeshActor)

#define LOCTEXT_NAMESPACE "CustomizableObject"


UCustomizableObjectInstance* ACustomizableSkeletalMeshActor::GetCustomizableObjectInstance()
{
	for (const UCustomizableSkeletalComponent* Component : CustomizableSkeletalComponents)
	{
		if (UCustomizableObjectInstance* COInstance = Component->CustomizableObjectInstance)
		{
			return COInstance;
		}
	}

	return nullptr;
}


ACustomizableSkeletalMeshActor::ACustomizableSkeletalMeshActor(const FObjectInitializer& Initializer)
{
	Private = CreateDefaultSubobject<UCustomizableSkeletalMeshActorPrivate>(TEXT("Private"));

	// Old assets used to create the first UCustomizableSkeletalComponent as a Subobject. To be able to deserialize them we need to create it.
	// Creating instead an Object instead of a Suobject will not work. Only Component UCustomizableSkeletalComponent 0 is a Subobject.
	{
		UCustomizableSkeletalComponent* CustomizableSkeletalComponent = CreateDefaultSubobject<UCustomizableSkeletalComponent>(TEXT("CustomizableSkeletalComponent0"));
		CustomizableSkeletalComponents.Add(CustomizableSkeletalComponent);
		if (USkeletalMeshComponent* SkeletalMeshComp = Super::GetSkeletalMeshComponent()) 
		{
			SkeletalMeshComponents.Add(SkeletalMeshComp);
			CustomizableSkeletalComponents[0]->AttachToComponent(SkeletalMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}


USkeletalMeshComponent* ACustomizableSkeletalMeshActor::GetSkeletalMeshComponent(const FName& ComponentName)
{
	for (int32 ComponentIndex = 0; ComponentIndex < CustomizableSkeletalComponents.Num(); ComponentIndex++)
	{
		const UCustomizableSkeletalComponent* Component = CustomizableSkeletalComponents[ComponentIndex];
		
		if (Component->GetComponentName() == ComponentName)
		{
			return SkeletalMeshComponents[ComponentIndex];
		}
	}

	return nullptr; 
}


void UCustomizableSkeletalMeshActorPrivate::Init(UCustomizableObjectInstance* Instance)
{
	UCustomizableObject* Object = Instance->GetCustomizableObject();
	if (!Object)
	{
		return;
	}
	
	for (int32 ObjectComponentIndex = 0; ObjectComponentIndex < Object->GetComponentCount(); ++ObjectComponentIndex)
	{
		const FName& ComponentName = Object->GetComponentName(ObjectComponentIndex);
		const FString ComponentNameString = ComponentName.ToString();

		USkeletalMeshComponent* Component;
		if (ObjectComponentIndex == 0)
		{
			Component = GetPublic()->SkeletalMeshComponents[0];
			// Renaming the component will make it disappear. Keep it as it is.
		}
		else
		{
			const FString SkeletalMeshComponentName = FString::Printf(TEXT("SkeletalMeshComponent %s"), *ComponentNameString);
			Component = NewObject<USkeletalMeshComponent>(GetPublic(), USkeletalMeshComponent::StaticClass(), FName(*SkeletalMeshComponentName));
			Component->CreationMethod = EComponentCreationMethod::Native;
			Component->AttachToComponent(GetPublic()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			Component->RegisterComponent();
		}
					
		if (!Component)
		{
			continue;
		}

		UCustomizableSkeletalComponent* CustomizableComponent;
		if (ObjectComponentIndex == 0)
		{
			CustomizableComponent = GetPublic()->CustomizableSkeletalComponents[0];
			// Renaming the component will make it disappear. Keep it as it is.
		}
		else
		{
			const FString CustomizableComponentName = FString::Printf(TEXT("CustomizableSkeletalComponent %s"), *ComponentNameString);
			CustomizableComponent = NewObject<UCustomizableSkeletalComponent>(GetPublic(), UCustomizableSkeletalComponent::StaticClass(), FName(CustomizableComponentName));
		}

		CustomizableComponent->AttachToComponent(Component, FAttachmentTransformRules::KeepRelativeTransform);
		CustomizableComponent->CustomizableObjectInstance = Instance;
		CustomizableComponent->SetComponentName(ComponentName);
		CustomizableComponent->RegisterComponent();
		
		GetPublic()->SkeletalMeshComponents.Add(Component);
		GetPublic()->CustomizableSkeletalComponents.Add(CustomizableComponent);
	}

	Instance->UpdateSkeletalMeshAsync();
}


void ACustomizableSkeletalMeshActor::SetDebugMaterial(UMaterialInterface* InDebugMaterial)
{
	if (!InDebugMaterial)
	{
		return;
	}

	DebugMaterial = InDebugMaterial;
}


void ACustomizableSkeletalMeshActor::EnableDebugMaterial(bool bEnableDebugMaterial)
{
	bRemoveDebugMaterial = bDebugMaterialEnabled && !bEnableDebugMaterial;
	bDebugMaterialEnabled = bEnableDebugMaterial;

	if (UCustomizableObjectInstance* COInstance = GetCustomizableObjectInstance())
	{
		//Bind Instance Update delegate to Actor
		COInstance->UpdatedDelegate.AddUniqueDynamic(this, &ACustomizableSkeletalMeshActor::SwitchComponentsMaterials);
		SwitchComponentsMaterials(COInstance);
	}
}


UCustomizableSkeletalMeshActorPrivate* ACustomizableSkeletalMeshActor::GetPrivate()
{
	check(Private);
	return Private;
}


const UCustomizableSkeletalMeshActorPrivate* ACustomizableSkeletalMeshActor::GetPrivate() const
{
	check(Private);
	return Private;
}


void ACustomizableSkeletalMeshActor::SwitchComponentsMaterials(UCustomizableObjectInstance* Instance)
{
	if (!DebugMaterial)
	{
		return;
	}

	if (bDebugMaterialEnabled || bRemoveDebugMaterial)
	{
		UCustomizableObjectInstance* COInstance = GetCustomizableObjectInstance();

		if (!COInstance)
		{
			return;
		}

		for (int32 ObjectComponentIndex = 0; ObjectComponentIndex < SkeletalMeshComponents.Num(); ++ObjectComponentIndex)
		{
			int32 NumMaterials = SkeletalMeshComponents[ObjectComponentIndex]->GetNumMaterials();

			if (bDebugMaterialEnabled)
			{
				for (int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex)
				{
					SkeletalMeshComponents[ObjectComponentIndex]->SetMaterial(MatIndex, DebugMaterial);
				}
			}
			else // Remove debugmaterial
			{
				const FName ComponentName = CustomizableSkeletalComponents[ObjectComponentIndex]->GetComponentName();
				
				// check if original materials already overriden
				const TArray<UMaterialInterface*> OverrideMaterials = COInstance->GetSkeletalMeshComponentOverrideMaterials(ComponentName);
				const bool bUseOverrideMaterials = COInstance->GetCustomizableObject()->bEnableMeshCache && CVarEnableMeshCache.GetValueOnAnyThread();

				if (bUseOverrideMaterials && OverrideMaterials.Num() > 0)
				{
					for (int32 MatIndex = 0; MatIndex < OverrideMaterials.Num(); ++MatIndex)
					{
						SkeletalMeshComponents[ObjectComponentIndex]->SetMaterial(MatIndex, OverrideMaterials[MatIndex]);
					}
				}
				else
				{
					SkeletalMeshComponents[ObjectComponentIndex]->EmptyOverrideMaterials();
				}

				bRemoveDebugMaterial = false;
			}
		}
	}
}

ACustomizableSkeletalMeshActor* UCustomizableSkeletalMeshActorPrivate::GetPublic()
{
	return CastChecked<ACustomizableSkeletalMeshActor>(GetOuter());
}


TArray<TObjectPtr<UCustomizableSkeletalComponent>>& UCustomizableSkeletalMeshActorPrivate::GetComponents()
{
	return GetPublic()->CustomizableSkeletalComponents;
}


#undef LOCTEXT_NAMESPACE
