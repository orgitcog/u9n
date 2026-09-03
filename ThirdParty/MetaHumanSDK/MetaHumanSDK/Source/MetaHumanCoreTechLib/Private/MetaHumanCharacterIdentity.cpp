// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterIdentity.h"
#include "MetaHumanCharacterBodyIdentity.h"
#include "MetaHumanCoreTechLibGlobals.h"

#include "api/MetaHumanCreatorAPI.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "DNAUtils.h"
#include "Serialization/JsonSerializer.h"
#include "dna/Reader.h"
#include "DNAReaderAdapter.h"
#include "MetaHumanRigEvaluatedState.h"
#include "Serialization/MemoryReader.h"
#include "DNAAsset.h"

#include <string>

// Ensure the EAlignmentOptions match the same enum from the mhc api in titan
static_assert(static_cast<int32>(EAlignmentOptions::None) == static_cast<int32>(TITAN_API_NAMESPACE::AlignmentOptions::None));
static_assert(static_cast<int32>(EAlignmentOptions::Translation) == static_cast<int32>(TITAN_API_NAMESPACE::AlignmentOptions::Translation));
static_assert(static_cast<int32>(EAlignmentOptions::RotationTranslation) == static_cast<int32>(TITAN_API_NAMESPACE::AlignmentOptions::RotationTranslation));
static_assert(static_cast<int32>(EAlignmentOptions::ScalingTranslation) == static_cast<int32>(TITAN_API_NAMESPACE::AlignmentOptions::ScalingTranslation));
static_assert(static_cast<int32>(EAlignmentOptions::ScalingRotationTranslation) == static_cast<int32>(TITAN_API_NAMESPACE::AlignmentOptions::ScalingRotationTranslation));

static_assert(static_cast<int32>(EBlendOptions::Proportions) == static_cast<int32>(TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FaceAttribute::Proportions));
static_assert(static_cast<int32>(EBlendOptions::Features) == static_cast<int32>(TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FaceAttribute::Features));
static_assert(static_cast<int32>(EBlendOptions::Both) == static_cast<int32>(TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FaceAttribute::Both));

struct FMetaHumanCharacterIdentity::FImpl
{
	std::shared_ptr<const TITAN_API_NAMESPACE::MetaHumanCreatorAPI> MHCAPI;
	EMetaHumanCharacterOrientation InputDatabaseOrient;
	dna::Reader* InternalDnaReader;
};

struct FMetaHumanCharacterIdentity::FState::FImpl
{
	std::shared_ptr<const TITAN_API_NAMESPACE::MetaHumanCreatorAPI> MHCAPI;
	std::shared_ptr<const TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> MHCState;
	EMetaHumanCharacterOrientation InputDatabaseOrient;
	TArray<FVector3f> BodyVertexNormals;
	TArray<int32> BodyNumVerticesPerLod;
	dna::Reader* InternalDnaReader;

	TSharedPtr<const FMetaHumanRigEvaluatedState> CachedEvaluatedState;
};

struct FMetaHumanCharacterIdentity::FSettings::FImpl
{
	std::shared_ptr<const TITAN_API_NAMESPACE::MetaHumanCreatorAPI::Settings> MHCSettings;
};

FMetaHumanCharacterIdentity::FMetaHumanCharacterIdentity()
{

}

bool FMetaHumanCharacterIdentity::Init(const FString& InMHCDataPath, const FString& InBodyMHCDataPath, UDNAAsset* InDNAAsset, EMetaHumanCharacterOrientation InDNAAssetOrient)
{
#if WITH_EDITORONLY_DATA // can only use InDNAAsset->GetGeometryReader() in editor
	Impl = MakePimpl<FImpl>();

	// define which conversion is needed to render properly in UE
	Impl->InputDatabaseOrient = InDNAAssetOrient;

	dna::Reader* BinaryDnaReader = InDNAAsset->GetGeometryReader()->Unwrap();
	Impl->InternalDnaReader = BinaryDnaReader;	

	// Load combined head and body dna
	const FString CombinedDnaPath = InBodyMHCDataPath + TEXT("/body_head_combined.dna");
	TSharedPtr<IDNAReader> CombinedDnaReader = ReadDNAFromFile(CombinedDnaPath, EDNADataLayer::Geometry);

	int MaxThreads = -1;
	Impl->MHCAPI = TITAN_API_NAMESPACE::MetaHumanCreatorAPI::CreateMHCApi(BinaryDnaReader, TCHAR_TO_UTF8(*InMHCDataPath), MaxThreads, (CombinedDnaReader != nullptr) ? CombinedDnaReader->Unwrap() : nullptr);
	if (!Impl->MHCAPI)
	{
		UE_LOG(LogMetaHumanCoreTechLib, Error, TEXT("failed to initialize MHC API"));
		return false;
	}

	return true;
#else
	UE_LOG(LogMetaHumanCoreTechLib, Error, TEXT("MHC API only works with EditorOnly Data"));
	return false;
#endif
}

FMetaHumanCharacterIdentity::~FMetaHumanCharacterIdentity() = default;

TSharedPtr<FMetaHumanCharacterIdentity::FState> FMetaHumanCharacterIdentity::CreateState() const
{
	if (!Impl->MHCAPI) return nullptr;

	TSharedPtr<FState, ESPMode::ThreadSafe> State = MakeShared<FState>();
	State->Impl->MHCState = Impl->MHCAPI->CreateState();
	State->Impl->MHCAPI = Impl->MHCAPI;
	State->Impl->InputDatabaseOrient = Impl->InputDatabaseOrient;
	State->Impl->InternalDnaReader = Impl->InternalDnaReader;
	return State;
}

TArray<FString> FMetaHumanCharacterIdentity::GetPresetNames() const
{
	check(Impl->MHCAPI);

	std::vector<std::string> PresetNames = Impl->MHCAPI->GetPresetNames();
	TArray<FString> Out;
	Out.AddDefaulted(PresetNames.size());
	for (size_t i = 0; i < PresetNames.size(); ++i)
	{
		Out[i] = UTF8_TO_TCHAR(PresetNames[i].c_str());
	}
	return Out;
}

TSharedPtr<IDNAReader> FMetaHumanCharacterIdentity::CopyBodyJointsToFace(dna::Reader* InBodyDnaReader, dna::Reader* InFaceDnaReader) const
{
	check(Impl->MHCAPI);

	pma::ScopedPtr<dna::MemoryStream> OutputStream = pma::makeScoped<dna::MemoryStream>();
	pma::ScopedPtr<dna::BinaryStreamWriter> DnaWriter = pma::makeScoped<dna::BinaryStreamWriter>(OutputStream.get());
	DnaWriter->setFrom(InFaceDnaReader);

	Impl->MHCAPI->CopyBodyJointsToFace(InBodyDnaReader, InFaceDnaReader, DnaWriter.get());

	DnaWriter->write();

	pma::ScopedPtr<dna::BinaryStreamReader> StateDnaReader = pma::makeScoped<dna::BinaryStreamReader>(OutputStream.get());
	StateDnaReader->read();

	return MakeShared<FDNAReader<dna::BinaryStreamReader>>(StateDnaReader.release());
}

TSharedPtr<IDNAReader> FMetaHumanCharacterIdentity::UpdateFaceSkinWeightsFromBodyAndVertexNormals(const TArray<TPair<int32, TArray<FFloatTriplet>>>& InCombinedBodySkinWeights, dna::Reader* InFaceDnaReader, const FMetaHumanCharacterIdentity::FState& InState) const
{
	check(Impl->MHCAPI);

	pma::ScopedPtr<dna::MemoryStream> OutputStream = pma::makeScoped<dna::MemoryStream>();
	pma::ScopedPtr<dna::BinaryStreamWriter> DnaWriter = pma::makeScoped<dna::BinaryStreamWriter>(OutputStream.get());
	DnaWriter->setFrom(InFaceDnaReader);

	std::vector<std::pair<int, std::vector<Eigen::Triplet<float>>>> CombinedBodySkinWeights(InCombinedBodySkinWeights.Num());
	ensure(sizeof(Eigen::Triplet<float>) == sizeof(FFloatTriplet));

	for (int32 Lod = 0; Lod < InCombinedBodySkinWeights.Num(); ++Lod)
	{
		size_t DataSize = sizeof(Eigen::Triplet<float>) * InCombinedBodySkinWeights[Lod].Value.Num();
		CombinedBodySkinWeights[size_t(Lod)].second.resize(size_t(InCombinedBodySkinWeights[size_t(Lod)].Value.Num()));
		std::memcpy((void*)CombinedBodySkinWeights[size_t(Lod)].second.data(), (void*)InCombinedBodySkinWeights[Lod].Value.GetData(), DataSize);
		CombinedBodySkinWeights[size_t(Lod)].first = InCombinedBodySkinWeights[Lod].Key;
	}

	Impl->MHCAPI->UpdateFaceSkinWeightsFromBody(CombinedBodySkinWeights, InFaceDnaReader, DnaWriter.get());

	const FMetaHumanRigEvaluatedState VerticesAndNormals = InState.Evaluate();
	const TArray<FVector3f>& FaceVertexNormals = VerticesAndNormals.VertexNormals;

	const int32 MeshCount = InFaceDnaReader->getMeshCount();
	for (int32 MeshIndex = 0; MeshIndex < MeshCount; MeshIndex++)
	{
		const int32 VertexCount = InFaceDnaReader->getVertexPositionCount(MeshIndex);
		TArray<dna::Vector3> Normals;
		Normals.SetNum(VertexCount);
		for (int32 DNAVertexIndex = 0; DNAVertexIndex < VertexCount; DNAVertexIndex++)
		{
			// note that we get the RAW vertex for normals as they are already in the correct coordinate frame
			const FVector3f StateVertexNormal = InState.GetRawVertex(FaceVertexNormals, MeshIndex, DNAVertexIndex); 
			Normals[DNAVertexIndex] = dna::Vector3{ StateVertexNormal.X, StateVertexNormal.Y, StateVertexNormal.Z };
		}

		DnaWriter->setVertexNormals(MeshIndex, Normals.GetData(), VertexCount);
	}

	DnaWriter->write();

	pma::ScopedPtr<dna::BinaryStreamReader> StateDnaReader = pma::makeScoped<dna::BinaryStreamReader>(OutputStream.get());
	StateDnaReader->read();

	return MakeShared<FDNAReader<dna::BinaryStreamReader>>(StateDnaReader.release());

}


FMetaHumanCharacterIdentity::FSettings::FSettings() 
{
	Impl = MakePimpl<FImpl>();
}

FMetaHumanCharacterIdentity::FSettings::~FSettings() = default;

FMetaHumanCharacterIdentity::FSettings::FSettings(const FSettings& InOther)
{
	Impl = MakePimpl<FImpl>(*InOther.Impl);
}

float FMetaHumanCharacterIdentity::FSettings::GlobalVertexDeltaScale() const
{
	check(Impl->MHCSettings);
	return Impl->MHCSettings->GlobalVertexDeltaScale();
}

void FMetaHumanCharacterIdentity::FSettings::SetGlobalVertexDeltaScale(float InGlobalVertexDeltaScale)
{
	check(Impl->MHCSettings);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::Settings> NewMHCSettings = Impl->MHCSettings->Clone();
	NewMHCSettings->SetGlobalVertexDeltaScale(InGlobalVertexDeltaScale);
	Impl->MHCSettings = NewMHCSettings;
}

bool FMetaHumanCharacterIdentity::FSettings::UseBodyDeltaInEvaluation() const
{
	check(Impl->MHCSettings);
	return Impl->MHCSettings->UseBodyDeltaInEvaluation();
}

void FMetaHumanCharacterIdentity::FSettings::SetBodyDeltaInEvaluation(bool bInIsBodyDeltaInEvaluation)
{
	check(Impl->MHCSettings);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::Settings> NewMHCSettings = Impl->MHCSettings->Clone();
	NewMHCSettings->SetUseBodyDeltaInEvaluation(bInIsBodyDeltaInEvaluation);
	Impl->MHCSettings = NewMHCSettings;
}

float FMetaHumanCharacterIdentity::FSettings::GlobalHighFrequencyScale() const
{
	check(Impl->MHCSettings);
	return Impl->MHCSettings->GlobalHFScale();
}


void FMetaHumanCharacterIdentity::FSettings::SetGlobalHighFrequencyScale(float InGlobalHighFrequencyScale)
{
	check(Impl->MHCSettings);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::Settings> NewMHCSettings = Impl->MHCSettings->Clone();
	NewMHCSettings->SetGlobalHFScale(InGlobalHighFrequencyScale);
	Impl->MHCSettings = NewMHCSettings;
}

void FMetaHumanCharacterIdentity::FSettings::SetHighFrequencyIteration(int32 InHighFrequencyScale)
{
	check(Impl->MHCSettings);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::Settings> NewMHCSettings = Impl->MHCSettings->Clone();
	NewMHCSettings->SetHFIterations(static_cast<int32>(InHighFrequencyScale));
	Impl->MHCSettings = NewMHCSettings;
}

FMetaHumanCharacterIdentity::FState::FState()
{
	Impl = MakePimpl<FImpl>();
}

FMetaHumanCharacterIdentity::FState::~FState() = default;

FMetaHumanCharacterIdentity::FState::FState(const FState& InOther)
{
	Impl = MakePimpl<FImpl>(*InOther.Impl);
}

FMetaHumanRigEvaluatedState FMetaHumanCharacterIdentity::FState::Evaluate() const
{
	check(Impl->MHCAPI);
	check(Impl->MHCState);
	FMetaHumanRigEvaluatedState Out;
	if (Impl->CachedEvaluatedState)
	{
		Out = *Impl->CachedEvaluatedState;
	}
	else
	{
		Out.Vertices.AddUninitialized(Impl->MHCAPI->NumVertices());
		Out.VertexNormals.AddUninitialized(Impl->MHCAPI->NumVertices());
		ensure(Impl->MHCAPI->Evaluate(*Impl->MHCState, (float*)Out.Vertices.GetData()));

		Eigen::Map<Eigen::Matrix<float, 3, -1>> Vertices((float*)Out.Vertices.GetData(), 3, Impl->MHCAPI->NumVertices());
		std::vector<Eigen::Ref<const Eigen::Matrix<float, 3, -1>>> BodyVertexNormals;
		{
			BodyVertexNormals.reserve((Impl->BodyNumVerticesPerLod.Num()));
			const float* DataPtr = (const float*)Impl->BodyVertexNormals.GetData();
			for (int32 Lod = 0; Lod < Impl->BodyNumVerticesPerLod.Num(); ++Lod)
			{
				Eigen::Map<const Eigen::Matrix<float, 3, -1>> MappedMatrix(DataPtr, 3, Impl->BodyNumVerticesPerLod[Lod]);
				BodyVertexNormals.emplace_back(MappedMatrix);
				DataPtr += Impl->BodyNumVerticesPerLod[Lod] * 3;
			}

			Eigen::Matrix<float, 3, -1> VertexNormals(3, Impl->MHCAPI->NumVertices());
			Impl->MHCAPI->EvaluateNormals(*Impl->MHCState, Vertices, VertexNormals, BodyVertexNormals);
			// copy to the state
			FMemory::Memcpy(Out.VertexNormals.GetData(), VertexNormals.data(), VertexNormals.size() * sizeof(float));
		}

		Impl->CachedEvaluatedState = MakeShared<FMetaHumanRigEvaluatedState>(Out);
	}
	return Out;
}

bool FMetaHumanCharacterIdentity::FState::FitToFaceDna(TSharedRef<class IDNAReader> InFaceDna, const FFitToTargetOptions& InFitToTargetOptions)
{
	check(Impl->MHCState);

	TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FitToTargetOptions FitToTargetOptions;
	FitToTargetOptions.adaptNeck = InFitToTargetOptions.bAdaptNeck;
	FitToTargetOptions.alignmentOptions = static_cast<TITAN_API_NAMESPACE::AlignmentOptions>(static_cast<int32>(InFitToTargetOptions.AlignmentOptions));
	TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FitToTargetResult Result;

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	if (NewMHCState->FitToTarget(InFaceDna->Unwrap(), FitToTargetOptions, &Result))
	{
		std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::Settings> NewSettings = NewMHCState->GetSettings()->Clone();
		NewSettings->SetGlobalVertexDeltaScale(1.0f);
		if (InFitToTargetOptions.bDisableHighFrequencyDelta)
		{
			NewSettings->SetGlobalHFScale(0.0f);
		}
		NewMHCState->SetSettings(NewSettings);
		Impl->MHCState = NewMHCState;
		Impl->CachedEvaluatedState.Reset();
		return true;
	}
	return false;	
}


bool FMetaHumanCharacterIdentity::FState::FitToTarget(const TMap<int32, TArray<FVector3f>>& InPartsVertices, const FFitToTargetOptions& InFitToTargetOptions)
{
	check(Impl->MHCState);

	std::map<int, Eigen::Ref<const Eigen::Matrix<float, 3, -1>>> Vertices;
	TArray< TArray<FVector3f> > AllVertices;

	for (const TPair<int32, TArray<FVector3f>>& PartVertices : InPartsVertices)
	{
		TArray<FVector3f> VerticesDNASpace;
		if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
		{
			VerticesDNASpace.AddUninitialized(PartVertices.Value.Num());
			for (int32 I = 0; I < PartVertices.Value.Num(); ++I)
			{
				VerticesDNASpace[I] = FVector3f{ PartVertices.Value[I].X, PartVertices.Value[I].Z, PartVertices.Value[I].Y };
			}
		}
		else
		{
			VerticesDNASpace.AddUninitialized(PartVertices.Value.Num());
			for (int32 I = 0; I < PartVertices.Value.Num(); ++I)
			{
				VerticesDNASpace[I] = FVector3f{ PartVertices.Value[I].X, -PartVertices.Value[I].Y, PartVertices.Value[I].Z };
			}
		}

		AllVertices.Emplace(VerticesDNASpace);
		const Eigen::Map<const Eigen::Matrix<float, 3, -1>> VerticesEigenMap((const float*)AllVertices.Last().GetData(), 3, AllVertices.Last().Num());
		
		Vertices.insert(std::make_pair( PartVertices.Key, Eigen::Ref<const Eigen::Matrix<float, 3, -1>>(VerticesEigenMap) ));
	}

	TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FitToTargetOptions FitToTargetOptions;
	FitToTargetOptions.adaptNeck = InFitToTargetOptions.bAdaptNeck;
	FitToTargetOptions.alignmentOptions = static_cast<TITAN_API_NAMESPACE::AlignmentOptions>(static_cast<int32>(InFitToTargetOptions.AlignmentOptions));
	TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FitToTargetResult Result;

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	if (NewMHCState->FitToTarget(Vertices, FitToTargetOptions, &Result, /* useStabModel*/ false)) // TODO turning off stabilization model for now as it does not seem to be working correctly
	{
		std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::Settings> NewSettings = NewMHCState->GetSettings()->Clone();
		NewSettings->SetGlobalVertexDeltaScale(1.0f);
		if (InFitToTargetOptions.bDisableHighFrequencyDelta)
		{
			NewSettings->SetGlobalHFScale(0.0f);
		}
		NewMHCState->SetSettings(NewSettings);
		Impl->MHCState = NewMHCState;
		Impl->CachedEvaluatedState.Reset();
		return true;
	}

	return false;
}

FVector3f FMetaHumanCharacterIdentity::FState::GetVertex(const TArray<FVector3f>& InVertices, int32 InDNAMeshIndex, int32 InDNAVertexIndex) const
{
	check(Impl->MHCAPI);

	float Pos[3];
	ensure(Impl->MHCAPI->GetVertex((const float*)InVertices.GetData(), InDNAMeshIndex, InDNAVertexIndex, Pos));
	// convert DNA to UE
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		return FVector3f{ Pos[0], Pos[2], Pos[1] };
	}
	else {
		return FVector3f{ Pos[0], -Pos[1], Pos[2] };
	}
}

FVector3f FMetaHumanCharacterIdentity::FState::GetRawVertex(const TArray<FVector3f>& InVertices, int32 InDNAMeshIndex, int32 InDNAVertexIndex) const
{
	check(Impl->MHCAPI);

	float Pos[3];
	ensure(Impl->MHCAPI->GetVertex((const float*)InVertices.GetData(), InDNAMeshIndex, InDNAVertexIndex, Pos));

	return FVector3f{ Pos[0], Pos[1], Pos[2] };;
}

void FMetaHumanCharacterIdentity::FState::GetRawBindPose(const TArray<FVector3f>& InVertices, TArray<float>& OutBindPose) const
{
	check(Impl->MHCAPI);
	Eigen::Matrix3Xf BindPose;
	ensure(Impl->MHCAPI->GetBindPose((const float*)InVertices.GetData(), BindPose));
	OutBindPose.SetNumUninitialized(static_cast<int32>(BindPose.size()));
	Eigen::Map< Eigen::Matrix3Xf> OutBindPoseMap(OutBindPose.GetData(), BindPose.rows(), BindPose.cols());
	OutBindPoseMap = BindPose;
}

void FMetaHumanCharacterIdentity::FState::GetCoefficients(TArray<float>& OutCoefficients) const
{
	check(Impl->MHCAPI);
	Eigen::VectorXf Coefficients;
	ensure(Impl->MHCAPI->GetParameters(*Impl->MHCState, Coefficients));
	OutCoefficients.SetNumUninitialized(static_cast<int32>(Coefficients.size()));
	Eigen::Map< Eigen::VectorXf> OutCoefficientsMap(OutCoefficients.GetData(), Coefficients.size());
	OutCoefficientsMap = Coefficients;
}

void FMetaHumanCharacterIdentity::FState::GetModelIdentifier(FString& OutModelIdentifier) const
{
	check(Impl->MHCAPI);
	std::string ModelIdentifier;
	ensure(Impl->MHCAPI->GetModelIdentifier(*Impl->MHCState, ModelIdentifier));
	OutModelIdentifier = FString(ModelIdentifier.c_str());
}


int32 FMetaHumanCharacterIdentity::FState::NumGizmos() const
{
	check(Impl->MHCState);
	return static_cast<int32>(Impl->MHCState->NumGizmos());
}

int32 FMetaHumanCharacterIdentity::FState::NumLandmarks() const
{
	check(Impl->MHCState);
	return static_cast<int32>(Impl->MHCState->NumLandmarks());
}


TArray<FVector3f> FMetaHumanCharacterIdentity::FState::EvaluateGizmos(const TArray<FVector3f>& InVertices) const
{
	check(Impl->MHCState);

	TArray<FVector3f> Out;
	Out.AddUninitialized(Impl->MHCState->NumGizmos());
	ensure(Impl->MHCState->EvaluateGizmos((const float*)InVertices.GetData(), (float*)Out.GetData()));

	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		for (int32 I = 0; I < Out.Num(); ++I)
		{
			Out[I] = FVector3f{ Out[I].X, Out[I].Z, Out[I].Y };
		}
	}
	else
	{
		for (int32 I = 0; I < Out.Num(); ++I)
		{
			Out[I] = FVector3f{ Out[I].X, -Out[I].Y, Out[I].Z };
		}
	}

	return Out;
}

TArray<FVector3f> FMetaHumanCharacterIdentity::FState::EvaluateLandmarks(const TArray<FVector3f>& InVertices) const
{
	check(Impl->MHCState);

	TArray<FVector3f> Out;
	Out.AddUninitialized(Impl->MHCState->NumLandmarks());
	ensure(Impl->MHCState->EvaluateLandmarks((const float*)InVertices.GetData(), (float*)Out.GetData()));

	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		for (int32 I = 0; I < Out.Num(); ++I)
		{
			Out[I] = FVector3f{ Out[I].X, Out[I].Z, Out[I].Y };
		}
	}
	else
	{
		for (int32 I = 0; I < Out.Num(); ++I)
		{
			Out[I] = FVector3f{ Out[I].X, -Out[I].Y, Out[I].Z };
		}
	}

	return Out;
}

bool FMetaHumanCharacterIdentity::FState::HasLandmark(int32 InVertexIndex) const
{
	check(Impl->MHCState);

	return Impl->MHCState->HasLandmark(InVertexIndex);
}

void FMetaHumanCharacterIdentity::FState::AddLandmark(int32 InVertexIndex)
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->AddLandmark(InVertexIndex));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::RemoveLandmark(int32 InLandmarkIndex)
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->RemoveLandmark(InLandmarkIndex));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

int32 FMetaHumanCharacterIdentity::FState::SelectFaceVertex(FVector3f InOrigin, FVector3f InDirection, FVector3f& OutVertex, FVector3f& OutNormal)
{
	check(Impl->MHCState);

	Eigen::Vector3f Origin;
	Eigen::Vector3f Direction;
	Eigen::Vector3f Vertex;
	Eigen::Vector3f Normal;
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		Origin = Eigen::Vector3f(InOrigin[0], InOrigin[2], InOrigin[1]);
		Direction = Eigen::Vector3f(InDirection[0], InDirection[2], InDirection[1]);
	}
	else
	{
		Origin = Eigen::Vector3f(InOrigin[0], -InOrigin[1], InOrigin[2]);
		Direction = Eigen::Vector3f(InDirection[0], -InDirection[1], InDirection[2]);
	}
	int32 VertexID = static_cast<int32>(Impl->MHCState->SelectFaceVertex(Origin, Direction, Vertex, Normal));
	if (VertexID != INDEX_NONE)
	{
		if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
		{
			OutVertex = FVector3f{ Vertex[0], Vertex[2], Vertex[1]};
			OutNormal = FVector3f{ Normal[0], Normal[2], Normal[1] };
		}
		else
		{
			OutVertex = FVector3f{ Vertex[0], -Vertex[1], Vertex[2] };
			OutNormal = FVector3f{ Normal[0], -Normal[1], Normal[2] };
		}
	}
	return VertexID;
}

void FMetaHumanCharacterIdentity::FState::ResetNeckExclusionMask()
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->ResetNeckExclusionMask());
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::Reset()
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->Reset());
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::ResetNeckRegion()
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	int neckRegionIndex = Impl->MHCAPI->GetNeckRegionIndex();
	if (neckRegionIndex >= 0)
	{
		TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::BlendOptions BlendOptions;
		BlendOptions.Type = TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FaceAttribute::Both;
		BlendOptions.bBlendSymmetrically = true;
		BlendOptions.bBlendRelativeTranslation = false;
		ensure(NewMHCState->ResetRegion(neckRegionIndex, 1.0f, BlendOptions));
		Impl->MHCState = NewMHCState;
		Impl->CachedEvaluatedState.Reset();
	}
}

void FMetaHumanCharacterIdentity::FState::Randomize(float InMagnitude)
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->Randomize(InMagnitude));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::GetPreset(const FString& InPresetName, int32 InPresetType, int32 InPresetRegion)
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	//ensure(NewMHCState->SelectPreset(TCHAR_TO_UTF8(*InPresetName), static_cast<int>(InPresetType), static_cast<int>(InPresetRegion)));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::BlendPresets(int32 InGizmoIndex, const TArray<TPair<float, const FState*>>& InStates, EBlendOptions InBlendOptions, bool bInBlendSymmetrically)
{
	check(Impl->MHCState);

	if (InStates.Num() > 0)
	{
		std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
		std::vector<std::pair<float, const TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State*>> InnerStates;
		for (int32 PresetIndex = 0; PresetIndex < InStates.Num(); PresetIndex++)
		{
			std::pair<float, const TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State*> Preset{ InStates[PresetIndex].Key, InStates[PresetIndex].Value->Impl->MHCState.get() };
			InnerStates.emplace_back(Preset);
		}
		TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::BlendOptions BlendOptions;
		BlendOptions.Type = static_cast<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::FaceAttribute>(static_cast<int32>(InBlendOptions));
		BlendOptions.bBlendSymmetrically = bInBlendSymmetrically;
		BlendOptions.bBlendRelativeTranslation = (InGizmoIndex >= 0);
		ensure(NewMHCState->Blend(static_cast<int>(InGizmoIndex), InnerStates, BlendOptions));
		Impl->MHCState = NewMHCState;
		Impl->CachedEvaluatedState.Reset();
	}
}

void FMetaHumanCharacterIdentity::FState::SetGizmoPosition(int32 InGizmoIndex, const FVector3f& InPosition, bool bInSymmetric, bool bInEnforceBounds)
{
	check(Impl->MHCState);
	Eigen::Vector3f Position;
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		Position = Eigen::Vector3f{ InPosition[0], InPosition[2], InPosition[1] };
	}
	else
	{
		Position = Eigen::Vector3f{ InPosition[0], -InPosition[1], InPosition[2] };
	}
	TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::GizmoPositionOptions Options;
	Options.bEnforceBounds = bInEnforceBounds;
	Options.bSymmetric = bInSymmetric;

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	NewMHCState->SetGizmoPosition(InGizmoIndex, Position.data(), Options);
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
	
}

void FMetaHumanCharacterIdentity::FState::GetGizmoPosition(int32 InGizmoIndex, FVector3f& OutPosition) const
{
	check(Impl->MHCState);
	Eigen::Vector3f Position;
	Impl->MHCState->GetGizmoPosition(InGizmoIndex, Position.data());
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		OutPosition = FVector3f{ Position[0], Position[2], Position[1] };
	}
	else
	{
		OutPosition = FVector3f{ Position[0], -Position[1], Position[2] };
	}
}

void FMetaHumanCharacterIdentity::FState::GetGizmoPositionBounds(int32 InGizmoIndex, FVector3f& OutMinPosition, FVector3f& OutMaxPosition, float InBBoxReduction, bool bInExpandToCurrent) const\
{
	check(Impl->MHCState);
	Eigen::Vector3f MinPosition;
	Eigen::Vector3f MaxPosition;
	Impl->MHCState->GetGizmoPositionBounds(InGizmoIndex, MinPosition.data(), MaxPosition.data(), InBBoxReduction, bInExpandToCurrent);
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		OutMinPosition = FVector3f{ MinPosition[0], MinPosition[2], MinPosition[1] };
		OutMaxPosition = FVector3f{ MaxPosition[0], MaxPosition[2], MaxPosition[1] };
	}
	else
	{
		OutMinPosition = FVector3f{ MinPosition[0], -MaxPosition[1], MinPosition[2] };
		OutMaxPosition = FVector3f{ MaxPosition[0], -MinPosition[1], MaxPosition[2] };
	}
}

/*
void FMetaHumanCharacterIdentity::FState::RotateGizmo(int32 InGizmoIndex, const FVector3f& InDelta, bool bInSymmetric)
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	Eigen::Vector3f CurRegionRotation;
	NewMHCState->GetGizmoRotation(static_cast<int>(InGizmoIndex), CurRegionRotation.data());

	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		float RotationInDNACoord[3] = { CurRegionRotation.x() + InDelta.Z, CurRegionRotation.y() - InDelta.Y, CurRegionRotation.z() + InDelta.X};
		ensure(NewMHCState->SetGizmoRotation(static_cast<int>(InGizmoIndex), RotationInDNACoord, bInSymmetric));
	}
	else
	{
		float RotationInDNACoord[3] = { CurRegionRotation.x() + InDelta.X, -CurRegionRotation.y() - InDelta.Y, CurRegionRotation.z() + InDelta.Z };
		ensure(NewMHCState->SetGizmoRotation(static_cast<int>(InGizmoIndex), RotationInDNACoord, bInSymmetric));
	}
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}
*/

void FMetaHumanCharacterIdentity::FState::SetGizmoRotation(int32 InGizmoIndex, const FVector3f& InRotation, bool bInSymmetric, bool bInEnforceBounds)
{
	check(Impl->MHCState);
	Eigen::Vector3f Rotation;
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		Rotation = Eigen::Vector3f{ InRotation[0], -InRotation[2], InRotation[1] };
	}
	else
	{
		Rotation = Eigen::Vector3f{ InRotation[0], -InRotation[1], InRotation[2] };
	}
	TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::GizmoRotationOptions Options;
	Options.bEnforceBounds = bInEnforceBounds;
	Options.bSymmetric = bInSymmetric;

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	NewMHCState->SetGizmoRotation(InGizmoIndex, Rotation.data(), Options);
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();

}

void FMetaHumanCharacterIdentity::FState::GetGizmoRotation(int32 InGizmoIndex, FVector3f& OutRotation) const
{
	check(Impl->MHCState);
	Eigen::Vector3f Rotation;
	Impl->MHCState->GetGizmoRotation(InGizmoIndex, Rotation.data());
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		OutRotation = FVector3f{ Rotation[0], Rotation[2], -Rotation[1] };
	}
	else
	{
		OutRotation = FVector3f{ Rotation[0], -Rotation[1], Rotation[2] };
	}
}

void FMetaHumanCharacterIdentity::FState::GetGizmoRotationBounds(int32 InGizmoIndex, FVector3f& OutMinRotation, FVector3f& OutMaxRotation, bool bInExpandToCurrent) const\
{
	check(Impl->MHCState);
	Eigen::Vector3f MinRotation;
	Eigen::Vector3f MaxRotation;
	Impl->MHCState->GetGizmoRotationBounds(InGizmoIndex, MinRotation.data(), MaxRotation.data(), bInExpandToCurrent);
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		OutMinRotation = FVector3f{ MinRotation[0], MinRotation[2], -MaxRotation[1] };
		OutMaxRotation = FVector3f{ MaxRotation[0], MaxRotation[2], -MinRotation[1] };
	}
	else
	{
		OutMinRotation = FVector3f{ MinRotation[0], -MaxRotation[1], MinRotation[2] };
		OutMaxRotation = FVector3f{ MaxRotation[0], -MinRotation[1], MaxRotation[2] };
	}
}

void FMetaHumanCharacterIdentity::FState::SetGizmoScale(int32 InGizmoIndex, float InScale, bool bInSymmetric, bool bInEnforceBounds)
{
	check(Impl->MHCState);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State::GizmoScalingOptions Options;
	Options.bSymmetric = bInSymmetric;
	Options.bEnforceBounds = bInEnforceBounds;
	ensure(NewMHCState->SetGizmoScale(static_cast<int>(InGizmoIndex), InScale, Options));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::GetGizmoScale(int32 InGizmoIndex, float& OutScale) const
{
	check(Impl->MHCState);
	ensure(Impl->MHCState->GetGizmoScale(static_cast<int>(InGizmoIndex), OutScale));
}

void FMetaHumanCharacterIdentity::FState::GetGizmoScaleBounds(int32 InGizmoIndex, float& OutMinScale, float& OutMaxScale, bool bInExpandToCurrent) const
{
	check(Impl->MHCState);
	ensure(Impl->MHCState->GetGizmoScaleBounds(static_cast<int>(InGizmoIndex), OutMinScale, OutMaxScale, bInExpandToCurrent));
}

void FMetaHumanCharacterIdentity::FState::TranslateLandmark(int32 InLandmarkIndex, const FVector3f& InDelta, bool bInSymmetric)
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	if (Impl->InputDatabaseOrient == EMetaHumanCharacterOrientation::Y_UP)
	{
		float DeltaInDNACoord[3] = { InDelta.X, InDelta.Z, InDelta.Y };
		ensure(NewMHCState->TranslateLandmark(static_cast<int>(InLandmarkIndex), DeltaInDNACoord, bInSymmetric));
	}
	else
	{
		float DeltaInDNACoord[3] = { InDelta.X, -InDelta.Y, InDelta.Z };
		ensure(NewMHCState->TranslateLandmark(static_cast<int>(InLandmarkIndex), DeltaInDNACoord, bInSymmetric));
	}
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::SetBodyVertexNormals(const TArray<FVector3f>& InVertexNormals, const TArray<int32>& InNumVerticesPerLod)
{
	check(Impl->MHCState);

	Impl->BodyVertexNormals = InVertexNormals;
	Impl->BodyNumVerticesPerLod = InNumVerticesPerLod;
}

void FMetaHumanCharacterIdentity::FState::SetBodyJointsAndBodyFaceVertices(const TArray<FMatrix44f>& InBodyJoints, const TArray<FVector3f>& InVertices)
{
	check(Impl->MHCState);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->SetBodyJointsAndBodyFaceVertices((float*)InBodyJoints.GetData(), (float*)InVertices.GetData()));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::WriteDebugAutoriggingData(const FString& DirectoryPath) const
{
	check(Impl->MHCAPI);
	Impl->MHCState->DumpDataForAR(std::string(TCHAR_TO_UTF8(*DirectoryPath)));
}

FArchive& operator<<(FArchive& Ar, std::string& InStdString)
{
	int32 Length = static_cast<int32>(InStdString.size()); 
	Ar << Length;

	if (Ar.IsSaving())
	{
		Ar.Serialize(const_cast<char*>(InStdString.data()), Length);
	}
	else if (Ar.IsLoading())
	{
		int64 AvailableLength = Ar.TotalSize() - Ar.Tell();
		if (Length < 0 || Length > AvailableLength)
		{
			UE_LOG(LogMetaHumanCoreTechLib, Error, TEXT("failed to deserialize string"));
			Ar.SetError();
			return Ar;
		}

		InStdString.resize(Length);
		Ar.Serialize(InStdString.data(), Length);
	}

	return Ar;
}

void FMetaHumanCharacterIdentity::FState::Serialize(FSharedBuffer& OutArchive) const
{
	check(Impl->MHCState);

	// serialize state
	pma::ScopedPtr<dna::MemoryStream> MemStream = pma::makeScoped<dna::MemoryStream>();
	Impl->MHCState->Serialize(MemStream.get());
	MemStream->seek(0);

	// create bytearray from serialized state, and append rest of the data
	TArray<uint8> ByteArray;
	ByteArray.AddUninitialized(MemStream->size());
	MemStream->read((char*)ByteArray.GetData(), MemStream->size());
			
	FMemoryWriter Writer(ByteArray, /* bIsPersistent */ true, /* bSetOffset */ true);
	Writer << Impl->InputDatabaseOrient;
	Writer << Impl->BodyVertexNormals;
	Writer << Impl->BodyNumVerticesPerLod;
	OutArchive = FSharedBuffer::Clone(ByteArray.GetData(), ByteArray.Num());
}

bool FMetaHumanCharacterIdentity::FState::Deserialize(const FSharedBuffer& InArchive)
{
	check(Impl->MHCState);

	if (!InArchive.GetSize())
	{
		return false;
	}

	const int32 BufferSize = InArchive.GetSize();

	pma::ScopedPtr<dna::MemoryStream> MemStream = pma::makeScoped<dna::MemoryStream>();
	MemStream->write((char*)InArchive.GetData(), InArchive.GetSize());
	MemStream->seek(0);

	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	if (NewMHCState->Deserialize(MemStream.get()))
	{
		uint64 MemPos = MemStream->tell();

		TArray<uint8> ByteArray;
		ByteArray.SetNumUninitialized(BufferSize - MemPos);

		FMemoryReader Reader(ByteArray, /* bIsPersistent */ true);
		FMemory::Memcpy(ByteArray.GetData(), (const uint8*)InArchive.GetData() + MemPos, BufferSize - MemPos);

		Reader << Impl->InputDatabaseOrient;
		Reader << Impl->BodyVertexNormals;
		Reader << Impl->BodyNumVerticesPerLod;

		bool bOk = !Reader.IsError();

		if (bOk)
		{
			Impl->MHCState = NewMHCState;
			Impl->CachedEvaluatedState.Reset();
		}
		return bOk;
	}
	else
	{
		// revert back to string
		TArray<uint8> ByteArray;
		ByteArray.SetNumUninitialized(BufferSize);

		FMemoryReader Reader(ByteArray, /* bIsPersistent */ true);
		FMemory::Memcpy(ByteArray.GetData(), InArchive.GetData(), BufferSize);

		std::string StdStringValue;
		Reader << StdStringValue;
		bool bOk = NewMHCState->Deserialize(StdStringValue);
		
		Reader << Impl->InputDatabaseOrient;
		Reader << Impl->BodyVertexNormals;
		Reader << Impl->BodyNumVerticesPerLod;
		bOk = bOk && !Reader.IsError();

		if (bOk)
		{
			Impl->MHCState = NewMHCState;
			Impl->CachedEvaluatedState.Reset();
		}
		return bOk;
	}
}

FMetaHumanCharacterIdentity::FSettings FMetaHumanCharacterIdentity::FState::GetSettings() const
{
	check(Impl->MHCState);
	FSettings Settings;
	Settings.Impl->MHCSettings = Impl->MHCState->GetSettings();
	return Settings;
}

void FMetaHumanCharacterIdentity::FState::SetSettings(const FSettings& InSettings)
{
	check(Impl->MHCState);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::Settings> NewSettings = InSettings.Impl->MHCSettings->Clone();
	NewMHCState->SetSettings(NewSettings);
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

bool FMetaHumanCharacterIdentity::FState::GetGlobalScale(float& scale) const
{
	check(Impl->MHCState);
	return Impl->MHCState->GetGlobalScale(scale);
}

void FMetaHumanCharacterIdentity::FState::SetVariant(const FString & InVariantName, TConstArrayView<float> InVariantWeights)
{
	check(Impl->MHCState);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->SetVariant(TCHAR_TO_UTF8(*InVariantName), InVariantWeights.GetData()));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

void FMetaHumanCharacterIdentity::FState::SetExpressionActivations(TMap<FString, float>& InExpressionActivations)
{
	check(Impl->MHCState);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	std::map<std::string, float> ExpressionActivations;
	for (const TPair<FString, float>& ExpressionActivation : InExpressionActivations)
	{
		ExpressionActivations[TCHAR_TO_UTF8(*ExpressionActivation.Key)] = ExpressionActivation.Value;
	}
	ensure(NewMHCState->SetExpressionActivations(ExpressionActivations));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}


int32 FMetaHumanCharacterIdentity::FState::GetVariantsCount(const FString& InVariantName) const
{
	check(Impl->MHCAPI);
	return Impl->MHCAPI->GetVariantNames(TCHAR_TO_UTF8(*InVariantName)).size();
}


int32 FMetaHumanCharacterIdentity::FState::GetNumHighFrequencyVariants() const
{
	check(Impl->MHCAPI);
	return static_cast<int32>(Impl->MHCAPI->NumHFVariants());
}

void FMetaHumanCharacterIdentity::FState::SetHighFrequenctVariant(int32 InHighFrequencyVariant)
{
	check(Impl->MHCState);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->SetHFVariant(static_cast<int>(InHighFrequencyVariant)));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

float FMetaHumanCharacterIdentity::FState::GetFaceScale() const
{
	check(Impl->MHCState);
	float Scale = 1.0f;
	ensure(Impl->MHCState->GetFaceScale(Scale));
	return Scale;
}

void FMetaHumanCharacterIdentity::FState::SetFaceScale(float InFaceScale)
{
	check(Impl->MHCState);
	std::shared_ptr<TITAN_API_NAMESPACE::MetaHumanCreatorAPI::State> NewMHCState = Impl->MHCState->Clone();
	ensure(NewMHCState->SetFaceScale(InFaceScale));
	Impl->MHCState = NewMHCState;
	Impl->CachedEvaluatedState.Reset();
}

int32 FMetaHumanCharacterIdentity::FState::GetHighFrequenctVariant() const
{
	check(Impl->MHCState);
	return Impl->MHCState->GetHFVariant();
}

TSharedRef<IDNAReader> FMetaHumanCharacterIdentity::FState::StateToDna(dna::Reader* InDnaReader) const
{
	check(Impl->MHCState);

	pma::ScopedPtr<dna::MemoryStream> OutputStream = pma::makeScoped<dna::MemoryStream>();
	pma::ScopedPtr<dna::BinaryStreamWriter> DnaWriter = pma::makeScoped<dna::BinaryStreamWriter>(OutputStream.get());
	DnaWriter.get()->setFrom(InDnaReader);

	Impl->MHCAPI->StateToDna(*(Impl->MHCState), DnaWriter.get());
	DnaWriter->write();

	pma::ScopedPtr<dna::BinaryStreamReader> StateDnaReader = pma::makeScoped<dna::BinaryStreamReader>(OutputStream.get());
	StateDnaReader->read();

	return MakeShared<FDNAReader<dna::BinaryStreamReader>>(StateDnaReader.release());
}

TSharedRef<class IDNAReader> FMetaHumanCharacterIdentity::FState::StateToDna(class UDNAAsset* InFaceDNA) const
{
	pma::ScopedPtr<dna::MemoryStream> MemoryStream = pma::makeScoped<dna::MemoryStream>();
	pma::ScopedPtr<dna::BinaryStreamWriter> DnaWriter = pma::makeScoped<dna::BinaryStreamWriter>(MemoryStream.get());

	DnaWriter->setFrom(InFaceDNA->GetBehaviorReader()->Unwrap(), dna::DataLayer::All);
#if WITH_EDITORONLY_DATA
	DnaWriter->setFrom(InFaceDNA->GetGeometryReader()->Unwrap(), dna::DataLayer::Geometry);
#endif
	DnaWriter->write();

	pma::ScopedPtr<dna::BinaryStreamReader> BinaryDnaReader = pma::makeScoped<dna::BinaryStreamReader>(MemoryStream.get());
	BinaryDnaReader->read();

	return StateToDna(BinaryDnaReader.get());
}

