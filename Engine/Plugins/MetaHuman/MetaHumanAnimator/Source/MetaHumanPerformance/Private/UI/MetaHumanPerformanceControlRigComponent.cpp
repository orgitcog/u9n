// Copyright Epic Games, Inc.All Rights Reserved.

#include "MetaHumanPerformanceControlRigComponent.h"

#include "ControlRig.h"
#include "ControlRigGizmoActor.h"

const FName UMetaHumanPerformanceControlRigComponent::HeadIKControlName = TEXT("mha_head_ik_ctrl");
const FName UMetaHumanPerformanceControlRigComponent::HeadIKSwitchControlName = TEXT("mha_head_ik_switch_ctrl");

void UMetaHumanPerformanceControlRigComponent::SetControlRig(UControlRig* InControlRig)
{
	ControlRig = InControlRig;

	if (IsRegistered())
	{
		DestroyControlRigShapes();
		SpawnControlRigShapes();
	}
}

void UMetaHumanPerformanceControlRigComponent::UpdateControlRigShapes()
{
	if (ControlRig != nullptr)
	{
		FTransform ParentActorTransform = FTransform::Identity;
		if (AActor* ParentActor = GetAttachParentActor())
		{
			ParentActorTransform = ParentActor->GetActorTransform();
		}

		FTransform ParentComponentTransform = FTransform::Identity;
		if (USceneComponent* ParentComponent = GetAttachParent())
		{
			ParentComponentTransform = ParentComponent->GetComponentTransform();
		}

		const bool bIsComponentVisible = IsVisible();

		for (AControlRigShapeActor* ShapeActor : ShapeActors)
		{
			const FTransform Transform = ControlRig->GetControlGlobalTransform(ShapeActor->ControlName);
			ShapeActor->SetActorTransform(Transform * ParentComponentTransform * ParentActorTransform);

			if (FRigControlElement* ControlElement = ControlRig->FindControl(ShapeActor->ControlName))
			{
				const bool bIsVisible = ControlElement->Settings.IsVisible() && bIsComponentVisible;
				ShapeActor->SetShapeColor(ShapeActor->OverrideColor.A < SMALL_NUMBER ? ControlElement->Settings.ShapeColor : ShapeActor->OverrideColor);
				ShapeActor->SetIsTemporarilyHiddenInEditor(!bIsVisible);
			}
		}
	}
}

void UMetaHumanPerformanceControlRigComponent::DestroyControlRigShapes()
{
	for (AControlRigShapeActor* ShapeActor : ShapeActors)
	{
		ShapeActor->Destroy();
	}
	ShapeActors.Empty();
}

FBox UMetaHumanPerformanceControlRigComponent::GetShapesBoundingBox() const
{
	FBox ControlRigShapesBoundingBox{ ForceInit };

	for (AControlRigShapeActor* ShapeActor : ShapeActors)
	{
		constexpr bool bNonColliding = true;
		constexpr bool bIncludeChildren = true;
		if (!ShapeActor->IsHiddenEd())
		{
			ControlRigShapesBoundingBox += ShapeActor->GetComponentsBoundingBox(bNonColliding, bIncludeChildren);
		}
	}

	return ControlRigShapesBoundingBox;
}

void UMetaHumanPerformanceControlRigComponent::OnRegister()
{
	Super::OnRegister();

	if (ControlRig != nullptr)
	{
		SpawnControlRigShapes();
	}
}

void UMetaHumanPerformanceControlRigComponent::OnUnregister()
{
	Super::OnUnregister();

	DestroyControlRigShapes();
}

void UMetaHumanPerformanceControlRigComponent::OnComponentDestroyed(bool bInDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bInDestroyingHierarchy);

	DestroyControlRigShapes();
}

void UMetaHumanPerformanceControlRigComponent::OnVisibilityChanged()
{
	Super::OnVisibilityChanged();

	UpdateControlRigShapes();
}

void UMetaHumanPerformanceControlRigComponent::SpawnControlRigShapes()
{
	if (ControlRig != nullptr)
	{
		// Start by destroying existing shapes so we can create new ones
		DestroyControlRigShapes();

		const TArray<FRigControlElement*> Controls = ControlRig->AvailableControls();
		const TArray<TSoftObjectPtr<UControlRigShapeLibrary>> ShapeLibraries = ControlRig->GetShapeLibraries();
		for (FRigControlElement* ControlElement : Controls)
		{
			if (!ControlElement->Settings.SupportsShape())
			{
				continue;
			}

			// Suppress the display of the Head IK Controls in Performance
			if (ControlElement->GetName() == HeadIKControlName || ControlElement->GetName() == HeadIKSwitchControlName)
			{
				continue;
			}

			FControlShapeActorCreationParam Param;
			Param.ManipObj = ControlRig;
			Param.ControlRigIndex = 0;
			Param.ControlRig = ControlRig;
			Param.ControlName = ControlElement->GetFName();
			Param.ShapeName = ControlElement->Settings.ShapeName;
			Param.SpawnTransform = ControlRig->GetControlGlobalTransform(ControlElement->GetFName());
			Param.ShapeTransform = ControlRig->GetHierarchy()->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
			constexpr bool bRespectVisibility = false;
			Param.bSelectable = ControlElement->Settings.IsSelectable(bRespectVisibility);

			if (const FControlRigShapeDefinition* ShapeDef = UControlRigShapeLibrary::GetShapeByName(ControlElement->Settings.ShapeName, ShapeLibraries, {}))
			{
				Param.MeshTransform = ShapeDef->Transform;
				Param.StaticMesh = ShapeDef->StaticMesh;
				Param.Material = ShapeDef->Library->DefaultMaterial;
				Param.ColorParameterName = ShapeDef->Library->MaterialColorParameter;
			}

			Param.Color = ControlElement->Settings.ShapeColor;

			if (AControlRigShapeActor* ShapeActor = FControlRigShapeHelper::CreateDefaultShapeActor(GetWorld(), Param))
			{
				ShapeActors.Add(ShapeActor);
			}
		}

		if (!ShapeActors.IsEmpty())
		{
			// This will set the visibility of the shapes so we can query which ones are visible to focus the viewport on them
			UpdateControlRigShapes();
		}
	}
}