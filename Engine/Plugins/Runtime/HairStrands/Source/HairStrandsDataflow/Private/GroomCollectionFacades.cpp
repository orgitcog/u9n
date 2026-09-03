// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomCollectionFacades.h"
#include "GeometryCollection/GeometryCollection.h"

namespace UE::Groom
{
	// Strands/Guides Groups
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::CurvesGroup(DerivedType::GroupPrefix.ToString() + FString("Curves"));
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::EdgesGroup(DerivedType::GroupPrefix.ToString() + FString("Edges"));
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::ObjectsGroup(DerivedType::GroupPrefix.ToString() + FString("Objects"));
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::PointsGroup(DerivedType::GroupPrefix.ToString() + FString("Points"));
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::VerticesGroup(DerivedType::GroupPrefix.ToString() + FString("Vertices"));
	
	// Strands/Guides Attributes
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::CurvePointOffsetsAttribute("CurvePointOffsets");
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::PointCurveIndicesAttribute("PointCurveIndices");
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::ObjectCurveOffsetsAttribute("ObjectCurveOffsets");
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::CurveObjectIndicesAttribute("CurveObjectIndices");
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::EdgeRestOrientationsAttribute("EdgeRestOrientations");
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::PointRestPositionsAttribute("PointRestPositions");
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::VertexLinearColorsAttribute("VertexLinearColors");
	template<typename DerivedType>
	const FName FGroomCollectionFacade<DerivedType>::ObjectGroupNamesAttribute("ObjectGroupNames");

	// Strands/Guides Prefix
	const FName FGroomStrandsFacade::GroupPrefix("Strands");
	const FName FGroomGuidesFacade::GroupPrefix("Guides");

	// Guides Attributes
	const FName FGroomGuidesFacade::ObjectMeshLODsAttribute("ObjectMeshLODs");
	const FName FGroomGuidesFacade::ObjectSkeletalMeshesAttribute("ObjectSkeletalMeshes");
	const FName FGroomGuidesFacade::PointKinematicWeightsAttribute("PointKinematicWeights");
	const FName FGroomGuidesFacade::PointBoneIndicesAttribute("PointBoneIndices");
	const FName FGroomGuidesFacade::PointBoneWeightsAttribute("PointBoneWeights");
	const FName FGroomGuidesFacade::ObjectPointSamplesAttribute("ObjectPointSamples");
	const FName FGroomGuidesFacade::CurveStrandIndicesAttribute("CurveStrandIndices");
	const FName FGroomGuidesFacade::CurveParentIndicesAttribute("CurveParentIndices");
	const FName FGroomGuidesFacade::CurveLodIndicesAttribute("CurveLodIndices");
	
	template<typename DerivedType>
	FGroomCollectionFacade<DerivedType>::FGroomCollectionFacade(FManagedArrayCollection& InCollection) :
		ConstCollection(InCollection), Collection(&InCollection),
		EdgeRestOrientations(InCollection, EdgeRestOrientationsAttribute, EdgesGroup),
		PointRestPositions(InCollection, PointRestPositionsAttribute, PointsGroup),
		CurvePointOffsets(InCollection, CurvePointOffsetsAttribute, CurvesGroup),
		ObjectCurveOffsets(InCollection, ObjectCurveOffsetsAttribute, ObjectsGroup),
		PointCurveIndices(InCollection, PointCurveIndicesAttribute, PointsGroup),
		CurveObjectIndices(InCollection, CurveObjectIndicesAttribute, CurvesGroup),
		VertexLinearColors(InCollection, VertexLinearColorsAttribute, VerticesGroup),
		ObjectGroupNames(InCollection, ObjectGroupNamesAttribute, ObjectsGroup)
	{
	}

	template<typename DerivedType>
	FGroomCollectionFacade<DerivedType>::FGroomCollectionFacade(const FManagedArrayCollection& InCollection) :
		ConstCollection(InCollection), Collection(nullptr),
		EdgeRestOrientations(InCollection, EdgeRestOrientationsAttribute, EdgesGroup),
		PointRestPositions(InCollection, PointRestPositionsAttribute, PointsGroup),
		CurvePointOffsets(InCollection, CurvePointOffsetsAttribute, CurvesGroup),
		ObjectCurveOffsets(InCollection, ObjectCurveOffsetsAttribute, ObjectsGroup),
		PointCurveIndices(InCollection, PointCurveIndicesAttribute, PointsGroup),
		CurveObjectIndices(InCollection, CurveObjectIndicesAttribute, CurvesGroup),
		VertexLinearColors(InCollection, VertexLinearColorsAttribute, VerticesGroup),
		ObjectGroupNames(InCollection, ObjectGroupNamesAttribute, ObjectsGroup)
	{
	}

	template<typename DerivedType>
	bool FGroomCollectionFacade<DerivedType>::IsValid() const
	{
		return EdgeRestOrientations.IsValid() && PointRestPositions.IsValid() && 
			CurvePointOffsets.IsValid() && ObjectCurveOffsets.IsValid() && PointCurveIndices.IsValid() &&
					CurveObjectIndices.IsValid() && VertexLinearColors.IsValid() && ObjectGroupNames.IsValid() && static_cast<const DerivedType*>(this)->IsFacadeValid();
	}

	template<typename DerivedType>
	void FGroomCollectionFacade<DerivedType>::DefineSchema()
	{
		check(!IsConst());
		EdgeRestOrientations.Add();
		PointRestPositions.Add();
		CurvePointOffsets.Add();
		ObjectCurveOffsets.Add();
		PointCurveIndices.Add();
		CurveObjectIndices.Add();
		VertexLinearColors.Add();
		ObjectGroupNames.Add();
		
		static_cast<DerivedType*>(this)->DefineFacadeSchema();
	}

	template<typename DerivedType>
	void FGroomCollectionFacade<DerivedType>::InitGroomCollection(const TArray<FVector3f>& InPointRestPositions,
		const TArray<int32>& InCurvePointOffsets, const TArray<int32>& InObjectCurveOffsets, const TArray<FString>& InObjectGroupNames)
	{
		if(Collection)
		{
			// Curves group
			Collection->EmptyGroup(CurvesGroup);
			Collection->AddElements(InCurvePointOffsets.Num(), CurvesGroup);

			// Object group
			Collection->EmptyGroup(ObjectsGroup);
			Collection->AddElements(InObjectCurveOffsets.Num(), ObjectsGroup);

			// Points group
			Collection->EmptyGroup(PointsGroup);
			Collection->AddElements(InPointRestPositions.Num(), PointsGroup);
			
			// Edges group
			Collection->EmptyGroup(EdgesGroup);
			Collection->AddElements(InPointRestPositions.Num()-InCurvePointOffsets.Num(), EdgesGroup);

			// Vertices groups
			Collection->EmptyGroup(VerticesGroup);
			Collection->AddElements(InPointRestPositions.Num() * 2, VerticesGroup);

			// Fill attributes
			SetObjectCurveOffsets(InObjectCurveOffsets);
			SetCurvePointOffsets(InCurvePointOffsets);
			SetPointRestPositions(InPointRestPositions);
			SetObjectGroupNames(InObjectGroupNames);

			static_cast<DerivedType*>(this)->InitFacadeCollection();
		}
	}

	template<typename DerivedType>
	void FGroomCollectionFacade<DerivedType>::UpdateCurveObjectIndices()
	{
		const int32 NumObjects = GetNumObjects();

		int32 CurveOffset = 0;
		for(int32 ObjectIndex = 0; ObjectIndex < NumObjects; ++ObjectIndex)
		{
			for(int32 CurveIndex = CurveOffset, CurveEnd = ObjectCurveOffsets[ObjectIndex];
					  CurveIndex < CurveEnd; ++CurveIndex)
			{
				CurveObjectIndices.ModifyAt(CurveIndex,ObjectIndex);
			}
			CurveOffset = ObjectCurveOffsets[ObjectIndex];
		}
	}

	template<typename DerivedType>
	void FGroomCollectionFacade<DerivedType>::UpdatePointCurveIndices()
	{
		const int32 NumCurves = GetNumCurves();

		int32 PointOffset = 0;
		for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
		{
			for (int32 PointIndex = PointOffset, PointEnd = CurvePointOffsets[CurveIndex];
					   PointIndex < PointEnd; ++PointIndex)
			{
				PointCurveIndices.ModifyAt(PointIndex, CurveIndex);
			}
			PointOffset = CurvePointOffsets[CurveIndex];
		}
	}

	template<typename DerivedType>
	void FGroomCollectionFacade<DerivedType>::UpdateEdgeRestOrientations()
	{
		const int32 NumCurves = GetNumCurves();

		int32 EdgeOffset = 0;
		int32 PointOffset = 0;
		for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
		{
			FVector3f TangentPrev = FVector3f(0.,0.,1.), TangentNext = FVector3f::Zero();
			FQuat4f EdgeOrientation = FQuat4f::Identity;

			const int32 NumEdges = (CurvePointOffsets[CurveIndex] - PointOffset)-1;
			for (int32 EdgeIndex = 0; EdgeIndex < NumEdges; ++EdgeIndex)
			{
				TangentPrev = TangentNext;
				TangentNext = (PointRestPositions[PointOffset+EdgeIndex+1]-PointRestPositions[PointOffset+EdgeIndex]).GetSafeNormal();

				EdgeOrientation = (FQuat4f::FindBetweenNormals(TangentPrev,TangentNext) * EdgeOrientation).GetNormalized();
				EdgeRestOrientations.ModifyAt(EdgeOffset+EdgeIndex, EdgeOrientation);
			}
			EdgeOffset += NumEdges;
			PointOffset = CurvePointOffsets[CurveIndex];
		}
	}

	template class FGroomCollectionFacade<FGroomStrandsFacade>;
	template class FGroomCollectionFacade<FGroomGuidesFacade>;

	// Strands
	
	FGroomStrandsFacade::FGroomStrandsFacade(FManagedArrayCollection& InCollection) :
		FGroomCollectionFacade<FGroomStrandsFacade>(InCollection)
	{
		DefineSchema();
	}

	FGroomStrandsFacade::FGroomStrandsFacade(const FManagedArrayCollection& InCollection) :
		FGroomCollectionFacade<FGroomStrandsFacade>(InCollection)
	{
	}

	// Guides
	
	FGroomGuidesFacade::FGroomGuidesFacade(FManagedArrayCollection& InCollection) :
		FGroomCollectionFacade<FGroomGuidesFacade>(InCollection),
		PointKinematicWeights(InCollection, PointKinematicWeightsAttribute, VerticesGroup),
		PointBoneIndices(InCollection, PointBoneIndicesAttribute, VerticesGroup),
		PointBoneWeights(InCollection, PointBoneWeightsAttribute, VerticesGroup),
		ObjectPointSamples(InCollection, ObjectPointSamplesAttribute, ObjectsGroup),
		CurveStrandIndices(InCollection, CurveStrandIndicesAttribute, CurvesGroup),
		CurveParentIndices(InCollection, CurveParentIndicesAttribute, CurvesGroup),
		CurveLodIndices(InCollection, CurveLodIndicesAttribute, CurvesGroup)
	{
		DefineSchema();
	}

	FGroomGuidesFacade::FGroomGuidesFacade(const FManagedArrayCollection& InCollection) :
		FGroomCollectionFacade<FGroomGuidesFacade>(InCollection),
		PointKinematicWeights(InCollection, PointKinematicWeightsAttribute, VerticesGroup),
		PointBoneIndices(InCollection, PointBoneIndicesAttribute, VerticesGroup),
		PointBoneWeights(InCollection, PointBoneWeightsAttribute, VerticesGroup),
		ObjectPointSamples(InCollection, ObjectPointSamplesAttribute, ObjectsGroup),
		CurveStrandIndices(InCollection, CurveStrandIndicesAttribute, CurvesGroup),
		CurveParentIndices(InCollection, CurveParentIndicesAttribute, CurvesGroup),
		CurveLodIndices(InCollection, CurveLodIndicesAttribute, CurvesGroup)
	{
	}

	bool FGroomGuidesFacade::IsFacadeValid() const
	{
		return PointKinematicWeights.IsValid() && PointBoneIndices.IsValid() && PointBoneWeights.IsValid() && ObjectPointSamples.IsValid() &&
			CurveStrandIndices.IsValid() && CurveLodIndices.IsValid() && CurveParentIndices.IsValid();
	}

	void FGroomGuidesFacade::DefineFacadeSchema()
	{
		check(!IsConst());
		PointKinematicWeights.Add();
		PointBoneWeights.Add();
		PointBoneIndices.Add();
		ObjectPointSamples.Add();
		CurveStrandIndices.Add();
		CurveParentIndices.Add();
		CurveLodIndices.Add();
	}

	void FGroomGuidesFacade::InitFacadeCollection()
	{
		// Reset attributes
		PointKinematicWeights.Modify().Fill(0.0f);
		PointBoneIndices.Modify().Fill(FIntVector4::ZeroValue);
		PointBoneWeights.Modify().Fill(FVector4f::Zero());
		ObjectPointSamples.Modify().Fill(4);
		CurveStrandIndices.Modify().Fill(INDEX_NONE);
		CurveParentIndices.Modify().Fill(INDEX_NONE);
		CurveLodIndices.Modify().Fill(INDEX_NONE);
	}

	void FGroomGuidesFacade::ResizePointsGroups(const int32 NumPoints) const 
	{
		if(Collection)
		{
			const int32 NumCurves = GetNumCurves();
			
			Collection->EmptyGroup(PointsGroup);
			Collection->AddElements(NumPoints, PointsGroup);

			Collection->EmptyGroup(EdgesGroup);
			Collection->AddElements(NumPoints-NumCurves, EdgesGroup);

			Collection->EmptyGroup(VerticesGroup);
			Collection->AddElements(NumPoints*2, VerticesGroup);
		}
	}
}

