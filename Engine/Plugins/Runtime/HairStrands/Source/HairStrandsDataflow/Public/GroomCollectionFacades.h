// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GroomEdit.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"

#include "GroomCollectionFacades.generated.h"

class FGeometryCollection;
namespace Chaos
{
	class FChaosArchive;
}

/** Enum to pick strands or guides in dataflow nodes */
UENUM()
enum class EGroomCollectionType : uint8
{
	/** Strands type (Rendering Mesh) */
	Strands UMETA(DisplayName = "Strands"),

	/** Guides type (Simulation Mesh)*/
	Guides UMETA(DisplayName = "Guides"),
};

namespace UE::Groom
{
	/**
	* FGroomCollectionFacade 
	* @brief Base facade to store the TArray<T> groups necessary to setup the groom asset.
	*/
	template<typename DerivedType>
	class HAIRSTRANDSDATAFLOW_API FGroomCollectionFacade
	{
	public:
		/** Groom collection group names */
		static const FName CurvesGroup;
		static const FName EdgesGroup;
		static const FName ObjectsGroup;
		static const FName PointsGroup;
		static const FName VerticesGroup;
		
		/** Groom collection attribute names */
		static const FName CurvePointOffsetsAttribute;
		static const FName ObjectCurveOffsetsAttribute;
		static const FName EdgeRestOrientationsAttribute;
		static const FName PointRestPositionsAttribute;
		static const FName PointCurveIndicesAttribute;
		static const FName CurveObjectIndicesAttribute;
		static const FName VertexLinearColorsAttribute;
		static const FName ObjectGroupNamesAttribute;

		FGroomCollectionFacade(FManagedArrayCollection& InCollection);
		FGroomCollectionFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		void DefineSchema();

		/** Is the facade defined constant. */
		bool IsConst() const { return Collection == nullptr; }

		/** Is the Facade defined on the collection? */
		bool IsValid() const;
		
		/** Get the number of curves */
		int32 GetNumCurves() const { return CurvePointOffsets.Num(); }

		/** Get the number of sections */
		int32 GetNumObjects() const { return ObjectCurveOffsets.Num(); }

		/** Get the number of points */
		int32 GetNumPoints() const { return PointRestPositions.Num(); }

		/** Get the number of edges */
		int32 GetNumEdges() const { return EdgeRestOrientations.Num(); }

		/** Get the number of vertices */
		int32 GetNumVertices() const { return VertexLinearColors.Num(); }
		
		/** Get the point rest positions */
		const TArray<FVector3f>& GetPointRestPositions() const { return PointRestPositions.Get().GetConstArray(); }

		/** Get the edge rest orientations */
		const TArray<FQuat4f>& GetEdgeRestOrientations() const { return EdgeRestOrientations.Get().GetConstArray(); }

		/** Get the curve point offsets */
		const TArray<int32>& GetCurvePointOffsets() const { return CurvePointOffsets.Get().GetConstArray(); }

		/** Get the object curve offsets */
		const TArray<int32>& GetObjectCurveOffsets() const { return ObjectCurveOffsets.Get().GetConstArray(); }

		/** Get the point curve indices */
		const TArray<int32>& GetPointCurveIndices() const { return PointCurveIndices.Get().GetConstArray(); }

		/** Get the curve object indices */
		const TArray<int32>& GetCurveObjectIndices() const { return CurveObjectIndices.Get().GetConstArray(); }

		/** Get the curve object indices */
		const TArray<FLinearColor>& GetVertexLinearColors() const { return VertexLinearColors.Get().GetConstArray(); }

		/** Get the object group names */
		const TArray<FString>& GetObjectGroupNames() const { return ObjectGroupNames.Get().GetConstArray(); }
		
		/** Set the point rest positions */
		void SetPointRestPositions(const TArray<FVector3f>& InPointRestPositions) { PointRestPositions.Modify() = InPointRestPositions; UpdateEdgeRestOrientations(); }	

		/** Set the object group names */
		void SetObjectGroupNames(const TArray<FString>& InObjectGroupNames) { ObjectGroupNames.Modify() = InObjectGroupNames; }	
		
		/** Set the curve point offsets */
		void SetCurvePointOffsets(const TArray<int32>& InCurvePointOffsets) { CurvePointOffsets.Modify() = InCurvePointOffsets; UpdatePointCurveIndices(); }

		/** Set the object curve offsets */
		void SetObjectCurveOffsets(const TArray<int32>& InObjectCurveOffsets) { ObjectCurveOffsets.Modify() = InObjectCurveOffsets; UpdateCurveObjectIndices(); }
		
		/** Set the vertex linear colors */
		void SetVertexLinearColors(const TArray<FLinearColor>& InVertexLinearColors) { VertexLinearColors.Modify() = InVertexLinearColors; }
		
		/** Initialize the curve vertices from point positions and objects offsets */
		void InitGroomCollection(const TArray<FVector3f>& InPointRestPositions,
			const TArray<int32>& InCurvePointOffsets, const TArray<int32>& InObjectCurveOffsets, const TArray<FString>& InObjectGroupNames);

		/** Get the managed array collection */
		const FManagedArrayCollection& GetManagedArrayCollection() const {return ConstCollection;}
		
	protected :
		
		/** Update the edge rest orientations with parallel transport */
		void UpdateEdgeRestOrientations();
		
		/** Update the point curve indices from the offsets  */
		void UpdatePointCurveIndices();

		/** Update the curve object indices from the offsets  */
		void UpdateCurveObjectIndices();
		
		/** Const collection the facade is linked to */
		const FManagedArrayCollection& ConstCollection;

		/** Non-const collection the facade is linked to */
		FManagedArrayCollection* Collection = nullptr;
		
		/** Groom edges local orientation */
		TManagedArrayAccessor<FQuat4f> EdgeRestOrientations;

		/** Groom points local position */
		TManagedArrayAccessor<FVector3f> PointRestPositions;

		/** Groom curves point offset */
		TManagedArrayAccessor<int32> CurvePointOffsets;
                                                 
		/** Groom objects curve offset */
		TManagedArrayAccessor<int32> ObjectCurveOffsets;

		/** Groom points curve index */
		TManagedArrayAccessor<int32> PointCurveIndices;

		/** Groom curves object index */
		TManagedArrayAccessor<int32> CurveObjectIndices;

		/** Groom vertices linear color */
		TManagedArrayAccessor<FLinearColor> VertexLinearColors;

		/** Groom object group names */
		TManagedArrayAccessor<FString> ObjectGroupNames;
	};
	
	/**
	* FGroomStrandsFacade 
	* @brief Strands facade to store the TArray<T> groups necessary to setup the groom strands.
	*/
	class HAIRSTRANDSDATAFLOW_API FGroomStrandsFacade : public FGroomCollectionFacade<FGroomStrandsFacade>
	{
	public:
		/** Groom collection group names */
		static const FName GroupPrefix;
		using FEditableType = FEditableHairStrand;

		FGroomStrandsFacade(FManagedArrayCollection& InCollection);
		FGroomStrandsFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		void DefineFacadeSchema() {}

		/** Is the Facade defined on the collection? */
		bool IsFacadeValid() const {return true;}

		/** Init facade collection attributes */
		void InitFacadeCollection() {};

		/** Get the editable groom types */
		static const TArray<FEditableType>& GetEditableGroom(const FEditableGroomGroup& GroomGroup)
		{
			return GroomGroup.Strands;
		}
	};

	/**
	* FGroomGuidesFacade 
	* @brief Guides facade to store the TArray<T> groups necessary to setup the groom guides.
	*/
	class HAIRSTRANDSDATAFLOW_API FGroomGuidesFacade : public FGroomCollectionFacade<FGroomGuidesFacade>
	{
	public:
		/** Groom collection group names */
		static const FName GroupPrefix;
		using FEditableType = FEditableHairGuide;

		FGroomGuidesFacade(FManagedArrayCollection& InCollection);
		FGroomGuidesFacade(const FManagedArrayCollection& InCollection);

		static const FName ObjectMeshLODsAttribute;
		static const FName ObjectSkeletalMeshesAttribute;
		static const FName PointKinematicWeightsAttribute;
		static const FName PointBoneIndicesAttribute;
		static const FName PointBoneWeightsAttribute;
		static const FName ObjectPointSamplesAttribute;
		static const FName CurveStrandIndicesAttribute;
		static const FName CurveParentIndicesAttribute;
		static const FName CurveLodIndicesAttribute;
		
		/** Create the facade attributes. */
		void DefineFacadeSchema();

		/** Is the Facade defined on the collection? */
		bool IsFacadeValid() const;

		/** Init facade collection attributes */
		void InitFacadeCollection();

		/** Get the editable groom types */
		static const TArray<FEditableType>& GetEditableGroom(const FEditableGroomGroup& GroomGroup)
		{
			return GroomGroup.Guides;
		}
		
		/** Get the point kinematic weights */
		const TArray<float>& GetPointKinematicWeights() const { return PointKinematicWeights.Get().GetConstArray(); }
		
		/** Get the point bone indices */
		const FIntVector4& GetPointBoneIndices(const int32 PointIndex) const { return PointBoneIndices.Get()[PointIndex]; }

		/** Get the point bone weights*/
		const FVector4f& GetPointBoneWeights(const int32 PointIndex) const { return PointBoneWeights.Get()[PointIndex]; }

		/** Get the object point samples */
		const TArray<int32>& GetObjectPointSamples() const { return ObjectPointSamples.Get().GetConstArray(); }
		
		/** Get the guide strand indices */
		const TArray<int32>& GetCurveStrandIndices() const { return CurveStrandIndices.Get().GetConstArray(); }

		/** Get the guide parent indices */
		const TArray<int32>& GetCurveParentIndices() const { return CurveParentIndices.Get().GetConstArray(); }
		
		/** Get the guide lod indices */
		const TArray<int32>& GetCurveLodIndices() const { return CurveLodIndices.Get().GetConstArray(); }

		/** Set the vertex kinematic weights */
		void SetPointKinematicWeights(const TArray<float>& InKinematicWeights) { PointKinematicWeights.Modify() = InKinematicWeights; }

		/** Set the point bone indices */
		void SetPointBoneIndices(const int32 PointIndex, const FIntVector4& InBoneIndices) { return PointBoneIndices.ModifyAt(PointIndex,InBoneIndices); }

		/** Set the point bone weights*/
		void SetPointBoneWeights(const int32 PointIndex, const FVector4f& InBoneWeights) { return PointBoneWeights.ModifyAt(PointIndex, InBoneWeights); }
		
		/** Set the object point samples */
		void SetObjectPointSamples(const TArray<int32>& NumPointSamples) { ObjectPointSamples.Modify() = NumPointSamples; }
		
		/** Set the guide strand indices */
        void SetCurveStrandIndices(const TArray<int32>& StrandIndices) { CurveStrandIndices.Modify() = StrandIndices; }

		/** Set the guide parent indices */
		void SetCurveParentIndices(const TArray<int32>& ParentIndices) { CurveParentIndices.Modify() = ParentIndices; }

		/** Set the guide lod indices */
		void SetCurveLodIndices(const TArray<int32>& LodIndices) { CurveLodIndices.Modify() = LodIndices; }

		/** Resize the geometry groups (vertex, points, edges) based on a new total number of points */
		void ResizePointsGroups(const int32 NumPoints) const;

	protected :
		
		/** Max distance from the kinematic target */
		TManagedArrayAccessor<float> PointKinematicWeights;

		/** Point bone indices */
		TManagedArrayAccessor<FIntVector4> PointBoneIndices;

		/** Point bone weights */
		TManagedArrayAccessor<FVector4f> PointBoneWeights;

		/** Object point samples */
		TManagedArrayAccessor<int32> ObjectPointSamples;
		
		/** Strand index from which the guide has been generated */
		TManagedArrayAccessor<int32> CurveStrandIndices;

		/** Parent guide indices */
		TManagedArrayAccessor<int32> CurveParentIndices;

		/** Lod guide indices */
		TManagedArrayAccessor<int32> CurveLodIndices;
	};
}
