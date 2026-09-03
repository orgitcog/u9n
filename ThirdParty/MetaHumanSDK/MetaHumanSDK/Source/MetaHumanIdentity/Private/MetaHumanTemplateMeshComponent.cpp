// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanTemplateMeshComponent.h"

#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityLog.h"

#include "UObject/ConstructorHelpers.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/MeshNormals.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "DNAUtils.h"
#include "MaterialDomain.h"
#include "InterchangeDnaTranslator.h"

static inline TSet<int32> GetObjToUEVertexMapping(const FString& InObjFileName)
{
	const FString PluginContentDir = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir();
	const FString ObjFilePath = FString::Format(TEXT("{0}/MeshFitting/Template/{1}.obj"), { PluginContentDir, InObjFileName });

	TSet<int32> Indices;
	FFileHelper ObjParser;
	ObjParser.LoadFileToStringWithLineVisitor(*ObjFilePath, [&Indices](FStringView InView)
	{
		if (InView.Left(2).Equals("f "))
		{
			InView.RightChopInline(2);
			int32 IndexDash = INDEX_NONE;
			int32 IndexWhitespace = INDEX_NONE;
			int32 Itr = 0;

			while (Itr < 4)
			{
				InView.FindChar(TCHAR('/'), IndexDash);
				InView.FindChar(TCHAR(' '), IndexWhitespace);

				auto NumStrin = FString(InView.Left(IndexDash));
				int32 Ind = FCString::Atoi(*NumStrin);
				Indices.Add(Ind - 1);

				InView.RightChopInline(IndexWhitespace + 1);
				++Itr;
			}
		}
	});

	return Indices;
}

inline FVector UMetaHumanTemplateMeshComponent::ConvertVertex(const FVector& InVertex, ETemplateVertexConversion InConversionType)
{
	switch (InConversionType)
	{
		case ETemplateVertexConversion::ConformerToUE:
			return FVector{ InVertex.Z, InVertex.X, -InVertex.Y };

		case ETemplateVertexConversion::UEToConformer:
			return FVector{ InVertex.Y, -InVertex.Z, InVertex.X };

		case ETemplateVertexConversion::None:
		default:
			break;
	}

	return InVertex;
}

FTransform UMetaHumanTemplateMeshComponent::UEToRigSpaceTransform{ FRotator{ 180.0, -90.0, 0.0 } };

UMetaHumanTemplateMeshComponent::UMetaHumanTemplateMeshComponent()
	: bShowFittedTeeth{ true }
	, bShowEyes{ true }
	, bShowTeethMesh { true }
{
	HeadMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Template Head Mesh Component"));
	TeethMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Template Teeth Mesh Component"));
	LeftEyeComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Template Left Eye Mesh Component"));
	RightEyeComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Template Left Right Mesh Component"));
	OriginalTeethMesh = CreateDefaultSubobject<UDynamicMesh>(TEXT("Original Teeth Mesh"));
	FittedTeethMesh = CreateDefaultSubobject<UDynamicMesh>(TEXT("Fitted Teeth Mesh"));

	PoseHeadMeshes =
	{
		{ EIdentityPoseType::Neutral, CreateDefaultSubobject<UDynamicMesh>(TEXT("Neutral Head Mesh")) },
		{ EIdentityPoseType::Teeth, CreateDefaultSubobject<UDynamicMesh>(TEXT("Teeth Head Mesh")) }
	};

	HeadMeshComponent->SetupAttachment(this);
	TeethMeshComponent->SetupAttachment(this);
	LeftEyeComponent->SetupAttachment(this);
	RightEyeComponent->SetupAttachment(this);
}

void UMetaHumanTemplateMeshComponent::OnRegister()
{
	Super::OnRegister();

	HeadMeshComponent->RegisterComponent();
	TeethMeshComponent->RegisterComponent();
	LeftEyeComponent->RegisterComponent();
	RightEyeComponent->RegisterComponent();
}

void UMetaHumanTemplateMeshComponent::OnUnregister()
{
	Super::OnUnregister();

	HeadMeshComponent->UnregisterComponent();
	TeethMeshComponent->UnregisterComponent();
	LeftEyeComponent->UnregisterComponent();
	RightEyeComponent->UnregisterComponent();
}

void UMetaHumanTemplateMeshComponent::OnVisibilityChanged()
{
	Super::OnVisibilityChanged();

	LeftEyeComponent->SetVisibility(bShowEyes);
	RightEyeComponent->SetVisibility(bShowEyes);
	TeethMeshComponent->SetVisibility(bShowTeethMesh);
}

void UMetaHumanTemplateMeshComponent::LoadMaterialsForMeshes()
{
	static UMaterial* TemplateHeadMaterial = LoadObject<UMaterial>(nullptr, TEXT("/" UE_PLUGIN_NAME "/IdentityTemplate/M_MetaHumanIdentity_Head.M_MetaHumanIdentity_Head"));
	static UMaterial* TemplateTeethMaterial = LoadObject<UMaterial>(nullptr, TEXT("/" UE_PLUGIN_NAME "/IdentityTemplate/M_MetaHumanIdentity_Teeth.M_MetaHumanIdentity_Teeth"));
	static UMaterial* TemplateEyeMaterial = LoadObject<UMaterial>(nullptr, TEXT("/" UE_PLUGIN_NAME "/IdentityTemplate/M_MetaHumanIdentity_Eye.M_MetaHumanIdentity_Eye"));

	HeadMeshComponent->SetOverrideRenderMaterial(TemplateHeadMaterial);
	TeethMeshComponent->SetOverrideRenderMaterial(TemplateTeethMaterial);
	LeftEyeComponent->SetOverrideRenderMaterial(TemplateEyeMaterial);
	RightEyeComponent->SetOverrideRenderMaterial(TemplateEyeMaterial);
}

void UMetaHumanTemplateMeshComponent::LoadMeshAssets()
{
	LoadMaterialsForMeshes();

	HeadMeshComponent->GetDynamicMesh()->InitializeMesh();
	TeethMeshComponent->GetDynamicMesh()->InitializeMesh();
	LeftEyeComponent->GetDynamicMesh()->InitializeMesh();
	RightEyeComponent->GetDynamicMesh()->InitializeMesh();
	OriginalTeethMesh->InitializeMesh();
	FittedTeethMesh->InitializeMesh();

	bool bLoadedTemplateMeshFromDNA = false;
	const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME))->GetContentDir();
	const FString PathToDNA = PluginDir + TEXT("/IdentityTemplate/Face_Archetype.ardna");	

	TArray<uint8> DNADataAsBuffer;
	if (FFileHelper::LoadFileToArray(DNADataAsBuffer, *PathToDNA))
	{
		if (TSharedPtr<IDNAReader> DNAReader = ReadDNAFromBuffer(&DNADataAsBuffer, EDNADataLayer::All))
		{
			FMeshDescription HeadMeshDescription;
			FMeshDescription TeethMeshDescription;
			FMeshDescription EyeLeftMeshDescription;
			FMeshDescription EyeRightMeshDescription;

			// Populate mesh description from dna data. Mesh index for relevant geometries match layout in DNA file
			UInterchangeDnaTranslator::PopulateStaticMeshDescription(HeadMeshDescription, *DNAReader.Get(), 0);
			UInterchangeDnaTranslator::PopulateStaticMeshDescription(TeethMeshDescription, *DNAReader.Get(), 1);
			UInterchangeDnaTranslator::PopulateStaticMeshDescription(EyeLeftMeshDescription, *DNAReader.Get(), 3);
			UInterchangeDnaTranslator::PopulateStaticMeshDescription(EyeRightMeshDescription, *DNAReader.Get(), 4);

			FDynamicMesh3 HeadMesh;
			FDynamicMesh3 TeethMesh;
			FMeshDescriptionToDynamicMesh DynamicMeshConverter;

			DynamicMeshConverter.Convert(&HeadMeshDescription, HeadMesh);
			DynamicMeshConverter.Convert(&TeethMeshDescription, TeethMesh);
			DynamicMeshConverter.Convert(&EyeLeftMeshDescription, *LeftEyeComponent->GetMesh());
			DynamicMeshConverter.Convert(&EyeRightMeshDescription, *RightEyeComponent->GetMesh());

			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(HeadMesh);
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(TeethMesh);
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(*LeftEyeComponent->GetMesh());
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(*RightEyeComponent->GetMesh());

			// Initialize all pose head meshes so they start all in their default state
			for (TPair<EIdentityPoseType, TObjectPtr<UDynamicMesh>>& PoseHeadMeshPair : PoseHeadMeshes)
			{
				PoseHeadMeshPair.Value->SetMesh(HeadMesh);
			}

			// Initializes both teeth meshes so they start at their default state
			OriginalTeethMesh->SetMesh(TeethMesh);
			FittedTeethMesh->SetMesh(TeethMesh);

			// Set the neutral pose head mesh as the currently active head mesh
			HeadMeshComponent->GetDynamicMesh()->SetMesh(PoseHeadMeshes[EIdentityPoseType::Neutral]->GetMeshRef());
			TeethMeshComponent->GetDynamicMesh()->SetMesh(OriginalTeethMesh->GetMeshRef());

			bLoadedTemplateMeshFromDNA = true;
		}
	}

	if (!bLoadedTemplateMeshFromDNA)
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Failed to create a template mesh from the dna file:  %s"), *PathToDNA);
	}
}

#if WITH_EDITOR

void UMetaHumanTemplateMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	if (FProperty* Property = InPropertyChangedEvent.Property)
	{
		const FName PropertyName = *Property->GetName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bShowFittedTeeth))
		{
			TeethMeshComponent->GetDynamicMesh()->SetMesh(bShowFittedTeeth ? FittedTeethMesh->GetMeshRef() : OriginalTeethMesh->GetMeshRef());

			OnTemplateMeshChanged.Broadcast();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bShowEyes))
		{
			// Only set the visibility of eyes if the component itself is visible
			RightEyeComponent->SetVisibility(bShowEyes && IsVisible());
			LeftEyeComponent->SetVisibility(bShowEyes && IsVisible());

			OnTemplateMeshChanged.Broadcast();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bShowTeethMesh))
		{
			TeethMeshComponent->SetVisibility(bShowTeethMesh && IsVisible());
			OnTemplateMeshChanged.Broadcast();
		}
	}
}

#endif

FBoxSphereBounds UMetaHumanTemplateMeshComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	return HeadMeshComponent->Bounds.TransformBy(InLocalToWorld);
}

UDynamicMesh* UMetaHumanTemplateMeshComponent::GetPoseHeadMesh(EIdentityPoseType InPoseType) const
{
	check(PoseHeadMeshes.Contains(InPoseType));
	return PoseHeadMeshes[InPoseType];
}

void UMetaHumanTemplateMeshComponent::ShowHeadMeshForPose(EIdentityPoseType InPoseType) const
{
	check(PoseHeadMeshes.Contains(InPoseType));
	HeadMeshComponent->GetDynamicMesh()->SetMesh(PoseHeadMeshes[InPoseType]->GetMeshRef());

	OnTemplateMeshChanged.Broadcast();
}

void UMetaHumanTemplateMeshComponent::SetPoseHeadMeshVertices(EIdentityPoseType InPoseType, TConstArrayView<FVector3f> InNewVertices, ETemplateVertexConversion InConversionType) const
{
	UDynamicMesh* PoseHeadMesh = GetPoseHeadMesh(InPoseType);

	if (InNewVertices.Num() == PoseHeadMesh->GetMeshRef().VertexCount())
	{
		PoseHeadMesh->EditMesh([this, InNewVertices, InConversionType](FDynamicMesh3& InMesh3D)
		{
			for (int32 VertId = 0; VertId < InMesh3D.VertexCount(); VertId++)
			{
				const FVector3f& InputVertex = InNewVertices[VertId];
				const FVector Vertex = ConvertVertex(FVector(InputVertex), InConversionType);
				InMesh3D.SetVertex(VertId, Vertex);
			}

			// Need to recompute the overlay normals so it renders the mesh correctly
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(InMesh3D);

		}, EDynamicMeshChangeType::MeshVertexChange, EDynamicMeshAttributeChangeFlags::VertexPositions | EDynamicMeshAttributeChangeFlags::NormalsTangents);

		OnTemplateMeshChanged.Broadcast();
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Mismatch in number of vertices when setting mesh for %s pose. %d vertices provided but %d are expected"), *UMetaHumanIdentityPose::PoseTypeAsString(InPoseType), InNewVertices.Num(), PoseHeadMesh->GetMeshRef().VertexCount());
	}
}

void UMetaHumanTemplateMeshComponent::GetPoseHeadMeshVertices(EIdentityPoseType InPoseType, const FTransform& InTransform, ETemplateVertexConversion InConvertionType, TArray<FVector>& OutPoseHeadVertices) const
{
	UDynamicMesh* HeadMesh = GetPoseHeadMesh(InPoseType);
	const UE::Geometry::FDynamicMesh3& Mesh = HeadMesh->GetMeshRef();

	OutPoseHeadVertices.SetNumUninitialized(Mesh.VertexCount());

	for (int32 VertexId = 0; VertexId < Mesh.VertexCount(); ++VertexId)
	{
		FVector Vertex = Mesh.GetVertex(VertexId);
		Vertex = InTransform.TransformPosition(Vertex);
		OutPoseHeadVertices[VertexId] = ConvertVertex(Vertex, InConvertionType);
	}
}

void UMetaHumanTemplateMeshComponent::GetEyeMeshesVertices(const FTransform& InTransform, ETemplateVertexConversion InConversionType, TArray<FVector>& OutLeftEyeVertices, TArray<FVector>& OutRightEyeVertices) const
{
	check(LeftEyeComponent->GetMesh()->VertexCount() == RightEyeComponent->GetMesh()->VertexCount());
	const int32 NumVerts = LeftEyeComponent->GetMesh()->VertexCount();
	OutLeftEyeVertices.SetNumUninitialized(NumVerts);
	OutRightEyeVertices.SetNumUninitialized(NumVerts);

	for (int32 VertexId = 0; VertexId < NumVerts; ++VertexId)
	{
		FVector LeftEyeVertex = LeftEyeComponent->GetMesh()->GetVertex(VertexId);
		FVector RightEyeVertex = RightEyeComponent->GetMesh()->GetVertex(VertexId);

		// Transform the vertices to scan space
		LeftEyeVertex = InTransform.TransformPosition(LeftEyeVertex);
		RightEyeVertex = InTransform.TransformPosition(RightEyeVertex);

		OutLeftEyeVertices[VertexId] = ConvertVertex(LeftEyeVertex, InConversionType);
		OutRightEyeVertices[VertexId] = ConvertVertex(RightEyeVertex, InConversionType);
	}
}

void UMetaHumanTemplateMeshComponent::GetTeethMeshVertices(const FTransform& InTransform, ETemplateVertexConversion InConversionType, TArray<FVector>& OutTeethVertices) const
{
	const int32 NumVerts = TeethMeshComponent->GetMesh()->VertexCount();
	OutTeethVertices.SetNumUninitialized(NumVerts);

	for (int32 VertexId = 0; VertexId < NumVerts; ++VertexId)
	{
		FVector TeethVertex = TeethMeshComponent->GetMesh()->GetVertex(VertexId);

		// Transform the vertices to scan space
		TeethVertex = InTransform.TransformPosition(TeethVertex);

		OutTeethVertices[VertexId] = ConvertVertex(TeethVertex, InConversionType);
	}
}


void UMetaHumanTemplateMeshComponent::SetEyeMeshesVertices(TConstArrayView<FVector3f> InLeftEyeVertices, TConstArrayView<FVector3f> InRightEyeVertices, ETemplateVertexConversion InConversionType)
{
	check(LeftEyeComponent->GetDynamicMesh());
	check(RightEyeComponent->GetDynamicMesh());

	if (InLeftEyeVertices.Num() == LeftEyeComponent->GetMesh()->VertexCount() && InRightEyeVertices.Num() == RightEyeComponent->GetMesh()->VertexCount())
	{
		constexpr EDynamicMeshAttributeChangeFlags ChangeFlags = EDynamicMeshAttributeChangeFlags::VertexPositions | EDynamicMeshAttributeChangeFlags::NormalsTangents;

		LeftEyeComponent->GetDynamicMesh()->EditMesh([InLeftEyeVertices, InConversionType](FDynamicMesh3& InMesh3D)
		{
			int32 Ctr = 0;
			for (const FVector3f& Vertex : InLeftEyeVertices)
			{
				InMesh3D.SetVertex(Ctr++, ConvertVertex(FVector(Vertex), InConversionType));
			}

			// Need to recompute the overlay normals so it renders the mesh correctly
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(InMesh3D);

		}, EDynamicMeshChangeType::MeshVertexChange, ChangeFlags);

		RightEyeComponent->GetDynamicMesh()->EditMesh([InRightEyeVertices, InConversionType](FDynamicMesh3& InMesh3D)
		{
			int32 Ctr = 0;
			for (const FVector3f& Vertex : InRightEyeVertices)
			{
				InMesh3D.SetVertex(Ctr++, ConvertVertex(FVector(Vertex), InConversionType));
			}

			// Need to recompute the overlay normals so it renders the mesh correctly
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(InMesh3D);

		}, EDynamicMeshChangeType::MeshVertexChange, ChangeFlags);

		OnTemplateMeshChanged.Broadcast();
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity,
			   Error,
			   TEXT("Mismatch in number of vertices when setting mesh for eyes. Expected %d for left eye but %d provided. Expected %d for right eye but %d provided"),
			   LeftEyeComponent->GetMesh()->VertexCount(),
			   InLeftEyeVertices.Num(),
			   RightEyeComponent->GetMesh()->VertexCount(),
			   InRightEyeVertices.Num());
	}
}

void UMetaHumanTemplateMeshComponent::SetEyeMeshesVisibility(bool bInVisible)
{
	bShowEyes = bInVisible;

	RightEyeComponent->SetVisibility(IsVisible() && bShowEyes);
	LeftEyeComponent->SetVisibility(IsVisible() && bShowEyes);

	OnTemplateMeshChanged.Broadcast();
}

void UMetaHumanTemplateMeshComponent::SetTeethMeshVisibility(bool bInVisible)
{
	bShowTeethMesh = bInVisible;

	TeethMeshComponent->SetVisibility(IsVisible() && bShowTeethMesh);
	OnTemplateMeshChanged.Broadcast();
}

void UMetaHumanTemplateMeshComponent::BakeEyeMeshesTransform(const FTransform& InTransform)
{
	check(LeftEyeComponent->GetDynamicMesh());
	check(RightEyeComponent->GetDynamicMesh());

	constexpr EDynamicMeshAttributeChangeFlags ChangeFlags = EDynamicMeshAttributeChangeFlags::VertexPositions | EDynamicMeshAttributeChangeFlags::NormalsTangents;

	auto BakeTransform = [&InTransform](FDynamicMesh3& InMesh3D)
	{
		for (int32 VertexId = 0; VertexId < InMesh3D.VertexCount(); ++VertexId)
		{
			const FVector Vertex = InMesh3D.GetVertex(VertexId);
			InMesh3D.SetVertex(VertexId, InTransform.TransformPosition(Vertex));
		}

		// Need to recompute the overlay normals so it renders the mesh correctly
		UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(InMesh3D);
	};

	LeftEyeComponent->GetDynamicMesh()->EditMesh(BakeTransform, EDynamicMeshChangeType::MeshVertexChange, ChangeFlags);
	RightEyeComponent->GetDynamicMesh()->EditMesh(BakeTransform, EDynamicMeshChangeType::MeshVertexChange, ChangeFlags);

	OnTemplateMeshChanged.Broadcast();
}

void UMetaHumanTemplateMeshComponent::SetTeethMeshVertices(TConstArrayView<FVector3f> InNewVertices, ETemplateVertexConversion InConversionType) const
{
	check(FittedTeethMesh);

	if (FittedTeethMesh->GetMeshRef().VertexCount() == InNewVertices.Num())
	{
		FittedTeethMesh->EditMesh([InNewVertices, InConversionType](FDynamicMesh3& InMesh3D)
		{
			int32 Ctr = 0;
			for (const FVector3f& Vertex : InNewVertices)
			{
				InMesh3D.SetVertex(Ctr++, ConvertVertex(FVector(Vertex), InConversionType));
			}

			// Need to recompute the overlay normals so it renders the mesh correctly
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(InMesh3D);

		}, EDynamicMeshChangeType::MeshVertexChange, EDynamicMeshAttributeChangeFlags::VertexPositions | EDynamicMeshAttributeChangeFlags::NormalsTangents);

		TeethMeshComponent->GetDynamicMesh()->SetMesh(FittedTeethMesh->GetMeshRef());

		OnTemplateMeshChanged.Broadcast();
	}
	else
	{
		UE_LOG(LogMetaHumanIdentity, Error, TEXT("Mismatch in number of vertices when setting mesh for teeth. %d vertices provided but %d are expected"), InNewVertices.Num(), FittedTeethMesh->GetMeshRef().VertexCount());
	}
}

void UMetaHumanTemplateMeshComponent::BakeTeethMeshTransform(const FTransform& InTransform)
{
	// Populate the meshes for the CDO if needed
	UMetaHumanTemplateMeshComponent* TemplateMeshComponentCDO = GetMutableDefault<UMetaHumanTemplateMeshComponent>();
	if (TemplateMeshComponentCDO->OriginalTeethMesh->IsEmpty() || TemplateMeshComponentCDO->FittedTeethMesh->IsEmpty())
	{
		TemplateMeshComponentCDO->LoadMeshAssets();
	}

	OriginalTeethMesh->SetMesh(TemplateMeshComponentCDO->OriginalTeethMesh->GetMeshRef());
	FittedTeethMesh->SetMesh(TemplateMeshComponentCDO->FittedTeethMesh->GetMeshRef());

	if (!InTransform.Identical(&FTransform::Identity, 0))
	{
		auto BakeTransform = [&InTransform](FDynamicMesh3& InMesh3D)
		{
			for (int32 VertexId = 0; VertexId < InMesh3D.VertexCount(); ++VertexId)
			{
				const FVector Vertex = InMesh3D.GetVertex(VertexId);
				InMesh3D.SetVertex(VertexId, InTransform.TransformPosition(Vertex));
			}

			// Need to recompute the overlay normals so it renders the mesh correctly
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(InMesh3D);
		};

		constexpr EDynamicMeshAttributeChangeFlags ChangeFlags = EDynamicMeshAttributeChangeFlags::VertexPositions | EDynamicMeshAttributeChangeFlags::NormalsTangents;
		OriginalTeethMesh->EditMesh(BakeTransform, EDynamicMeshChangeType::MeshVertexChange, ChangeFlags);
		FittedTeethMesh->EditMesh(BakeTransform, EDynamicMeshChangeType::MeshVertexChange, ChangeFlags);
	}

	// Reset the teeth mesh in the teeth mesh component so the change is reflected in the instance being visualized
	TeethMeshComponent->GetDynamicMesh()->SetMesh(bShowFittedTeeth ? FittedTeethMesh->GetMeshRef() : OriginalTeethMesh->GetMeshRef());

	OnTemplateMeshChanged.Broadcast();
}

void UMetaHumanTemplateMeshComponent::ResetMeshes()
{
	UMetaHumanTemplateMeshComponent* TemplateMeshComponentCDO = GetMutableDefault<UMetaHumanTemplateMeshComponent>();
	if (TemplateMeshComponentCDO->HeadMeshComponent->GetDynamicMesh()->IsEmpty())
	{
		TemplateMeshComponentCDO->LoadMeshAssets();
	}

	OriginalTeethMesh->SetMesh(TemplateMeshComponentCDO->OriginalTeethMesh->GetMeshRef());
	FittedTeethMesh->SetMesh(TemplateMeshComponentCDO->FittedTeethMesh->GetMeshRef());
	LeftEyeComponent->GetDynamicMesh()->SetMesh(TemplateMeshComponentCDO->LeftEyeComponent->GetDynamicMesh()->GetMeshRef());
	RightEyeComponent->GetDynamicMesh()->SetMesh(TemplateMeshComponentCDO->RightEyeComponent->GetDynamicMesh()->GetMeshRef());

	for (const TPair<EIdentityPoseType, TObjectPtr<UDynamicMesh>>& PoseHeadMeshPair : TemplateMeshComponentCDO->PoseHeadMeshes)
	{
		PoseHeadMeshes[PoseHeadMeshPair.Key]->SetMesh(PoseHeadMeshPair.Value->GetMeshRef());
	}

	// Reset the meshes in the components that display the data
	HeadMeshComponent->GetDynamicMesh()->SetMesh(PoseHeadMeshes[EIdentityPoseType::Neutral]->GetMeshRef());
	TeethMeshComponent->GetDynamicMesh()->SetMesh(bShowFittedTeeth ? FittedTeethMesh->GetMeshRef() : OriginalTeethMesh->GetMeshRef());

	OnTemplateMeshChanged.Broadcast();
}