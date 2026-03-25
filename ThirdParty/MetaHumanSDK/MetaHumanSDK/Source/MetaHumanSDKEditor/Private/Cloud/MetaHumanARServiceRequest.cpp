// Copyright Epic Games, Inc. All Rights Reserved.
#include "Cloud/MetaHumanARServiceRequest.h"
#include "Cloud/MetaHumanCloudServicesSettings.h"
#include "Cloud/MetaHumanZlib.h"
#include "Logging/StructuredLog.h"
#include "Misc/EngineVersion.h"
#include "DNAUtils.h"
#include "DNACommon.h"
#include "DNAReader.h"

// enable this in order to save out a copy of the protobuf payload for requests
#define DEBUG_SAVE_PROTOBUF_PAYLOAD 0
#if DEBUG_SAVE_PROTOBUF_PAYLOAD
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#endif // #ifdef DEBUG_SAVE_PROTOBUF_PAYLOAD

THIRD_PARTY_INCLUDES_START
#pragma warning(disable:4068)
#pragma pvs(push)
#pragma pvs(disable: 590)
#pragma pvs(disable: 568)
#include "Proto/metahuman_service_api.pb.cc.inc"
#pragma pvs(pop)
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogAutorigServiceRequest, Log, All)

namespace UE::MetaHuman
{
	/** Number of vertices of the face mesh on the LOD0. */
	static const int32 FaceMeshVertexCount = 24049;
	/** Number of vertices of the eye mesh on the LOD0. */
	static const int32 EyeMeshVertexCount = 770;

	void FAutoRigServiceRequest::RequestSolveAsync()
	{
		ExecuteRequestAsync(nullptr);
	}

	TSharedRef<FAutoRigServiceRequest> FAutoRigServiceRequest::CreateRequest(const FTargetSolveParameters& InSolveParams)
	{
		TSharedRef<FAutoRigServiceRequest> Request = MakeShared<FAutoRigServiceRequest>();
		Request->SolveParameters = InSolveParams;
		return Request;
	}

	bool FAutoRigServiceRequest::DoBuildRequest(TSharedRef<IHttpRequest> HttpRequest, FRequestContextBasePtr Context)
	{
		const UMetaHumanCloudServicesSettings* Settings = GetDefault<UMetaHumanCloudServicesSettings>();		
		HttpRequest->SetVerb("POST");
		HttpRequest->SetURL(Settings->AutorigServiceUrl);
		
		const auto FillProtoMesh = [](metahuman_service_api::Mesh* Mesh, const TArray<FVector>& SourceArray) -> bool
			{
				for (const FVector& Vector : SourceArray)
				{
					if (FMath::IsFinite(Vector.X) && FMath::IsFinite(Vector.Y) && FMath::IsFinite(Vector.Z))
					{
						metahuman_service_api::Vertex* vertex = Mesh->add_vertices();
						vertex->set_x(Vector.X);
						vertex->set_y(Vector.Y);
						vertex->set_z(Vector.Z);
					}
					else
					{
						UE_LOGFMT(LogAutorigServiceRequest, Warning, "Invalid FVector input to autorigger");
						return false;
					}
				}
				return true;
			};

		// build Protobuf message
		{
#define MH_ARS_ALLOCATE_MESSAGE(Type, Name)\
	metahuman_service_api::Type * Name = new metahuman_service_api::Type

			MH_ARS_ALLOCATE_MESSAGE(Head, Head);
			MH_ARS_ALLOCATE_MESSAGE(Mesh, Face);
			FillProtoMesh(Face, SolveParameters.ConformedFaceVertices);
			Head->set_allocated_face(Face);

			MH_ARS_ALLOCATE_MESSAGE(Eyes, Eyes);
			MH_ARS_ALLOCATE_MESSAGE(Mesh, LeftEye);
			FillProtoMesh(LeftEye, SolveParameters.ConformedLeftEyeVertices);
			MH_ARS_ALLOCATE_MESSAGE(Mesh, RightEye);
			FillProtoMesh(RightEye, SolveParameters.ConformedRightEyeVertices);
			MH_ARS_ALLOCATE_MESSAGE(Mesh, Shell);
			FillProtoMesh(Shell, SolveParameters.ConformedEyeShellVertices);
			MH_ARS_ALLOCATE_MESSAGE(Mesh, Lashes);
			FillProtoMesh(Lashes, SolveParameters.ConformedEyeLashesVertices);
			MH_ARS_ALLOCATE_MESSAGE(Mesh, Edge);
			FillProtoMesh(Edge, SolveParameters.ConformedEyeEdgeVertices);
			Eyes->set_allocated_edge(Edge);
			Eyes->set_allocated_left(LeftEye);
			Eyes->set_allocated_right(RightEye);
			Eyes->set_allocated_lashes(Lashes);
			Eyes->set_allocated_shell(Shell);
			Head->set_allocated_eyes(Eyes);

			MH_ARS_ALLOCATE_MESSAGE(Mesh, Teeth);
			FillProtoMesh(Teeth, SolveParameters.ConformedTeethVertices);
			Head->set_allocated_teeth(Teeth);

			MH_ARS_ALLOCATE_MESSAGE(Mesh, Cartilage);
			FillProtoMesh(Cartilage, SolveParameters.ConformedCartilageVertices);
			Head->set_allocated_cartilage(Cartilage);

			metahuman_service_api::AutorigRequest ProtoRequest;
			ProtoRequest.set_allocated_head(Head);
			ProtoRequest.set_high_frequency_index(SolveParameters.HighFrequency);
			ProtoRequest.set_to_target_scale(SolveParameters.Scale);

			if (SolveParameters.BindPose.Num() && SolveParameters.Coefficients.Num())
			{
				MH_ARS_ALLOCATE_MESSAGE(Parameters, Parameters);
				*Parameters->mutable_bind_pose() = { SolveParameters.BindPose.GetData(), SolveParameters.BindPose.GetData() + SolveParameters.BindPose.Num() };
				*Parameters->mutable_solver_coefficients() = { SolveParameters.Coefficients.GetData(), SolveParameters.Coefficients.GetData() + SolveParameters.Coefficients.Num() };
				Parameters->set_model_id(TCHAR_TO_UTF8(*SolveParameters.ModelIdentifier));
				ProtoRequest.set_allocated_parameters(Parameters);
			}

			MH_ARS_ALLOCATE_MESSAGE(Quality, Quality);
			switch (SolveParameters.RigType)
			{
			case ERigType::JointsAndBlendshapes:
				Quality->set_rig_type(metahuman_service_api::RIG_TYPE_JOINTS_AND_BLENDSHAPES);
				break;
			case ERigType::JointsOnly:
			default:
				Quality->set_rig_type(metahuman_service_api::RIG_TYPE_JOINTS_ONLY);
				break;
			}
			switch (SolveParameters.RigRefinementLevel)
			{
			case ERigRefinementLevel::Medium:
				Quality->set_refinement_level(metahuman_service_api::REFINEMENT_LEVEL_MEDIUM);
				break;
			case ERigRefinementLevel::None:
			default:
				Quality->set_refinement_level(metahuman_service_api::REFINEMENT_LEVEL_NONE);
				break;
			}
			switch (SolveParameters.ExportLayers)
			{
			case EExportLayers::Rbf:
				Quality->set_export_layers(metahuman_service_api::ExportLayers::EXPORT_LAYERS_RBF);
				break;
			case EExportLayers::None:
				Quality->set_export_layers(metahuman_service_api::ExportLayers::EXPORT_LAYERS_NONE);
				break;
			case EExportLayers::Default:
			default:
				Quality->set_export_layers(metahuman_service_api::ExportLayers::EXPORT_LAYERS_UNKNOWN);
				break;
			}

			ProtoRequest.set_allocated_quality(Quality);

			MH_ARS_ALLOCATE_MESSAGE(UEVersion, UEVersion);
			UEVersion->set_minor(FEngineVersion::Current().GetMinor());
			UEVersion->set_major(FEngineVersion::Current().GetMajor());
			ProtoRequest.set_allocated_ue_version(UEVersion);
			
			const size_t SizeOfProtoRequest = ProtoRequest.ByteSizeLong();
			TArray<uint8> ProtoRequestData;
			ProtoRequestData.SetNumUninitialized(SizeOfProtoRequest);
			ProtoRequest.SerializeToArray(ProtoRequestData.GetData(), SizeOfProtoRequest);

#if DEBUG_SAVE_PROTOBUF_PAYLOAD
			{
				const FString PayloadSaveName = FPaths::ProjectDir() / TEXT("ars_proto.bin");
				FFileHelper::SaveArrayToFile(ProtoRequestData, *PayloadSaveName);
			}
#endif

			HttpRequest->SetHeader("Content-Type", TEXT("application/octet-stream"));
			HttpRequest->SetContent(MoveTemp(ProtoRequestData));
		}

		return true;
	}

	void FAutoRigServiceRequest::OnRequestCompleted(const TArray<uint8>& Content, FRequestContextBasePtr Context)
	{
		FAutorigResponse Response(Content);
		if (Response.IsValid())
		{
			AutorigRequestCompleteDelegate.ExecuteIfBound(Response);
		}
		else
		{
			UE_LOGFMT(LogAutorigServiceRequest, Error, "Service returned invalid DNA");
			OnRequestFailed(EMetaHumanServiceRequestResult::ServerError, Context);
		}
	}

	// ========================================================================

	bool FAutorigResponse::ReadDna()
	{
		TArray<uint8> PayloadCopy;
		PayloadCopy.SetNumUninitialized(Payload.Num());
		FMemory::Memcpy(PayloadCopy.GetData(), Payload.GetData(), Payload.Num());
		Dna = ReadDNAFromBuffer(&PayloadCopy, EDNADataLayer::All);
		return Dna != nullptr;
	}
}