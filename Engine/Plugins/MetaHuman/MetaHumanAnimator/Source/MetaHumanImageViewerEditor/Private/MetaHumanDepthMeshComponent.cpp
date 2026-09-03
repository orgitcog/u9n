// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDepthMeshComponent.h"
#include "CameraCalibration.h"

#include "Utils/CustomMaterialUtils.h"

#include "OpenCVHelperLocal.h"

static void AddVertex(float InX, float InY, float InWidth, float InHeight,
					  TArray<FVector>& OutVertices, TArray<int32>& OutTriangles, TArray<FVector>& OutNormals, TArray<FVector2D>& OutUV0)
{
	OutTriangles.Push(OutVertices.Num());
	OutVertices.Push(FVector(InX, InY, 0));
	OutNormals.Push(FVector(0, 0, 1));
	OutUV0.Push(FVector2D(InX / InWidth, InY / InHeight));
}

static void AddQuad(float InX, float InY, float InTriangleSize, float InWidth, float InHeight,
					TArray<FVector>& OutVertices, TArray<int32>& OutTriangles, TArray<FVector>& OutNormals, TArray<FVector2D>& OutUV0)
{
	AddVertex(InX, InY, InWidth, InHeight, OutVertices, OutTriangles, OutNormals, OutUV0);
	AddVertex(InX, InY + InTriangleSize, InWidth, InHeight, OutVertices, OutTriangles, OutNormals, OutUV0);
	AddVertex(InX + InTriangleSize, InY, InWidth, InHeight, OutVertices, OutTriangles, OutNormals, OutUV0);

	AddVertex(InX, InY + InTriangleSize, InWidth, InHeight, OutVertices, OutTriangles, OutNormals, OutUV0);
	AddVertex(InX + InTriangleSize, InY + InTriangleSize, InWidth, InHeight, OutVertices, OutTriangles, OutNormals, OutUV0);
	AddVertex(InX + InTriangleSize, InY, InWidth, InHeight, OutVertices, OutTriangles, OutNormals, OutUV0);
}

UMetaHumanDepthMeshComponent::UMetaHumanDepthMeshComponent(const FObjectInitializer& InObjectInitializer)
	: Super{ InObjectInitializer }
{
}

void UMetaHumanDepthMeshComponent::OnRegister()
{
	Super::OnRegister();

	SetMaterial(0, CustomMaterialUtils::CreateDepthMeshMaterial(TEXT("Depth Mesh Material")));
	
	UpdateMaterialTexture();
	UpdateMaterialDepth();
	SetCameraCalibration(CameraCalibration.Get());
}

void UMetaHumanDepthMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDepthMeshComponent, DepthNear) || PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDepthMeshComponent, DepthFar))
	{
		UpdateMaterialDepth();
		SetDepthPlaneTransform();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanDepthMeshComponent, DepthTexture))
	{
		UpdateMaterialTexture();
	}
}

FBoxSphereBounds UMetaHumanDepthMeshComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	// Extend BB to account for the WPO material applied to the mesh. Prevents object being frustum culled too early

	FBoxSphereBounds Plane = UProceduralMeshComponent::CalcBounds(InLocalToWorld);

	Plane.BoxExtent /= BoundsScale;

	FBox PlaneBox = Plane.GetBox();

	FBox ExtrudedBox = FBox(FVector(0, PlaneBox.Min.Y, PlaneBox.Min.Z), PlaneBox.Max);

	FBoxSphereBounds Extruded = FBoxSphereBounds(ExtrudedBox);

	Extruded.BoxExtent *= BoundsScale;
	Extruded.SphereRadius *= BoundsScale;

	return Extruded;
}

void UMetaHumanDepthMeshComponent::SetDepthTexture(UTexture* InDepthTexture)
{
	DepthTexture = InDepthTexture;
	UpdateMaterialTexture();		
}

void UMetaHumanDepthMeshComponent::SetCameraCalibration(UCameraCalibration* InCameraCalibration)
{
	CameraCalibration = InCameraCalibration;
	UpdateMaterialCameraIntrinsics();
}

void UMetaHumanDepthMeshComponent::SetDepthRange(float InDepthNear, float InDepthFar)
{
	DepthNear = InDepthNear;
	DepthFar = InDepthFar;

	UpdateMaterialDepth();
	SetDepthPlaneTransform();
}

void UMetaHumanDepthMeshComponent::SetSize(int32 InWidth, int32 InHeight)
{
	if (InWidth != Width || InHeight != Height)
	{
		Width = InWidth;
		Height = InHeight;

		ClearMeshSection(0);

		if (Width > 0 && Height > 0)
		{
			const int32 TriangleSize = 4;
			const int32 OverestimatedNumberOfVertices = 6 * ((InWidth / TriangleSize) + 1) * ((InHeight / TriangleSize) + 1);

			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UV0;

			Vertices.Reserve(OverestimatedNumberOfVertices);
			Triangles.Reserve(OverestimatedNumberOfVertices);
			Normals.Reserve(OverestimatedNumberOfVertices);
			UV0.Reserve(OverestimatedNumberOfVertices);

			for (int32 X = 0; X < InWidth; X += TriangleSize)
			{
				for (int32 Y = 0; Y < InHeight; Y += TriangleSize)
				{
					AddQuad(X, Y, TriangleSize, InWidth, InHeight, Vertices, Triangles, Normals, UV0);
				}
			}

			CreateMeshSection(0, Vertices, Triangles, Normals, UV0, {}, {}, {}, {}, {}, false);
		}

		MarkRenderStateDirty();
	}
}

void UMetaHumanDepthMeshComponent::UpdateMaterialDepth()
{
	if (UMaterialInstanceDynamic* DepthMaterial = Cast<UMaterialInstanceDynamic>(GetMaterial(0)))
	{
		DepthMaterial->SetScalarParameterValue(TEXT("DepthNear"), DepthNear);
		DepthMaterial->SetScalarParameterValue(TEXT("DepthFar"), DepthFar);
	}
}

void UMetaHumanDepthMeshComponent::SetDepthPlaneTransform(bool bInNotifyMaterial)
{
	if (CameraCalibration)
	{
		TArray<FCameraCalibration> Calibrations;
		TArray<TPair<FString, FString>> StereoPairs;
		CameraCalibration->ConvertToTrackerNodeCameraModels(Calibrations, StereoPairs);

		const FCameraCalibration* DepthCalibrationPtr = Calibrations.FindByPredicate([](const FCameraCalibration& InCalibration)
		{
			return InCalibration.CameraType == FCameraCalibration::Depth;
		});

		if (!DepthCalibrationPtr)
		{
			return;
		}

		const FCameraCalibration& DepthCalibration = *DepthCalibrationPtr;

		FTransform Transform = FTransform(FVector(-DepthCalibration.PrincipalPoint.X, -DepthCalibration.PrincipalPoint.Y, 0)); // center mesh on principle point
		const float DesiredDistance = DepthFar; // scale so that when object is placed at the desired distance from camera it fills the fov
		const float DistanceScale = DepthCalibration.FocalLength.X / DesiredDistance;
		Transform *= FTransform(FRotator(0), FVector(0), FVector(1.0 / DistanceScale, 1.0 / DistanceScale, 1));
		Transform *= FTransform(FRotator(0, 90, 0)); // rotate 90 about Z axis
		Transform *= FTransform(FRotator(90, 0, 0)); // rotate 90 about Y axis
		Transform *= FTransform(FVector(DesiredDistance, 0, 0)); // translate along X axis

		// Calculate the inverse of the camera extrinsic matrix
		const FMatrix InvCamMatrix = DepthCalibration.Transform.Inverse();
		FTransform InverseCameraExtrinsics{ InvCamMatrix };
		FOpenCVHelperLocal::ConvertOpenCVToUnreal(InverseCameraExtrinsics);
		Transform *= InverseCameraExtrinsics;

		if (UMaterialInstanceDynamic* DepthMaterial = Cast<UMaterialInstanceDynamic>(GetMaterial(0)))
		{
			DepthMaterial->SetVectorParameterValue(TEXT("InvExtrinsicRow0"), FVector4{ InvCamMatrix.M[0][0], InvCamMatrix.M[0][1], InvCamMatrix.M[0][2], InvCamMatrix.M[0][3] });
			DepthMaterial->SetVectorParameterValue(TEXT("InvExtrinsicRow1"), FVector4{ InvCamMatrix.M[1][0], InvCamMatrix.M[1][1], InvCamMatrix.M[1][2], InvCamMatrix.M[1][3] });
			DepthMaterial->SetVectorParameterValue(TEXT("InvExtrinsicRow2"), FVector4{ InvCamMatrix.M[2][0], InvCamMatrix.M[2][1], InvCamMatrix.M[2][2], InvCamMatrix.M[2][3] });
			DepthMaterial->SetVectorParameterValue(TEXT("InvExtrinsicRow3"), FVector4{ InvCamMatrix.M[3][0], InvCamMatrix.M[3][1], InvCamMatrix.M[3][2], InvCamMatrix.M[3][3] });

			if (bInNotifyMaterial)
			{
				DepthMaterial->GetMaterial()->PostEditChange();
			}
		}

		SetRelativeTransform(Transform);
	}
}

void UMetaHumanDepthMeshComponent::UpdateMaterialTexture()
{
	if (DepthTexture)
	{
		if (UMaterialInstanceDynamic* DepthMaterial = Cast<UMaterialInstanceDynamic>(GetMaterial(0)))
		{
			DepthMaterial->SetTextureParameterValue(TEXT("Movie"), DepthTexture);
			DepthMaterial->GetMaterial()->PostEditChange();
		}
	}
}

void UMetaHumanDepthMeshComponent::UpdateMaterialCameraIntrinsics()
{
	if (CameraCalibration)
	{
		TArray<FCameraCalibration> Calibrations;
		TArray<TPair<FString, FString>> StereoPairs;
		CameraCalibration->ConvertToTrackerNodeCameraModels(Calibrations, StereoPairs);

		const FCameraCalibration* DepthCalibration = Calibrations.FindByPredicate([](const FCameraCalibration& InCalibration)
		{
			return InCalibration.CameraType == FCameraCalibration::Depth;
		});

		if (!DepthCalibration)
		{
			return;
		}

		// Set the depth material inverse camera intrinsic matrix
		FMatrix DepthCameraIntrinsic = FMatrix::Identity;
		DepthCameraIntrinsic.M[0][0] = DepthCalibration->FocalLength.X;
		DepthCameraIntrinsic.M[1][1] = DepthCalibration->FocalLength.Y;
		DepthCameraIntrinsic.M[0][2] = DepthCalibration->PrincipalPoint.X;
		DepthCameraIntrinsic.M[1][2] = DepthCalibration->PrincipalPoint.Y;

		const FMatrix InverseDepthCameraIntrinsic = DepthCameraIntrinsic.Inverse();
		
		if (UMaterialInstanceDynamic* DepthMaterial = Cast<UMaterialInstanceDynamic>(GetMaterial(0)))
		{
			DepthMaterial->SetScalarParameterValue(FName("InvFocal"), InverseDepthCameraIntrinsic.M[0][0]);
			DepthMaterial->SetScalarParameterValue(FName("InvX"), InverseDepthCameraIntrinsic.M[0][2]);
			DepthMaterial->SetScalarParameterValue(FName("InvY"), InverseDepthCameraIntrinsic.M[1][2]);
		}

		SetSize(DepthCalibration->ImageSize.X, DepthCalibration->ImageSize.Y);
		SetDepthPlaneTransform(true);
	}
}
