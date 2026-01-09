// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_GetActorTransform.h"

#include "Component/AnimNextComponent.h"
#include "Module/AnimNextModuleInstance.h"

void FAnimNextActorTransformComponent::OnInitialize()
{
	FAnimNextModuleInstance& ModuleInstance = GetModuleInstance();
	UAnimNextComponent* AnimNextComponent = Cast<UAnimNextComponent>(ModuleInstance.GetObject());

	FAnimNextModuleInstance::RunTaskOnGameThread(
		[WeakAnimNextComponent = TWeakObjectPtr<UAnimNextComponent>(AnimNextComponent)]()
		{
			UAnimNextComponent* Component = WeakAnimNextComponent.Get();
			if(Component == nullptr)
			{
				return;
			}

			AActor* Owner = Component->GetOwner();
			if(Owner == nullptr)
			{
				return;
			}

			USceneComponent* SceneComponent = Owner->GetRootComponent();
			if(SceneComponent == nullptr)
			{
				return;
			}

			SceneComponent->TransformUpdated.AddWeakLambda(Component, [Component](USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
			{
				AActor* UpdatedComponentOwner = UpdatedComponent->GetOwner();
				if(UpdatedComponentOwner == nullptr)
				{
					return;
				}

				Component->QueueTask(NAME_None, [Transform = UpdatedComponentOwner->GetActorTransform()](const UE::AnimNext::FModuleTaskContext& InContext)
				{
					InContext.TryAccessComponent<FAnimNextActorTransformComponent>([&Transform](FAnimNextActorTransformComponent& InComponent)
					{
						InComponent.ActorTransform = Transform;
					});
				});
			});
		});
}

FRigUnit_GetActorTransform_Execute()
{
	using namespace UE::AnimNext;

	const FAnimNextModuleContextData& ModuleContextData = ExecuteContext.GetContextData<FAnimNextModuleContextData>();
	FAnimNextModuleInstance& ModuleInstance = ModuleContextData.GetModuleInstance();
	const FAnimNextActorTransformComponent& TransformComponent = ModuleInstance.GetComponent<FAnimNextActorTransformComponent>();
	Transform = TransformComponent.GetActorTransform();
}