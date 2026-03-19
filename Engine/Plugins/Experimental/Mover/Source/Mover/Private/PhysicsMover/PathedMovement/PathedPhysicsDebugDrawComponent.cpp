// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/PathedPhysicsDebugDrawComponent.h"

#include "StaticMeshSceneProxy.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "PhysicsMover/PathedMovement/PathedMovementMode.h"
#include "PhysicsMover/PathedMovement/PathedMovementPatternBase.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsMoverComponent.h"

TAutoConsoleVariable<int32> CVarPathedPhysicsTotalDebugDrawSteps(
	TEXT("Mover.PathedPhysics.DebugDraw.TotalNumSteps"),
	32,
	TEXT("How many steps/lines to draw for each debug drawn path"));
	
TAutoConsoleVariable<int32> CVarPathedPhysicsDisplayedDebugDrawSteps(
	TEXT("Mover.PathedPhysics.DebugDraw.DisplayedSteps"),
	0,
	TEXT("Of the total number of steps in the path debug draw (see Mover.PathedPhysics.DebugDraw.TotalNumSteps), how many should actually get drawn? If <= 0, all steps are drawn."));

class FPathedPhysicsDebugRenderSceneProxy : public FDebugRenderSceneProxy
{
public:
	explicit FPathedPhysicsDebugRenderSceneProxy(const UPrimitiveComponent& InComponent)
		: FDebugRenderSceneProxy(&InComponent)
	{}

	virtual SIZE_T GetTypeHash() const override 
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = true; ///*bDrawDebug && *//*!IsSelected() &&*/ IsShown(View) /*&& View->Family->EngineShowFlags.Splines*/;
		Result.bDynamicRelevance = true;
		// Result.bShadowRelevance = IsShadowCast(View);
		Result.bEditorPrimitiveRelevance = true; // UseEditorCompositing(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return sizeof(*this) + GetAllocatedSize(); }
	
	//@todo DanH: Add a show flag for these
	// virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	// virtual uint32 GetMemoryFootprint() const override;
};

UPathedPhysicsDebugDrawComponent::UPathedPhysicsDebugDrawComponent()
{
	// bIsEditorOnly = true;
}

void UPathedPhysicsDebugDrawComponent::OnRegister()
{
	// DestroyProgressPreviewMeshComp();
	UpdatePreviewMeshComp();

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &ThisClass::HandleObjectPropertyChanged);
#endif

	Super::OnRegister();
}

void UPathedPhysicsDebugDrawComponent::OnUnregister()
{
	Super::OnUnregister();

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
#endif

	if (ProgressPreviewMeshComp)
	{
		ProgressPreviewMeshComp->UnregisterComponent();
	}
}

FBoxSphereBounds UPathedPhysicsDebugDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return Bounds;
}

void UPathedPhysicsDebugDrawComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	// Calculate everything for the scene proxy here so that our bounds are ready early enough (they get checked just before the scene proxy gets added)
	FBoxSphereBounds::Builder DebugBoundsBuilder;
	DebugBoundsBuilder += GetOwner()->GetTransform().GetLocation();
	
	DebugLines.Reset();
	DebugDashedLines.Reset();
	DebugArrowLines.Reset();
	DebugSpheres.Reset();
	DebugStars.Reset();

	const UPathedPhysicsMoverComponent& PathedMoverComp = *GetTypedOuter<UPathedPhysicsMoverComponent>();
	const UPathedPhysicsMovementMode* PathedMovementMode = PathedMoverComp.FindMode_Mutable<UPathedPhysicsMovementMode>(PathedMoverComp.StartingMovementMode);
	if (PathedMovementMode && !PathedMovementMode->PathPatterns.IsEmpty() && (PathedMovementMode->bDebugDrawAggregatePath || PathedMovementMode->bAllowPatternDebugDrawing))
	{
		int32 NumPatternsToDebugDraw = 0;
		TArray<const UPathedMovementPatternBase*> ValidPatterns;
		for (UPathedMovementPatternBase* PathPattern : PathedMovementMode->PathPatterns)
		{
			if (PathPattern)
			{
				ValidPatterns.Emplace(PathPattern);
				if (PathPattern->DebugDrawUsingStepSamples())
				{
					++NumPatternsToDebugDraw;
				}

				if (PathedMovementMode->bAllowPatternDebugDrawing)
				{
					PathPattern->AppendDebugDrawElements(*this, DebugBoundsBuilder);
				}
			}
		}

		if (NumPatternsToDebugDraw > 0 || PathedMovementMode->bDebugDrawAggregatePath)
		{
			const FTransform LocalToWorld = GetOwner()->GetTransform();
			FVector PreviousStepAggregateLocation = LocalToWorld.GetLocation();
			TArray<FVector> PreviousStepPatternLocations;

			int32 NumCurvesToDraw = PathedMovementMode->bDebugDrawAggregatePath ? 1 : 0;
			if (PathedMovementMode->bAllowPatternDebugDrawing)
			{
				PreviousStepPatternLocations.Init(PreviousStepAggregateLocation, ValidPatterns.Num());
				NumCurvesToDraw += NumPatternsToDebugDraw;
			}
			
			const float NumSteps = CVarPathedPhysicsTotalDebugDrawSteps.GetValueOnGameThread();
			DebugLines.Reserve(DebugLines.Num() + NumSteps * NumCurvesToDraw);
			const int32 NumStepsToDraw = CVarPathedPhysicsDisplayedDebugDrawSteps.GetValueOnGameThread() > 0 ? CVarPathedPhysicsDisplayedDebugDrawSteps.GetValueOnGameThread() : NumSteps;
			for (int32 Step = 1; Step <= NumStepsToDraw; ++Step)
			{
				FTransform TargetRelativeTransform = FTransform::Identity;
				const float ProgressAmt = Step / NumSteps;

				for (int32 PatternIdx = 0; PatternIdx < ValidPatterns.Num(); ++PatternIdx)
				{
					const UPathedMovementPatternBase& Pattern = *ValidPatterns[PatternIdx];
					const bool bDrawSampledPattern = Pattern.DebugDrawUsingStepSamples() && PathedMovementMode->bAllowPatternDebugDrawing;
					if (bDrawSampledPattern || PathedMovementMode->bDebugDrawAggregatePath)
					{
						// Effectively the same thing as UPathedPhysicsMovementMode::CalcTargetRelativeTransform, done manually to be able to draw each pattern as well
						const FTransform PatternRelativeTransform = Pattern.CalcTargetRelativeTransform(ProgressAmt, TargetRelativeTransform);
						TargetRelativeTransform.Accumulate(PatternRelativeTransform);

						if (bDrawSampledPattern)
						{
							const FVector PatternLocation = Chaos::FRigidTransform3::MultiplyNoScale(PatternRelativeTransform, LocalToWorld).GetLocation();
							DebugLines.Emplace(PreviousStepPatternLocations[PatternIdx], PatternLocation, Pattern.PatternDebugDrawColor, 1.f);

							PreviousStepPatternLocations[PatternIdx] = PatternLocation;
							DebugBoundsBuilder += PatternLocation;
						}
					}
				}

				// Aggregate Path
				if (PathedMovementMode->bDebugDrawAggregatePath)
				{
					const FVector StepLocation = Chaos::FRigidTransform3::MultiplyNoScale(TargetRelativeTransform, LocalToWorld).GetLocation();
					DebugLines.Emplace(PreviousStepAggregateLocation, StepLocation, PathedMovementMode->PathDebugDrawColor, 2.f);
					PreviousStepAggregateLocation = StepLocation;
					DebugBoundsBuilder += StepLocation;
				}
			}
		}	
	}

	Bounds = DebugBoundsBuilder;
	Bounds = Bounds.ExpandBy(25.);

	Super::CreateRenderState_Concurrent(Context);
}

FDebugRenderSceneProxy* UPathedPhysicsDebugDrawComponent::CreateDebugSceneProxy()
{
	if (DebugLines.IsEmpty() &&
		DebugDashedLines.IsEmpty() &&
		DebugArrowLines.IsEmpty() &&
		DebugSpheres.IsEmpty() &&
		DebugStars.IsEmpty())
	{
		return nullptr;
	}

	FPathedPhysicsDebugRenderSceneProxy* DebugSceneProxy = new FPathedPhysicsDebugRenderSceneProxy(*this);
    DebugSceneProxy->DrawType = FDebugRenderSceneProxy::SolidAndWireMeshes;
    // SceneProxy->ViewFlagName = FString();
    // SceneProxy->ViewFlagIndex = 0;
    // SceneProxy->ViewFlagName = TEXT("PathedPhysics");
    DebugSceneProxy->ViewFlagName = TEXT("Splines");
    DebugSceneProxy->ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*DebugSceneProxy->ViewFlagName));
	
	DebugSceneProxy->Lines = DebugLines;
	DebugSceneProxy->DashedLines = DebugDashedLines;
	DebugSceneProxy->ArrowLines = DebugArrowLines;
	DebugSceneProxy->Spheres = DebugSpheres;
	DebugSceneProxy->Stars = DebugStars;
	
	return DebugSceneProxy;
}

void UPathedPhysicsDebugDrawComponent::UpdatePreviewMeshComp(bool bForce)
{
	UMeshComponent* MeshRoot = GetOwnerMeshRoot();
	if (MeshRoot && GetWorld() && !GetWorld()->IsGameWorld())
	{
		bool bDoUpdate = false;
		
		const UPathedPhysicsMoverComponent& PathedMoverComp = *GetTypedOuter<UPathedPhysicsMoverComponent>();
    	if (ProgressPreviewMeshComp)
    	{
		    if (ProgressPreviewMeshComp->GetClass() == MeshRoot->GetClass() && bForce)
		    {
		    	bDoUpdate = true;

		    	// Copy all the properties on the root mesh comp (except attachments)
		    	for (TFieldIterator<FProperty> PropIter(MeshRoot->GetClass()); PropIter; ++PropIter)
		    	{
				    if (PropIter->GetFName() == TEXT("AttachChildren") || PropIter->GetFName() == TEXT("AttachParent"))
				    {
					    continue;
				    }
		    		PropIter->CopyCompleteValue_InContainer(ProgressPreviewMeshComp, MeshRoot);
		    	}
		    }
		    else
		    {
			    DestroyProgressPreviewMeshComp();
		    }
    	}

		if (!ProgressPreviewMeshComp)
    	{
    		ProgressPreviewMeshComp = DuplicateObject<UMeshComponent>(MeshRoot, this, TEXT("ProgressPreviewMeshComp"));
    		// ProgressPreviewMeshComp->SetupAttachment(this);
			ProgressPreviewMeshComp->bAutoRegister = false;

			bDoUpdate = true;
    	}

		ensure(ProgressPreviewMeshComp->GetNumChildrenComponents() == 0);

		if (!ProgressPreviewMeshComp->IsRegistered())
		{
			ProgressPreviewMeshComp->RegisterComponent();
		}

		if (bDoUpdate)
		{
			//@todo DanH: Double-clicking the preview comp in the level editor can still select the component - how do I disable that?
			ProgressPreviewMeshComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			ProgressPreviewMeshComp->SetSimulatePhysics(false);
			ProgressPreviewMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ProgressPreviewMeshComp->bIsEditorOnly = true;
			ProgressPreviewMeshComp->SetHiddenInGame(true);

			const UPathedPhysicsMovementMode* PathedMovementMode = PathedMoverComp.FindMode_Mutable<UPathedPhysicsMovementMode>(PathedMoverComp.StartingMovementMode);
			if (PathedMovementMode && PathedMoverComp.bDisplayProgressPreviewMesh)
			{
				ProgressPreviewMeshComp->SetVisibility(true);
			
				const FTransform PreviewMeshRelativeTransform = PathedMovementMode->CalcTargetRelativeTransform(PathedMoverComp.PreviewMeshProgress);

				// Calculate the location without scale, then tack on the source mesh's scale
				FTransform PreviewMeshWorldTransform = Chaos::FRigidTransform3::MultiplyNoScale(PreviewMeshRelativeTransform, MeshRoot->GetComponentTransform());
				PreviewMeshWorldTransform.SetScale3D(MeshRoot->GetComponentScale());
				ProgressPreviewMeshComp->SetWorldTransform(PreviewMeshWorldTransform);

				if (PathedMoverComp.ProgressPreviewMeshMaterial)
				{
					const int32 NumMaterials = ProgressPreviewMeshComp->GetNumMaterials();
					for (int MaterialIdx = 0; MaterialIdx < NumMaterials; ++MaterialIdx)
					{
						ProgressPreviewMeshComp->SetMaterial(MaterialIdx, PathedMoverComp.ProgressPreviewMeshMaterial);
					}
				}
			}
			else
			{
				ProgressPreviewMeshComp->SetVisibility(false);
			}
		}
	}
	else if (ProgressPreviewMeshComp)
	{
		DestroyProgressPreviewMeshComp();
	}
}

void UPathedPhysicsDebugDrawComponent::DestroyProgressPreviewMeshComp()
{
	if (ProgressPreviewMeshComp)
	{
		ProgressPreviewMeshComp->DestroyComponent();
		ProgressPreviewMeshComp = nullptr;
	}
}

UMeshComponent* UPathedPhysicsDebugDrawComponent::GetOwnerMeshRoot() const
{
	if (AActor* Owner = GetOwner())
	{
		return Cast<UMeshComponent>(Owner->GetRootComponent());
	}
	return nullptr;
}

#if WITH_EDITOR
void UPathedPhysicsDebugDrawComponent::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(Object);
		MeshComponent && MeshComponent == GetOwner()->GetRootComponent())
	{
		UpdatePreviewMeshComp();		
	}
}
#endif
