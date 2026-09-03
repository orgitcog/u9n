// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ConvertData.h"
#include "MuR/MeshPrivate.h"
#include "MuR/Operations.h"
#include "MuR/OpMeshBind.h"

#include "Math/Plane.h"
#include "Math/Ray.h"
#include "Math/NumericLimits.h"

#include "IndexTypes.h"
#include "Spatial/MeshAABBTree3.h"
#include "Distance/DistPoint3Triangle3.h"
#include "LineTypes.h"
#include "MuR/OpMeshSmoothing.h"
#include "MuR/OpMeshComputeNormals.h"

#include "PackedNormal.h"

#include "Algo/AnyOf.h" 


// TODO: Make the handling of rotations an option. It is more expensive on CPU and memory, and for some
// cases it is not required at all.

// TODO: Face stretch to scale the deformation per-vertex? 

// TODO: Support multiple binding influences per vertex, to have smoother deformations.

// TODO: Support multiple binding sets, to have different shapes deformations at once.

// TODO: Deformation mask, to select the intensisty of the deformation per-vertex.

// TODO: This is a reference implementation with ample roof for optimization.

namespace mu
{

	struct FShapeMeshDescriptorApply
	{
		TArrayView<const FVector3f> Positions;
		TArray<FVector3f> Normals;
		TArrayView<const UE::Geometry::FIndex3i> Triangles;
	};

	// Methods to actually deform a point
	inline void GetDeform( const FShapeMeshDescriptorApply& Shape, const FReshapeVertexBindingData& Binding, FVector3f& NewPosition, FVector3f& NewNormalPosition)
	{
		const UE::Geometry::FIndex3i& Triangle = Shape.Triangles[Binding.Triangle];
	
		FVector3f VA = Shape.Positions[Triangle.A] + Shape.Normals[Triangle.A]*Binding.D.X;
		FVector3f VB = Shape.Positions[Triangle.B] + Shape.Normals[Triangle.B]*Binding.D.Y;
		FVector3f VC = Shape.Positions[Triangle.C] + Shape.Normals[Triangle.C]*Binding.D.Z;

		NewPosition = VA*Binding.S + VB*Binding.T + VC*(1.0f - Binding.S - Binding.T);
	
		FVector3f NVA = Shape.Positions[Triangle.A] + Shape.Normals[Triangle.A]*Binding.NormalD.X;
		FVector3f NVB = Shape.Positions[Triangle.B] + Shape.Normals[Triangle.B]*Binding.NormalD.Y;
		FVector3f NVC = Shape.Positions[Triangle.C] + Shape.Normals[Triangle.C]*Binding.NormalD.Z;

		NewNormalPosition = (NVA*Binding.NS + NVB*Binding.NT + NVC*(1.0f - Binding.NS - Binding.NT));
	}

	inline void GetDeform( const FShapeMeshDescriptorApply& Shape, const FReshapePointBindingData& Binding, FVector3f& NewPosition)
	{
		const UE::Geometry::FIndex3i& Triangle = Shape.Triangles[Binding.Triangle];
	
		FVector3f VA = Shape.Positions[Triangle.A] + Shape.Normals[Triangle.A]*Binding.D.X;
		FVector3f VB = Shape.Positions[Triangle.B] + Shape.Normals[Triangle.B]*Binding.D.Y;
		FVector3f VC = Shape.Positions[Triangle.C] + Shape.Normals[Triangle.C]*Binding.D.Z;

		NewPosition = VA*Binding.S + VB*Binding.T + VC*(1.0f - Binding.S - Binding.T);
	}


    //---------------------------------------------------------------------------------------------
    //! Physics Bodies Reshape 
    //---------------------------------------------------------------------------------------------
	template<uint32 NumPoints>
	inline bool GetDeformedPoints( const FShapeMeshDescriptorApply& Shape, const FReshapePointBindingData* BindingData,  TStaticArray<FVector3f, NumPoints>& OutPoints )
	{
		const int32 ShapeNumTris = Shape.Triangles.Num();
		for (int32 I = 0; I < NumPoints; ++I)
		{
			const FReshapePointBindingData& BindingDataPoint = *(BindingData + I);
			if ((BindingDataPoint.Triangle < 0) | (BindingDataPoint.Triangle >= ShapeNumTris))
			{
				return false;
			}
		}

		for (int32 I = 0; I < NumPoints; ++I)
		{
			GetDeform(Shape, *(BindingData + I), OutPoints[I]);
		}

		return true;
	}

	inline void GetDeformedConvex( const FShapeMeshDescriptorApply& Shape, const FReshapePointBindingData* BindingData, TArrayView<FVector3f>& InOutDeformedVertices )
	{
		const int32 ShapeNumTris = Shape.Triangles.Num();
		const int32 ConvexVertCount = InOutDeformedVertices.Num();

		for (int32 I = 0; I < ConvexVertCount; ++I)
		{
			const FReshapePointBindingData& BindingDataPoint = *(BindingData + I);

			if ((BindingDataPoint.Triangle < 0) | (BindingDataPoint.Triangle >= ShapeNumTris))
			{
				continue;
			}

			GetDeform(Shape, BindingDataPoint, InOutDeformedVertices[I]);	
		}
	}

	inline void ComputeSphereFromDeformedPoints( 
			const TStaticArray<FVector3f, 6>& Points, 
			FVector3f& OutP, float& OutR, 
			const FTransform3f& InvBoneT )
	{	
		FVector3f Centroid(ForceInitToZero);
		for (const FVector3f& V : Points)
		{
			Centroid += V;	
		}

		Centroid *= (1.0f/6.0f);

		OutP = Centroid + InvBoneT.GetTranslation();
		
		float Radius = 0.0f;
		for (const FVector3f& V : Points)
		{
			Radius += (V - Centroid).Length();
		}

		Radius *= (1.0f/6.0f);
		
		OutR = Radius;
	}

	inline void ComputeBoxFromDeformedPoints( 
			const TStaticArray<FVector3f, 14>& Points, 
			FVector3f& OutP, FQuat4f& OutQ, FVector3f& OutS, 
			const FTransform3f& InvBoneT )
	{	
		const FVector3f TopC = (Points[0] + Points[1] + Points[2] + Points[3])*0.25f;
		const FVector3f BottomC = (Points[4+0] + Points[4+1] + Points[4+2] + Points[4+3])*0.25f;
    
		const FVector3f FrontC = (Points[0] + Points[1] + Points[4+0] + Points[4+1])*0.25f;
		const FVector3f BackC = (Points[2] + Points[3] + Points[4+2] + Points[4+3])*0.25f;
    
		const FVector3f RightC =  (Points[1] + Points[2] + Points[4+1] + Points[4+2])*0.25f;
		const FVector3f LeftC =  (Points[3] + Points[0] + Points[4+3] + Points[4+0])*0.25f;
    
		FVector3f ZB = (TopC - BottomC).GetSafeNormal();
		FVector3f XB = (RightC - LeftC).GetSafeNormal();
		FVector3f YB = (FrontC - BackC).GetSafeNormal();


		// Pick the 2 most offaxis vectors and construct a rotation form those.	
		// TODO: Find a better way of finding and orientation form ZB, XB, TN maybe by averaging somehow different bases created from the vectors, with quaternions?
		FVector3f OF = FVector3f( XB.X, YB.Y, ZB.Y ).GetAbs();

		float M0 = FMath::Max3( OF.X, OF.Y, OF.X);
		float M1 = M0 == OF.X ? FMath::Max(OF.Y, OF.Z) : (M0 == OF.Y ? FMath::Max(OF.X,OF.Z) : FMath::Max(OF.Y, OF.X));
		
		FMatrix44f RotationMatrix = FMatrix44f::Identity;
		if (M0 == OF.X)
		{
			RotationMatrix = M1 == OF.Y 
					? FRotationMatrix44f::MakeFromXY(XB, YB)
					: FRotationMatrix44f::MakeFromXZ(XB, ZB);
		}
		else if (M0 == OF.Y)
		{
			RotationMatrix = M1 == XB.X 
					? FRotationMatrix44f::MakeFromYX(YB, XB)
					: FRotationMatrix44f::MakeFromYZ(YB, ZB);
		}
		else
		{
			RotationMatrix = M1 == XB.X 
					? FRotationMatrix44f::MakeFromZX(ZB, XB)
					: FRotationMatrix44f::MakeFromZY(ZB, YB);	
		} 
	
		FTransform3f ShapeToBone = FTransform3f(RotationMatrix.ToQuat(), (TopC + BottomC) * 0.5f) * InvBoneT;

		OutQ = ShapeToBone.GetRotation();
		OutP = ShapeToBone.GetTranslation();
		OutS = FVector3f((RightC - LeftC).Size(), (FrontC - BackC).Size(), (TopC - BottomC).Size()) * 0.5f;	
	}

	inline void ComputeSphylFromDeformedPoints( 
			const TStaticArray<FVector3f, 14>& Points, 
			FVector3f& OutP, FQuat4f& OutQ, float& OutR, float& OutL, 
			const FTransform3f& InvBoneT)
	{
		constexpr int32 NumCentroids = 5;
		TStaticArray<FVector3f, NumCentroids> Centroids;
		Centroids[0] = Points[0];
		Centroids[1] = Points[1];

		for (int32 I = 0; I < NumCentroids - 2; ++I)
		{
			Centroids[2 + I] = 
				(Points[2 + I*4 + 0] + Points[2 + I*4 + 1] +
				 Points[2 + I*4 + 2] + Points[2 + I*4 + 3] ) * 0.25f;
		}

		// Geometric linear regression of top, bottom and rings centroids.
		FVector3f Centroid = FVector3f::Zero();
		for (const FVector3f& C : Centroids) //-V1078
		{
			Centroid += C;
		}

		constexpr float OneOverNumCentroids = 1.0f / static_cast<float>(NumCentroids);
		Centroid *= OneOverNumCentroids;
		
		for (FVector3f& C : Centroids) //-V1078
		{
			C -= Centroid;
		}

		constexpr int32 NumIters = 3;
		FVector3f Direction = (Centroids[0] - Centroids[1]).GetSafeNormal();
		for (int32 Iter = 0; Iter < NumIters; ++Iter)
		{
			FVector3f IterDirRefinement = Direction;
			for (const FVector3f& C : Centroids) //-V1078
			{
				IterDirRefinement += C * FVector3f::DotProduct(Direction, C);
			}

			Direction = IterDirRefinement.GetSafeNormal();
		}

		// Project centroids to the line described by Direction and Centroid.
		for (FVector3f& C : Centroids) //-V1078
		{
			C = Centroid + Direction * FVector3f::DotProduct(C, Direction);
		}

		// Quaternion form {0,0,1} to Direction.
		const FQuat4f Rotation = FQuat4f(-Direction.Y, Direction.X, 0.0f, 1.0f + FMath::Max(Direction.Z, -1.0f + UE_SMALL_NUMBER)).GetNormalized(); 

		FTransform3f ShapeToBone = FTransform3f(Rotation, Centroid) * InvBoneT;
		OutQ = ShapeToBone.GetRotation();
		OutP = ShapeToBone.GetTranslation();

		// Project ring points to plane formed by ring centroid and direction to extract ring radius.
		const auto ComputeRadiusContribution = [&](const FVector3f& P, const FVector3f& Origin, const FVector3f& Dir) -> float
		{
			return ((P + Dir * FVector3f::DotProduct(Dir, P - Origin)) - Origin).Length();
		};

		const float R0 = 
			ComputeRadiusContribution(Points[2], Centroids[2], Direction) +
			ComputeRadiusContribution(Points[3], Centroids[2], Direction) +
			ComputeRadiusContribution(Points[4], Centroids[2], Direction) +
			ComputeRadiusContribution(Points[5], Centroids[2], Direction);

		const float R1 = 
			ComputeRadiusContribution(Points[6], Centroids[3], Direction) +
			ComputeRadiusContribution(Points[7], Centroids[3], Direction) +
			ComputeRadiusContribution(Points[8], Centroids[3], Direction) +
			ComputeRadiusContribution(Points[9], Centroids[3], Direction);

		const float R2 = 
			ComputeRadiusContribution(Points[10], Centroids[4], Direction) +
			ComputeRadiusContribution(Points[11], Centroids[4], Direction) +
			ComputeRadiusContribution(Points[12], Centroids[4], Direction) +
			ComputeRadiusContribution(Points[13], Centroids[4], Direction);

		OutR =  (R0 + R1 + R2) * (0.25f/3.0f);
		OutL = FMath::Max(0.0f, (Centroids[0] - Centroids[1]).Length() - OutR*2.0f);
	}

	inline void ComputeTaperedCapsuleFromDeformedPoints( 
			const TStaticArray<FVector3f, 14>& Points, 
			FVector3f& OutP, FQuat4f& OutQ, float& OutR0, float& OutR1, float& OutL, 
			const FTransform3f& InvBoneT)
	{
		constexpr int32 NumCentroids = 5;
		TStaticArray<FVector3f, NumCentroids> Centroids;
		Centroids[0] = Points[0];
		Centroids[1] = Points[1];

		for (int32 I = 0; I < NumCentroids - 2; ++I)
		{
			Centroids[2 + I] = 
				(Points[2 + I*4 + 0] + Points[2 + I*4 + 1] +
				 Points[2 + I*4 + 2] + Points[2 + I*4 + 3] ) * 0.25f;
		}
	
		// Geometric linear regression of top, bottom and ring centroids.
		FVector3f Centroid = FVector3f::Zero();
		for (const FVector3f& C : Centroids) //-V1078
		{
			Centroid += C;
		}

		constexpr float OneOverNumCentroids = 1.0f / static_cast<float>(NumCentroids);
		Centroid *= OneOverNumCentroids;
		
		for (FVector3f& C : Centroids) //-V1078
		{
			C -= Centroid;
		}

		constexpr int32 NumIters = 3;
		FVector3f Direction = (Centroids[0] - Centroids[1]).GetSafeNormal();
		for (int32 Iter = 0; Iter < NumIters; ++Iter)
		{
			FVector3f IterDirRefinement = Direction;
			for (const FVector3f& C : Centroids) //-V1078
			{
				IterDirRefinement += C * FVector3f::DotProduct(Direction, C);
			}

			Direction = IterDirRefinement.GetSafeNormal();
		}

		// Project centroids to the line described by Direction and Centroid.
		for (FVector3f& C : Centroids) //-V1078
		{
			C = Centroid + Direction * FVector3f::DotProduct(C, Direction);
		}

		// Quaternion form {0,0,1} to Direction.
		const FQuat4f Rotation = FQuat4f(-Direction.Y, Direction.X, 0.0f, 1.0f + FMath::Max(Direction.Z, -1.0f + UE_SMALL_NUMBER)).GetNormalized(); 

		FTransform3f ShapeToBone = FTransform3f(Rotation, Centroid) * InvBoneT;
		OutQ = ShapeToBone.GetRotation();
		OutP = ShapeToBone.GetTranslation();

		// Project ring points to plane formed by ring centroid and direction to extract ring radius.
		const auto ComputeRadiusContribution = [&](const FVector3f& P, const FVector3f& Origin, const FVector3f& Dir) -> float
		{
			return ((P + Dir * FVector3f::DotProduct(Dir, P - Origin)) - Origin).Length();
		};

		const float R0 = 
			ComputeRadiusContribution(Points[2], Centroids[2], Direction) +
			ComputeRadiusContribution(Points[3], Centroids[2], Direction) +
			ComputeRadiusContribution(Points[4], Centroids[2], Direction) +
			ComputeRadiusContribution(Points[5], Centroids[2], Direction);

		const float R1 = 
			ComputeRadiusContribution(Points[6], Centroids[3], Direction) +
			ComputeRadiusContribution(Points[7], Centroids[3], Direction) +
			ComputeRadiusContribution(Points[8], Centroids[3], Direction) +
			ComputeRadiusContribution(Points[9], Centroids[3], Direction);

		const float R2 = 
			ComputeRadiusContribution(Points[10], Centroids[4], Direction) +
			ComputeRadiusContribution(Points[11], Centroids[4], Direction) +
			ComputeRadiusContribution(Points[12], Centroids[4], Direction) +
			ComputeRadiusContribution(Points[13], Centroids[4], Direction);

		// TODO: Ajust for R1, center ring radius.
		OutR0 = R0*0.25f;
		OutR1 = R2*0.25f;
		OutL = FMath::Max(0.0f, (Centroids[0] - Centroids[1]).Length() - (OutR0 + OutR1));
	}

	inline void ApplyToVertices(FMesh* Mesh, TArrayView<const FReshapeVertexBindingData> BindingData, const FShapeMeshDescriptorApply& Shape, bool bSkipNormalReshape)
	{
		check(Mesh);
		check(Mesh->GetVertexCount() == BindingData.Num());
		
		MeshBufferIterator<EMeshBufferFormat::Float32, float, 3> PositionIter(Mesh->GetVertexBuffers(), EMeshBufferSemantic::Position);
		TArrayView<FVector3f> Positions = TArrayView<FVector3f>(reinterpret_cast<FVector3f*>(PositionIter.ptr()), Mesh->GetVertexCount());

		const UntypedMeshBufferIterator NormalIter(Mesh->GetVertexBuffers(), EMeshBufferSemantic::Normal);
		const UntypedMeshBufferIterator TangentIter(Mesh->GetVertexBuffers(), EMeshBufferSemantic::Tangent);
		const UntypedMeshBufferIterator BiNormalIter(Mesh->GetVertexBuffers(), EMeshBufferSemantic::Binormal);

        const EMeshBufferFormat NormalFormat   = NormalIter.GetFormat();
        const EMeshBufferFormat TangentFormat  = TangentIter.GetFormat();
        const EMeshBufferFormat BiNormalFormat = BiNormalIter.GetFormat();
	
        const int32 NormalComps   = NormalIter.GetComponents();
        const int32 TangentComps  = TangentIter.GetComponents();
        const int32 BiNormalComps = BiNormalIter.GetComponents();

#if DO_CHECK
		// checking if the Base shape has more triangles than the target shape
		const bool bTriangleOutOfScopeFound = Algo::AnyOf(BindingData, 
			[NumShapeTriangles = Shape.Triangles.Num()](const FReshapeVertexBindingData& B) { return B.Triangle >= NumShapeTriangles; });

		if (bTriangleOutOfScopeFound)
		{
			UE_LOG(LogMutableCore, Warning, TEXT("Performing a Mesh Reshape where base shape and target shape do not have the same number of triangles."));
		}	
#endif

		const int32 ShapeTriangleCount = Shape.Triangles.Num();
		const int32 ShapePositionCount = Shape.Positions.Num();

		const int32 MeshVertexCount = BindingData.Num();

		const bool bComputeNormal = bSkipNormalReshape && NormalIter.ptr();
		for (int32 VertexIndex = 0; VertexIndex < MeshVertexCount; ++VertexIndex)
		{
			const FReshapeVertexBindingData& Binding = BindingData[VertexIndex];

			const bool bModified = (Binding.Triangle >= 0) & (Binding.Triangle < ShapeTriangleCount);
			if (bModified)
			{
				FVector3f NewPosition;
				FVector3f NewNormalPosition;
				
				GetDeform(Shape, Binding, NewPosition, NewNormalPosition);

				const FVector3f& OldPosition = Positions[VertexIndex];
  
				if ((OldPosition - Binding.AttachmentPoint).IsNearlyZero())
				{
					Positions[VertexIndex] += (NewPosition - OldPosition) * Binding.Weight;
				}
				else
				{
					Positions[VertexIndex] += (NewPosition - Binding.AttachmentPoint) * Binding.Weight;
				}

				if (bComputeNormal)
				{
					const FVector3f OldNormal = (NormalIter + VertexIndex).GetAsVec3f();
					const FVector3f OldNormalPosition = OldPosition + OldNormal;
					
					FVector3f NormalDisplacement = (NewNormalPosition - OldNormalPosition) * Binding.Weight;

					FVector3f Normal = ((OldNormalPosition + NormalDisplacement) - NewPosition).GetSafeNormal();
					FVector3f Tangent = TangentIter.ptr() ? (TangentIter + VertexIndex).GetAsVec3f() : FVector3f::XAxisVector;
					FVector3f BiNormal = BiNormalIter.ptr() ? (BiNormalIter + VertexIndex).GetAsVec3f() : FVector3f::YAxisVector;
				
					OrthogonalizeTangentSpace(
							&Normal, 
							TangentIter.ptr() ? &Tangent : nullptr, 
							BiNormalIter.ptr() ? &BiNormal : nullptr,
							BiNormalIter.ptr() ? ComputeTangentBasisDeterminantSign(OldNormal, Tangent, BiNormal) : 0.0f);
					
					// Leave the tangent basis sign untouched for packed normals formats.
					uint8 * const NormalElemPtr = (NormalIter + VertexIndex).ptr();
					for (int32 C = 0; C < NormalComps && C < 3; ++C)
					{
						ConvertData(C, NormalElemPtr, NormalFormat, &Normal, EMeshBufferFormat::Float32);
					}

					if (TangentIter.ptr())
					{
						uint8 * const TangentElemPtr = (TangentIter + VertexIndex).ptr();
						for (int32 C = 0; C < TangentComps && C < 3; ++C)
						{
							ConvertData(C, TangentElemPtr, TangentFormat, &Tangent, EMeshBufferFormat::Float32);
						}
					}
					
					if (BiNormalIter.ptr())
					{
						uint8 * const BiNormalElemPtr = (BiNormalIter + VertexIndex).ptr();
						for (int32 C = 0; C < BiNormalComps && C < 3; ++C)
						{
							ConvertData(C, BiNormalElemPtr, BiNormalFormat, &BiNormal, EMeshBufferFormat::Float32);
						}
					}
				}
			}
		}
	}

	inline void ApplyToPose(FMesh* Result, 
			TArrayView<const FReshapePointBindingData> BindingData, 
			TArrayView<const int32> BoneIndices,
			const FShapeMeshDescriptorApply& Shape)
	{
		check(Result);

#if DO_CHECK
		// checking if the Base shape has more triangles than the target shape
		const bool bTriangleOutOfScopeFound = Algo::AnyOf(BindingData, 
			[NumShapeTriangles = Shape.Triangles.Num()](const FReshapePointBindingData& B) { return B.Triangle >= NumShapeTriangles; });

		if (bTriangleOutOfScopeFound)
		{
			UE_LOG(LogMutableCore, Warning, TEXT("Performing a Mesh Reshape where base shape and target shape do not have the same number of triangles."));
		}	
#endif

		const int32 NumShapeTris = Shape.Triangles.Num();
		const int32 NumBoneIndices = BoneIndices.Num();
		for (int32 BoneSelectionIndex = 0; BoneSelectionIndex < NumBoneIndices; ++BoneSelectionIndex)
		{
			int32 BoneIndex = BoneIndices[BoneSelectionIndex];

			const FReshapePointBindingData& Binding = BindingData[BoneSelectionIndex];

			check(!EnumHasAnyFlags(Result->BonePoses[BoneIndex].BoneUsageFlags, EBoneUsageFlags::Root));

			FTransform3f& T = Result->BonePoses[BoneIndex].BoneTransform;

			FVector3f NewPosition(ForceInitToZero);

			const bool bModified = (Binding.Triangle >= 0) & (Binding.Triangle < NumShapeTris);
			
			if (bModified)
			{
				GetDeform(Shape, Binding, NewPosition);
			}

			const bool bHasChanged = bModified && FVector3f::DistSquared(NewPosition, T.GetLocation()) > UE_SMALL_NUMBER;
			// Only set it if has actually moved.
			if (bHasChanged)
			{	
				// Mark as reshaped. 
				EnumAddFlags(Result->BonePoses[BoneIndex].BoneUsageFlags, EBoneUsageFlags::Reshaped);

				// TODO: Review if the rotation also needs to be applied.
				T.SetLocation(FMath::Lerp(T.GetLocation(), NewPosition, Binding.Weight));
			}
		}
	}

	inline void ApplyToPhysicsBodies(
			FPhysicsBody& PBody, int32& InOutNumProcessedBindPoints, 
			const FMesh& BaseMesh, 
			TArrayView<const FReshapePointBindingData> BindingData, 
			TArrayView<const int32> UsedIndices, 
			const FShapeMeshDescriptorApply& Shape)
	{
#if DO_CHECK
		// checking if the Base shape has more triangles than the target shape
		const bool bTriangleOutOfScopeFound = Algo::AnyOf(BindingData, 
			[NumShapeTriangles = Shape.Triangles.Num()](const FReshapePointBindingData& B) { return B.Triangle >= NumShapeTriangles; });

		if (bTriangleOutOfScopeFound)
		{
			UE_LOG(LogMutableCore, Warning, TEXT("Performing a Mesh Reshape where base shape and target shape do not have the same number of triangles."));
		}
#endif
		
		bool bAnyModified = false;

		// Retrieve them in the same order the boxes where put in, so they can be linked to the physics body volumes.
		for ( const int32 B : UsedIndices )
		{	
			int32 BoneIndex = BaseMesh.FindBonePose(PBody.GetBodyBoneId(B));
			FTransform3f BoneTransform = FTransform3f::Identity;
			if (BoneIndex >= 0)
			{
				BaseMesh.GetBonePoseTransform(BoneIndex, BoneTransform);
			}
			
			FTransform3f InvBoneTransform = BoneTransform.Inverse();

			const int32 NumSpheres = PBody.GetSphereCount(B); 
			for ( int32 I = 0; I < NumSpheres; ++I )
			{
				FVector3f P;
				float R;

				TStaticArray<FVector3f, 6> Points; 
				const bool bDeformed = GetDeformedPoints(Shape, &BindingData[InOutNumProcessedBindPoints], Points);

				if (bDeformed)
				{
					ComputeSphereFromDeformedPoints(Points, P, R, InvBoneTransform);
					PBody.SetSphere(B, I, P, R);
					bAnyModified = true;
				}

				InOutNumProcessedBindPoints += Points.Num();
			}

			const int32 NumBoxes = PBody.GetBoxCount(B);
			for ( int32 I = 0; I < NumBoxes; ++I )
			{
				TStaticArray<FVector3f, 14> Points;
				const bool bDeformed = GetDeformedPoints(Shape, &BindingData[InOutNumProcessedBindPoints], Points);
				
				if (bDeformed)
				{
					FVector3f P;
					FQuat4f Q;
					FVector3f S;
					
					ComputeBoxFromDeformedPoints(Points, P, Q, S, InvBoneTransform);
					PBody.SetBox(B, I, P, Q, S);
					bAnyModified = true;
				}

				InOutNumProcessedBindPoints += Points.Num();
			}
			
			const int32 NumSphyls = PBody.GetSphylCount(B);
			for ( int32 I = 0; I < NumSphyls; ++I )
			{
				TStaticArray<FVector3f, 14> Points;
				const bool bDeformed = GetDeformedPoints(Shape, &BindingData[InOutNumProcessedBindPoints], Points);

				if ( bDeformed )
				{
					FVector3f P;
					FQuat4f Q;
					float R;
					float L;						

					ComputeSphylFromDeformedPoints(Points, P, Q, R, L, InvBoneTransform);
					PBody.SetSphyl(B, I, P, Q, R, L);
					bAnyModified = true;
				}

				InOutNumProcessedBindPoints += Points.Num();
			}

			const int32 NumTaperedCapsules = PBody.GetTaperedCapsuleCount(B);
			for ( int32 I = 0; I < NumTaperedCapsules; ++I )
			{
				TStaticArray<FVector3f, 14> Points;
				const bool bDeformed = GetDeformedPoints(Shape, &BindingData[InOutNumProcessedBindPoints], Points);

				if ( bDeformed )
				{
					FVector3f P;
					FQuat4f Q;
					float R0;
					float R1;
					float L;
				
					ComputeTaperedCapsuleFromDeformedPoints(Points, P, Q, R0, R1, L, InvBoneTransform);
					PBody.SetTaperedCapsule(B, I, P, Q, R0, R1, L);
					bAnyModified = true;
				}
				
				InOutNumProcessedBindPoints += Points.Num();
			}

			const int32 NumConvex = PBody.GetConvexCount(B);
			for ( int32 I = 0; I < NumConvex; ++I )
			{
				TArrayView<FVector3f> VerticesView;
				TArrayView<int32> IndicesView;
				PBody.GetConvexMeshView(B, I, VerticesView, IndicesView);

				FTransform3f ConvexTransform;
				PBody.GetConvexTransform(B, I, ConvexTransform);

				GetDeformedConvex(Shape, &BindingData[InOutNumProcessedBindPoints], VerticesView);

				FTransform3f InvConvexT = InvBoneTransform * ConvexTransform.Inverse();
				for ( FVector3f& V : VerticesView )
				{
					V = InvConvexT.TransformPosition( V );				
				}
				
				InOutNumProcessedBindPoints += VerticesView.Num();
				bAnyModified = true;
			}
		}

		PBody.bBodiesModified = bAnyModified;
	}

	inline void ApplyToAllPhysicsBodies(
		FMesh& OutNewMesh, const FMesh& BaseMesh, 
		TArrayView<const FReshapePointBindingData> BindingData,
		TArrayView<const int32> UsedIndices,
		TArrayView<const int32> BodyOffsets,
		const FShapeMeshDescriptorApply& Shape)
	{
		if (!BodyOffsets.Num())
		{
			return; // Nothing to do.
		}

		int32 NumProcessedBindPoints = 0;

		auto ApplyPhysicsBody = [&](FPhysicsBody& OutBody, int32 IndicesBegin, int32 IndicesEnd) -> void
		{
			TArrayView<const int32> BodyUsedIndices(UsedIndices.GetData() + IndicesBegin, IndicesEnd - IndicesBegin);

			ApplyToPhysicsBodies(OutBody, NumProcessedBindPoints, BaseMesh, BindingData, BodyUsedIndices, Shape);
		};


		check(BodyOffsets.Num() > 1);
		check(OutNewMesh.AdditionalPhysicsBodies.Num() + 1 == BodyOffsets.Num() - 1);
		const int32 PhysicsBodiesNum = BodyOffsets.Num() - 1;
		
		// Apply main physics body
		if (BaseMesh.PhysicsBody)
		{
			TSharedPtr<FPhysicsBody> NewBody = BaseMesh.PhysicsBody->Clone();
			ApplyPhysicsBody(*NewBody, 0, BodyOffsets[1]);
			OutNewMesh.PhysicsBody = NewBody;
		}


		// Apply additional physics bodies
		for (int32 I = 1; I < PhysicsBodiesNum; ++I)
		{	
			TSharedPtr<FPhysicsBody> NewBody = BaseMesh.AdditionalPhysicsBodies[I - 1]->Clone();
			ApplyPhysicsBody(*NewBody, BodyOffsets[I], BodyOffsets[I + 1]);
			OutNewMesh.AdditionalPhysicsBodies[I - 1] = NewBody;
		}

		check(NumProcessedBindPoints == BindingData.Num());	
	}

	//---------------------------------------------------------------------------------------------
	//! Rebuild the (previously bound) mesh data for a new shape.
	//! Proof-of-concept implementation.
	//---------------------------------------------------------------------------------------------
	inline void MeshApplyShape(FMesh* Result, const FMesh* BaseMesh, const FMesh* ShapeMesh, EMeshBindShapeFlags BindFlags, bool& bOutSuccess)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshApplyReshape);

		bOutSuccess = true;

		if (!BaseMesh)
		{
			bOutSuccess = false;
			return;
		}
		
		const bool bReshapeVertices = EnumHasAnyFlags(BindFlags, EMeshBindShapeFlags::ReshapeVertices);
		const bool bRecomputeNormals = EnumHasAnyFlags(BindFlags, EMeshBindShapeFlags::RecomputeNormals);
		const bool bReshapeSkeleton = EnumHasAnyFlags(BindFlags, EMeshBindShapeFlags::ReshapeSkeleton);
		const bool bReshapePhysicsVolumes = EnumHasAnyFlags(BindFlags, EMeshBindShapeFlags::ReshapePhysicsVolumes);
		const bool bApplyLaplacian = EnumHasAnyFlags(BindFlags, EMeshBindShapeFlags::ApplyLaplacian);

		// Early out if nothing will be modified and the vertices discarted.
		const bool bSkeletonModification = BaseMesh->GetSkeleton() && bReshapeSkeleton;
		const bool bPhysicsModification = (BaseMesh->GetPhysicsBody() || BaseMesh->AdditionalPhysicsBodies.Num()) && bReshapePhysicsVolumes;

		if (!bReshapeVertices && !bSkeletonModification && !bPhysicsModification)
		{
			bOutSuccess = false;
			return;
		}
	
		// \TODO: Multiple binding data support
		int32 BindingDataIndex = 0;

		// If the base mesh has no binding data, just clone it.
		int32 BarycentricDataBuffer = 0;
		int32 BarycentricDataChannel = 0;
		const FMeshBufferSet& VB = BaseMesh->GetVertexBuffers();
		VB.FindChannel(EMeshBufferSemantic::BarycentricCoords, BindingDataIndex, &BarycentricDataBuffer, &BarycentricDataChannel);
		
		TSharedPtr<FMesh> TempMesh = nullptr;
		FMesh* VerticesReshapeMesh = Result;
		
		if (bApplyLaplacian)
		{
			TempMesh = MakeShared<FMesh>();
			VerticesReshapeMesh = TempMesh.Get();
		}

		// Copy Without VertexBuffers or AdditionalBuffers
		constexpr EMeshCopyFlags CopyFlags = ~(EMeshCopyFlags::WithVertexBuffers | EMeshCopyFlags::WithAdditionalBuffers);
		VerticesReshapeMesh->CopyFrom(*BaseMesh, CopyFlags);
	
		FMeshBufferSet& ResultBuffers = VerticesReshapeMesh->GetVertexBuffers();

		check(ResultBuffers.Buffers.Num() == 0);

		// Copy buffers skipping binding data. 
		ResultBuffers.ElementCount = VB.ElementCount;
		// Remove one element to the number of buffers if BarycentricDataBuffer found. 
		ResultBuffers.Buffers.SetNum(FMath::Max(0, VB.Buffers.Num() - int32(BarycentricDataBuffer >= 0)));
		
		for (int32 B = 0, R = 0; B < VB.Buffers.Num(); ++B)
		{
			if (B != BarycentricDataBuffer)
			{
				ResultBuffers.Buffers[R++] = VB.Buffers[B];
			}
		}

		// Copy the additional buffers skipping binding data.
		VerticesReshapeMesh->AdditionalBuffers.Reserve(BaseMesh->AdditionalBuffers.Num());
		for (const TPair<EMeshBufferType, FMeshBufferSet>& A : BaseMesh->AdditionalBuffers)
		{	
			const bool bIsBindOpBuffer = 
					A.Key == EMeshBufferType::SkeletonDeformBinding || 
					A.Key == EMeshBufferType::PhysicsBodyDeformBinding ||
					A.Key == EMeshBufferType::PhysicsBodyDeformSelection ||
					A.Key == EMeshBufferType::PhysicsBodyDeformOffsets;
	
			if (!bIsBindOpBuffer)
			{
				VerticesReshapeMesh->AdditionalBuffers.Add(A);
			}
		}
		if (!ShapeMesh)
		{
			return;
		}

		int32 ShapeVertexCount = ShapeMesh->GetVertexCount();
		int32 ShapeTriangleCount = ShapeMesh->GetFaceCount();
		if (!ShapeVertexCount || !ShapeTriangleCount)
		{	
			return;
		}

		// Generate the temp vertex query data for the shape
		// TODO: Vertex data copy may be saved in most cases.
		FShapeMeshDescriptorApply ShapeDescriptor;
		{
			const UntypedMeshBufferIteratorConst ItPosition(ShapeMesh->GetVertexBuffers(), EMeshBufferSemantic::Position);
			ShapeDescriptor.Positions = TArrayView<const FVector3f>(reinterpret_cast<const FVector3f*>(ItPosition.ptr()), ShapeVertexCount);


			const UntypedMeshBufferIteratorConst ItNormal(ShapeMesh->GetVertexBuffers(), EMeshBufferSemantic::Normal);
			ShapeDescriptor.Normals.SetNumUninitialized(ShapeVertexCount);
			
			if (ItNormal.GetFormat() == EMeshBufferFormat::PackedDirS8_W_TangentSign)
			{
				for (int32 ShapeVertexIndex = 0; ShapeVertexIndex < ShapeVertexCount; ++ShapeVertexIndex)
				{
					const FPackedNormal* PackedNormal = reinterpret_cast<const FPackedNormal*>((ItNormal + ShapeVertexIndex).ptr());
					ShapeDescriptor.Normals[ShapeVertexIndex] = PackedNormal->ToFVector3f();
				}
			}
			else
			{
				for (int32 ShapeVertexIndex = 0; ShapeVertexIndex < ShapeVertexCount; ++ShapeVertexIndex)
				{
					ShapeDescriptor.Normals[ShapeVertexIndex] = (ItNormal + ShapeVertexIndex).GetAsVec3f();
				}
			}
		}

		// Generate the temp face query data for the shape
		// TODO: Index data copy may be saved in most cases.

		TArray<UE::Geometry::FIndex3i> FormattedTriangles;

		{
			const UntypedMeshBufferIteratorConst ItIndices(ShapeMesh->GetIndexBuffers(), EMeshBufferSemantic::VertexIndex);
			if (ItIndices.GetFormat() == EMeshBufferFormat::UInt32)
			{
				ShapeDescriptor.Triangles = TArrayView<const UE::Geometry::FIndex3i>(reinterpret_cast<const UE::Geometry::FIndex3i*>(ItIndices.ptr()), ShapeTriangleCount);
			}
			else
			{
				FormattedTriangles.SetNumUninitialized(ShapeTriangleCount);
				for (int32 TriangleIndex = 0; TriangleIndex < ShapeTriangleCount; ++TriangleIndex)
				{
					UE::Geometry::FIndex3i Triangle;
					Triangle.A = int32((ItIndices + TriangleIndex * 3 + 0).GetAsUINT32());
					Triangle.B = int32((ItIndices + TriangleIndex * 3 + 1).GetAsUINT32());
					Triangle.C = int32((ItIndices + TriangleIndex * 3 + 2).GetAsUINT32());

					FormattedTriangles[TriangleIndex] = Triangle;
				}
				ShapeDescriptor.Triangles = TArrayView<const UE::Geometry::FIndex3i>(FormattedTriangles);
			}
		}

		if (bReshapeVertices && BarycentricDataBuffer >= 0)
		{
			{
				MUTABLE_CPUPROFILER_SCOPE(ReshapeVertices);
				// \TODO: More checks
				check(BarycentricDataChannel == 0);
				check(VB.GetElementSize(BarycentricDataBuffer) == (int)sizeof(FReshapeVertexBindingData));

				TArrayView<const FReshapeVertexBindingData> VerticesBindingData(
						(const FReshapeVertexBindingData*)VB.GetBufferData(BarycentricDataBuffer),
						VB.GetElementCount());

				ApplyToVertices(VerticesReshapeMesh, VerticesBindingData, ShapeDescriptor, bRecomputeNormals);
			}

			if (bApplyLaplacian)
			{
				check(Result && VerticesReshapeMesh);
				// check result is empty at this point.
				check(Result->GetVertexCount() == 0 && Result->GetIndexCount() == 0); 
				SmoothMeshLaplacian(*Result, *VerticesReshapeMesh);
			}

			if (bRecomputeNormals)
			{
				ComputeMeshNormals(*Result);
			}
		}
	
		if (bReshapeSkeleton)
		{
			MUTABLE_CPUPROFILER_SCOPE(ReshapeSkeleton);

			// If the base mesh has no binding data for the skeleton don't do anything.
			const FMeshBufferSet* SkeletonBindBuffer = nullptr;
			for ( const TPair<EMeshBufferType, FMeshBufferSet>& A : BaseMesh->AdditionalBuffers )
			{
				if ( A.Key == EMeshBufferType::SkeletonDeformBinding )
				{
					SkeletonBindBuffer = &A.Value;
					break;
				}
			}
			
			BarycentricDataBuffer = -1;
			if (SkeletonBindBuffer)
			{
				SkeletonBindBuffer->FindChannel(EMeshBufferSemantic::BarycentricCoords, BindingDataIndex, &BarycentricDataBuffer, &BarycentricDataChannel);
			}

			if (SkeletonBindBuffer && BarycentricDataBuffer >= 0)
			{
				// \TODO: More checks
				check(BarycentricDataChannel == 0);
				check(SkeletonBindBuffer && SkeletonBindBuffer->GetElementSize(BarycentricDataBuffer) == (int)sizeof(FReshapePointBindingData));

				TArrayView<const FReshapePointBindingData> SkeletonBindingData( 
						(const FReshapePointBindingData*)SkeletonBindBuffer->GetBufferData(BarycentricDataBuffer),
						SkeletonBindBuffer->GetElementCount());
			
				check(SkeletonBindBuffer->GetBufferCount() >= 2);
				TArrayView<const int32> BoneIndices( 
						(const int32*)SkeletonBindBuffer->GetBufferData(1), SkeletonBindBuffer->GetElementCount());

				ApplyToPose(Result, SkeletonBindingData, BoneIndices, ShapeDescriptor);
			}
		}

		// When transforming the physics volumes, the resulting pose of of the skeleton reshape operation will be used, so 
		// order of operation is important.

		// Transform physics volumes based on the deformed sampling points.
		const FPhysicsBody* OldPhysicsBody = Result->PhysicsBody.Get();
		const int32 AdditionalPhysicsBodiesNum = Result->AdditionalPhysicsBodies.Num();

		if (bReshapePhysicsVolumes && (OldPhysicsBody || AdditionalPhysicsBodiesNum))
		{	
			MUTABLE_CPUPROFILER_SCOPE(ReshapePhysicsBodies);
			
			using BufferEntryType = TPair<EMeshBufferType, FMeshBufferSet>;
			const BufferEntryType* FoundPhysicsBindBuffer = BaseMesh->AdditionalBuffers.FindByPredicate(
					[](BufferEntryType& E){ return E.Key == EMeshBufferType::PhysicsBodyDeformBinding; });

			const BufferEntryType* FoundPhysicsBindSelectionBuffer = BaseMesh->AdditionalBuffers.FindByPredicate(
					[](BufferEntryType& E){ return E.Key == EMeshBufferType::PhysicsBodyDeformSelection; });
	
			const BufferEntryType* FoundPhysicsBindOffsetsBuffer = BaseMesh->AdditionalBuffers.FindByPredicate(
					[](BufferEntryType& E){ return E.Key == EMeshBufferType::PhysicsBodyDeformOffsets; });

			BarycentricDataBuffer = -1;
			if (FoundPhysicsBindBuffer)
			{
				FoundPhysicsBindBuffer->Value.FindChannel(EMeshBufferSemantic::BarycentricCoords, BindingDataIndex, &BarycentricDataBuffer, &BarycentricDataChannel);
			}
			
			const bool bAllNeededBuffersFound =
				FoundPhysicsBindBuffer && FoundPhysicsBindSelectionBuffer && FoundPhysicsBindOffsetsBuffer && BarycentricDataBuffer >= 0;

			if (bAllNeededBuffersFound)
			{
				const FMeshBufferSet& PhysicsBindBuffer = FoundPhysicsBindBuffer->Value;
				const FMeshBufferSet& PhyiscsBindSelectionBuffer = FoundPhysicsBindSelectionBuffer->Value;
				const FMeshBufferSet& PhyiscsBindOffsetsBuffer = FoundPhysicsBindOffsetsBuffer->Value;

				// \TODO: More checks
				check(BarycentricDataChannel == 0);
				check(PhysicsBindBuffer.GetElementSize(BarycentricDataBuffer) == (int)sizeof(FReshapePointBindingData));
					
				TArrayView<const FReshapePointBindingData> BindingData(  
						(const FReshapePointBindingData*)PhysicsBindBuffer.GetBufferData(BarycentricDataBuffer),
						PhysicsBindBuffer.GetElementCount() );

				TArrayView<const int32> UsedIndices( 
						(const int32*)PhyiscsBindSelectionBuffer.GetBufferData(0), 
					    PhyiscsBindSelectionBuffer.GetElementCount());

				TArrayView<const int32> Offsets(
						(const int32*)PhyiscsBindOffsetsBuffer.GetBufferData(0), 
					    PhyiscsBindOffsetsBuffer.GetElementCount());

				ApplyToAllPhysicsBodies(*Result, *BaseMesh, BindingData, UsedIndices, Offsets, ShapeDescriptor);
			}
		}
	}
}
