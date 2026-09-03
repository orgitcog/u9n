// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshComputeFiberFieldNode.h"

#include "Chaos/Math/Poisson.h"
#include "ChaosFlesh/ChaosFlesh.h"
#include "ChaosFlesh/TetrahedralCollection.h"
#include "GeometryCollection/Facades/CollectionMeshFacade.h"
#include "GeometryCollection/Facades/CollectionMuscleActivationFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshComputeFiberFieldNode)

void FComputeFiberFieldNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		//
		// Gather inputs
		//
		if (!IsConnected(&Collection) || !IsConnected(&OriginIndices) || !IsConnected(&InsertionIndices))
		{
			return;
		}
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> InOriginIndices = GetValue<TArray<int32>>(Context, &OriginIndices);
		TArray<int32> InInsertionIndices = GetValue<TArray<int32>>(Context, &InsertionIndices);

		// Tetrahedra
		TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(
			FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
		if (!Elements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::TetrahedronAttribute.ToString(), *FTetrahedralCollection::TetrahedralGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Vertices
		TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices");
		if (!Vertex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr 'Vertex' in group 'Vertices'"));
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Incident elements
		TManagedArray<TArray<int32>>* IncidentElements = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}
		TManagedArray<TArray<int32>>* IncidentElementsLocalIndex = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsLocalIndexAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElementsLocalIndex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsLocalIndexAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		//
		// Pull Origin & Insertion data out of the geometry collection.  We may want other ways of specifying
		// these via an input on the node...
		//

		// Origin & Insertion
		TManagedArray<int32>* Origin = nullptr; 
		TManagedArray<int32>* Insertion = nullptr;
		if (InOriginIndices.IsEmpty() || InInsertionIndices.IsEmpty())
		{
			// Origin & Insertion group
			if (OriginInsertionGroupName.IsEmpty())
			{
				UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'OriginInsertionGroupName' cannot be empty."));
				Out->SetValue(MoveTemp(InCollection), Context);
				return;
			}

			// Origin vertices
			if (InOriginIndices.IsEmpty())
			{
				if (OriginVertexFieldName.IsEmpty())
				{
					UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'OriginVertexFieldName' cannot be empty."));
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
				Origin = InCollection.FindAttribute<int32>(FName(OriginVertexFieldName), FName(OriginInsertionGroupName));
				if (!Origin)
				{
					UE_LOG(LogChaosFlesh, Warning,
						TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
						*OriginVertexFieldName, *OriginInsertionGroupName);
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
				else
				{
					InOriginIndices = Origin->GetConstArray();
				}
			}

			// Insertion vertices
			if (InInsertionIndices.IsEmpty())
			{
				if (InsertionVertexFieldName.IsEmpty())
				{
					UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'InsertionVertexFieldName' cannot be empty."));
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
				Insertion = InCollection.FindAttribute<int32>(FName(InsertionVertexFieldName), FName(OriginInsertionGroupName));
				if (!Insertion)
				{
					UE_LOG(LogChaosFlesh, Warning,
						TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
						*InsertionVertexFieldName, *OriginInsertionGroupName);
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
				else
				{
					InInsertionIndices = Insertion->GetConstArray();
				}
			}
		}

		// Only solve for fiber field on muscle geometries
		TSet<int32> MuscleGeometries;
		GeometryCollection::Facades::FCollectionMeshFacade MeshFacade(InCollection);
		TArray<int32> GeometryIndex = MeshFacade.GetGeometryGroupIndexArray();
		for (int32 OriginIdx : InOriginIndices)
		{
			// Verify origins
			if (!Vertex->IsValidIndex(OriginIdx))
			{
				Context.Error(FString::Printf(
					TEXT("OriginIdx %d is not a valid vertex index for vertex group size %d."),
					OriginIdx, Vertex->Num()),
					this);
				return;
			}
			if (GeometryIndex.IsValidIndex(OriginIdx))
			{
				MuscleGeometries.Add(GeometryIndex[OriginIdx]);
			}
		}
		for (int32 InsertionIdx : InInsertionIndices)
		{
			// Verify insertions
			if (!Vertex->IsValidIndex(InsertionIdx))
			{
				Context.Error(FString::Printf(
					TEXT("OriginIdx %d is not a valid vertex index for vertex group size %d."),
					InsertionIdx, Vertex->Num()),
					this);
				return;
			}
			if (GeometryIndex.IsValidIndex(InsertionIdx))
			{
				MuscleGeometries.Add(GeometryIndex[InsertionIdx]);
			}
		}
		TArray<int32> MuscleElementIndices;
		TArray<FIntVector4> MuscleElements;

		TManagedArray<int32>* TetrahedronStart = InCollection.FindAttribute<int32>(
			FTetrahedralCollection::TetrahedronStartAttribute, FTetrahedralCollection::GeometryGroup);
		TManagedArray<int32>* TetrahedronCount = InCollection.FindAttribute<int32>(
			FTetrahedralCollection::TetrahedronCountAttribute, FTetrahedralCollection::GeometryGroup);
		TArray<TArray<int32>> MuscleConstraints;
		if (TetrahedronStart && TetrahedronCount)
		{
			for (int32 GeometryIdx : MuscleGeometries)
			{
				if (TetrahedronStart->IsValidIndex(GeometryIdx))
				{
					for (int32 ElemIdx = (*TetrahedronStart)[GeometryIdx]; ElemIdx < (*TetrahedronStart)[GeometryIdx] + (*TetrahedronCount)[GeometryIdx]; ++ElemIdx)
					{
						MuscleElementIndices.Add(ElemIdx);
						MuscleElements.Add((*Elements)[ElemIdx]);
					}
				}
			}
		}
		MuscleConstraints.SetNum(MuscleElements.Num());
		for (int32 LocalIdx = 0; LocalIdx < MuscleElements.Num(); ++LocalIdx)
		{
			for (int32 c = 0; c < 4; ++c)
			{
				MuscleConstraints[LocalIdx].Add(MuscleElements[LocalIdx][c]);
			}
		}
		TArray<TArray<int32>> MuscleIncidentElementsLocalIndex;
		TArray<TArray<int32>> MuscleIncidentElements = Chaos::Utilities::ComputeIncidentElements(MuscleConstraints, &MuscleIncidentElementsLocalIndex);

		//
		// Do the thing
		//
		TArray<FVector3f> MuscleFiberDirs;
		TArray<float> MuscleAttachmentScalarFieldTArray; //continuous field where origin = 1, insertion = 2, othernodes = 0
		Chaos::ComputeFiberField<float>(
			MuscleElements,
			Vertex->GetConstArray(),
			MuscleIncidentElements,
			MuscleIncidentElementsLocalIndex,
			InOriginIndices,
			InInsertionIndices,
			MuscleFiberDirs,
			MuscleAttachmentScalarFieldTArray,
			MaxIterations,
			Tolerance);

		//
		// Set output(s)
		//

		TManagedArray<FVector3f>& FiberDirections =
			InCollection.AddAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup);
		FiberDirections.Fill(FVector3f(0, 0, 0));
		for (int32 LocalIdx = 0; LocalIdx < MuscleElements.Num(); ++LocalIdx)
		{
			FiberDirections[MuscleElementIndices[LocalIdx]] = MuscleFiberDirs[LocalIdx];
		}

		if (bShowMuscleColor)
		{
			TManagedArray<FLinearColor>& Color =
				InCollection.AddAttribute<FLinearColor>("Color", FGeometryCollection::VerticesGroup);
			for (int32 i = 0; i < Color.Num(); ++i)
			{
				float Value = MuscleAttachmentScalarFieldTArray[i];
				if (1 <= Value && Value <= 2) // 1 <= Value <= 2 if muscle with origin and insertion
				{
					Color[i] = FLinearColor(FVector(Value - 1, 0, 2 - Value));
				}
			}
		}
		Out->SetValue(MoveTemp(InCollection), Context);
	}
}

TArray<int32>
FComputeFiberFieldNode::GetNonZeroIndices(const TArray<uint8>& Map) const
{
	int32 NumNonZero = 0;
	for (int32 i = 0; i < Map.Num(); i++)
		if (Map[i])
			NumNonZero++;
	TArray<int32> Indices; Indices.AddUninitialized(NumNonZero);
	int32 Idx = 0;
	for (int32 i = 0; i < Map.Num(); i++)
		if (Map[i])
			Indices[Idx++] = i;
	return Indices;
}

void FComputeFiberStreamlineNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	//
	// Gather inputs
	//

	FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	FFieldCollection OutVectorField;
	TArray<int32> InOriginIndices = GetValue<TArray<int32>>(Context, &OriginIndices);
	TArray<int32> InInsertionIndices = GetValue<TArray<int32>>(Context, &InsertionIndices);

	//
	// Pull Origin & Insertion data out of the geometry collection.  We may want other ways of specifying
	// these via an input on the node...
	//

	// Origin & Insertion
	TManagedArray<int32>* Origin = nullptr;
	TManagedArray<int32>* Insertion = nullptr;
	if (InOriginIndices.IsEmpty() || InInsertionIndices.IsEmpty())
	{
		// Origin & Insertion group
		if (OriginInsertionGroupName.IsEmpty())
		{
			UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'OriginInsertionGroupName' cannot be empty."));
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Origin vertices
		if (InOriginIndices.IsEmpty())
		{
			if (OriginVertexFieldName.IsEmpty())
			{
				UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'OriginVertexFieldName' cannot be empty."));
				Out->SetValue(MoveTemp(InCollection), Context);
				return;
			}
			Origin = InCollection.FindAttribute<int32>(FName(OriginVertexFieldName), FName(OriginInsertionGroupName));
			if (!Origin)
			{
				UE_LOG(LogChaosFlesh, Warning,
					TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
					*OriginVertexFieldName, *OriginInsertionGroupName);
				Out->SetValue(MoveTemp(InCollection), Context);
				return;
			}
		}

		// Insertion vertices
		if (InInsertionIndices.IsEmpty())
		{
			if (InsertionVertexFieldName.IsEmpty())
			{
				UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'InsertionVertexFieldName' cannot be empty."));
				Out->SetValue(MoveTemp(InCollection), Context);
				return;
			}
			Insertion = InCollection.FindAttribute<int32>(FName(InsertionVertexFieldName), FName(OriginInsertionGroupName));
			if (!Insertion)
			{
				UE_LOG(LogChaosFlesh, Warning,
					TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
					*InsertionVertexFieldName, *OriginInsertionGroupName);
				Out->SetValue(MoveTemp(InCollection), Context);
				return;
			}
		}
	}

	InOriginIndices = Origin ? Origin->GetConstArray() : InOriginIndices;
	InInsertionIndices = Insertion ? Insertion->GetConstArray() : InInsertionIndices;
	if (InOriginIndices.Num() == 0 || InInsertionIndices.Num() == 0)
	{
		FindOutput(&VectorField)->SetValue(MoveTemp(OutVectorField), Context);
		FindOutput(&Collection)->SetValue(MoveTemp(InCollection), Context);
		return;
	}
	//
	// Compute muscle fiber streamlines
	// Save streamlines to muscle group
	//
	GeometryCollection::Facades::FMuscleActivationFacade MuscleActivation(InCollection);

	TArray<TArray<TArray<FVector3f>>> Streamlines = MuscleActivation.BuildStreamlines(Origin ? Origin->GetConstArray() : InOriginIndices,
		Insertion ? Insertion->GetConstArray() : InInsertionIndices, NumLinesMultiplier, MaxStreamlineIterations, MaxPointsPerLine);

	//Render streamlines
	for (int32 i = 0; i < Streamlines.Num(); ++i)
	{
		for (int32 j = 0; j < Streamlines[i].Num(); ++j)
		{
			for (int32 k = 1; k < Streamlines[i][j].Num(); ++k)
			{
				OutVectorField.AddVectorToField(Streamlines[i][j][k - 1], Streamlines[i][j][k]);
			}
		}
	}

	//
	// Set output(s)
	//
	FindOutput(&VectorField)->SetValue(MoveTemp(OutVectorField), Context);
	FindOutput(&Collection)->SetValue(MoveTemp(InCollection), Context);
}