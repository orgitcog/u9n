// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassStationaryISMRepresentationFragmentDestructor.h"
#include "MassRepresentationSubsystem.h"
#include "MassCommonFragments.h"
#include "MassRepresentationProcessor.h"

//-----------------------------------------------------------------------------
// UMassStationaryISMRepresentationFragmentDestructor
//-----------------------------------------------------------------------------
UMassStationaryISMRepresentationFragmentDestructor::UMassStationaryISMRepresentationFragmentDestructor()
	: EntityQuery(*this)
{
	ObservedType = FMassRepresentationFragment::StaticStruct();
	Operation = EMassObservedOperation::Remove;
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllWorldModes;
	bRequiresGameThreadExecution = true; // not sure about this
}

void UMassStationaryISMRepresentationFragmentDestructor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FMassRepresentationParameters>();
	EntityQuery.AddSharedRequirement<FMassRepresentationSubsystemSharedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FMassStaticRepresentationTag>(EMassFragmentPresence::All);
}

void UMassStationaryISMRepresentationFragmentDestructor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context)
	{
		UMassRepresentationSubsystem* RepresentationSubsystem = Context.GetMutableSharedFragment<FMassRepresentationSubsystemSharedFragment>().RepresentationSubsystem;
		check(RepresentationSubsystem);
		FMassInstancedStaticMeshInfoArrayView ISMInfosView = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();

		const TArrayView<FMassRepresentationFragment> RepresentationList = Context.GetMutableFragmentView<FMassRepresentationFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassRepresentationFragment& Representation = RepresentationList[EntityIt];
			if (Representation.CurrentRepresentation == EMassRepresentationType::StaticMeshInstance)
			{
				FMassInstancedStaticMeshInfo& ISMInfo = ISMInfosView[Representation.StaticMeshDescHandle.ToIndex()];
				if (FMassLODSignificanceRange* OldRange = ISMInfo.GetLODSignificanceRange(Representation.PrevLODSignificance))
				{
					const FMassEntityHandle EntityHandle = Context.GetEntity(EntityIt);
					if (OldRange)
					{
						OldRange->RemoveInstance(EntityHandle);
					}
				}
				Representation.CurrentRepresentation = EMassRepresentationType::None;
			}
		}
	});
}
