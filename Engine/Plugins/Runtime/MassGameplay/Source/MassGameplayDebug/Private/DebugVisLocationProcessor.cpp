// Copyright Epic Games, Inc. All Rights Reserved.

#include "DebugVisLocationProcessor.h"
#include "MassDebuggerSubsystem.h"
#include "MassDebugVisualizationComponent.h"
#include "MassCommonFragments.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassDebugDrawHelpers.h"


namespace UE::Mass::Debug
{
	bool bDebugDrawAllEntities = false;

	namespace
	{
		FAutoConsoleVariableRef AnonymousCVars[] = {
			{TEXT("mass.debug.DrawAllEntities"), bDebugDrawAllEntities
				, TEXT("When enabled will debug-draw debug shapes marking all entities that have a TransformFragment")
				, ECVF_Cheat},
		};
	}
}

//----------------------------------------------------------------------//
// UDebugVisLocationProcessor
//----------------------------------------------------------------------//
UDebugVisLocationProcessor::UDebugVisLocationProcessor()
	: EntityQuery(*this)
	, AllLocationEntitiesQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SyncWorldToMass);
	bRequiresGameThreadExecution = true; // due to UMassDebuggerSubsystem access
	QueryBasedPruning = EMassQueryBasedPruning::Never;
}

void UDebugVisLocationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FSimDebugVisFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassDebuggableTag>(EMassFragmentPresence::All);
	EntityQuery.AddSubsystemRequirement<UMassDebuggerSubsystem>(EMassFragmentAccess::ReadWrite);

	AllLocationEntitiesQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UDebugVisLocationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if WITH_EDITORONLY_DATA
	QUICK_SCOPE_CYCLE_COUNTER(DebugVisLocationProcessor_Run);

	if (UE::Mass::Debug::bDebugDrawAllEntities == false)
	{
		EntityQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context)
			{
				UMassDebuggerSubsystem& Debugger = Context.GetMutableSubsystemChecked<UMassDebuggerSubsystem>();
				UMassDebugVisualizationComponent* Visualizer = Debugger.GetVisualizationComponent();
				check(Visualizer);
				TArrayView<UInstancedStaticMeshComponent* const> VisualDataISMCs = Visualizer->GetVisualDataISMCs();
				if (VisualDataISMCs.Num() > 0)
				{
					const int32 NumEntities = Context.GetNumEntities();
					const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
					const TConstArrayView<FSimDebugVisFragment> DebugVisList = Context.GetFragmentView<FSimDebugVisFragment>();

					for (int32 i = 0; i < NumEntities; ++i)
					{
						const FSimDebugVisFragment& VisualComp = DebugVisList[i];

						// @todo: remove this code once the asset is exported with correct alignment SM_Mannequin.uasset
						FTransform SMTransform = LocationList[i].GetTransform();
						FQuat FromEngineToSM(FVector::UpVector, -HALF_PI);
						SMTransform.SetRotation(FromEngineToSM * SMTransform.GetRotation());

						VisualDataISMCs[VisualComp.VisualType]->UpdateInstanceTransform(VisualComp.InstanceIndex, SMTransform, true);
					}
				}
				else
				{
					UE_LOG(LogMassDebug, Log, TEXT("UDebugVisLocationProcessor: Trying to update InstanceStaticMeshes while none created. Check your debug visualization setup"));
				}
			});

		UMassDebuggerSubsystem* Debugger = UWorld::GetSubsystem<UMassDebuggerSubsystem>(EntityManager.GetWorld());
		if (ensure(Debugger))
		{
			Debugger->GetVisualizationComponent()->DirtyVisuals();
		}
	}
#endif // WITH_EDITORONLY_DATA
	if (UE::Mass::Debug::bDebugDrawAllEntities)
	{
		if (UWorld* World = GetWorld())
		{
			UE::Mass::Debug::FLineBatcher LineBatcher = UE::Mass::Debug::FLineBatcher::MakeLineBatcher(World);

			AllLocationEntitiesQuery.ForEachEntityChunk(Context, [&LineBatcher](const FMassExecutionContext& Context)
				{
					const FColor ArchetypeColor =
#if WITH_MASSENTITY_DEBUG
						Context.DebugGetArchetypeColor();
#else
						FColor::Green;
#endif // WITH_MASSENTITY_DEBUG

					const FVector DebugDrawBoxExtent(20.);
					constexpr float ArrowLength = 30.f;

					const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();

					for (const FTransformFragment& LocationFragment : LocationList)
					{
						LineBatcher.DrawSolidBox(LocationFragment.GetTransform().GetLocation(), DebugDrawBoxExtent, ArchetypeColor);
						if (LocationFragment.GetTransform().GetRotation().IsIdentity() == false)
						{
							
							LineBatcher.DrawArrow(LocationFragment.GetTransform(), ArrowLength, ArchetypeColor);
						}
					}
				});
		}
	}
}

//----------------------------------------------------------------------//
//  UMassProcessor_UpdateDebugVis
//----------------------------------------------------------------------//
UMassProcessor_UpdateDebugVis::UMassProcessor_UpdateDebugVis()
	: EntityQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
	bRequiresGameThreadExecution = true; // due to UMassDebuggerSubsystem access
}

void UMassProcessor_UpdateDebugVis::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) 
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FDataFragment_DebugVis>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassDebuggableTag>(EMassFragmentPresence::All);

	ProcessorRequirements.AddSubsystemRequirement<UMassDebuggerSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UMassProcessor_UpdateDebugVis::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassProcessor_UpdateDebugVis_Run);

	UMassDebuggerSubsystem& Debugger = Context.GetMutableSubsystemChecked<UMassDebuggerSubsystem>();
	Debugger.ResetDebugShapes();

	EntityQuery.ForEachEntityChunk(Context, [&Debugger](FMassExecutionContext& Context)
		{
			const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
			const TArrayView<FDataFragment_DebugVis> DebugVisList = Context.GetMutableFragmentView<FDataFragment_DebugVis>();
			const TArrayView<FAgentRadiusFragment> RadiiList = Context.GetMutableFragmentView<FAgentRadiusFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				Debugger.AddShape(DebugVisList[EntityIt].Shape, LocationList[EntityIt].GetTransform().GetLocation(), RadiiList[EntityIt].Radius);
			}
		});
}
