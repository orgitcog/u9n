// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanConformer.h"
#include "FrameTrackingContourData.h"
#include "api/ActorCreationAPI.h"
#include "api/ActorRefinementAPI.h"
#include "DNAAsset.h"
#include "DNAReader.h"
#include <rig/RigLogicDNAResource.h>
#include "FReader.h"
#include "dna/BinaryStreamWriter.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "MetaHumanIdentityErrorCode.h"


namespace UE
{
namespace Wrappers
{
	static const int32_t NumElementsFace = 72147;
	static const int32_t NumElementsEye = 2310;
	static const int32_t NumElementsTeeth = 12738;

	struct FMetaHumanConformer::Private
	{
		TITAN_API_NAMESPACE::ActorCreationAPI CreationAPI;
		int32 NumInputs = 0;
		TITAN_API_NAMESPACE::ActorRefinementAPI RefinementAPI;
		FString FrontalCameraName = {};
	};

	FMetaHumanConformer::FMetaHumanConformer()
	{
		Impl = MakePimpl<Private>();
	}

	bool FMetaHumanConformer::Init(const FString& InTemplateDescriptionJson, const FString& InIdentityModelJson, const FString& InFittingConfigurationJson)
	{
		bool bInit = Impl->CreationAPI.Init(TCHAR_TO_ANSI(*InTemplateDescriptionJson), TCHAR_TO_ANSI(*InIdentityModelJson));
		if (!bInit)
		{
			return false;
		}

		return Impl->CreationAPI.LoadFittingConfigurations(TCHAR_TO_ANSI(*InFittingConfigurationJson));
	}

	bool FMetaHumanConformer::SetDepthInputData(const TMap<FString, const FFrameTrackingContourData*>& InLandmarksDataPerCamera,
		const TMap<FString, const float*>& InDepthMaps)
	{
		const FScopeLock ScopeLock(&AccessMutex);
		std::map<std::string, const unsigned char*> ImageDataMap;
		std::map<std::string, const std::map<std::string, TITAN_API_NAMESPACE::FaceTrackingLandmarkData>> LandmarkMap;
		std::map<std::string, const float*> DepthMapDataMap;
		Impl->NumInputs += InDepthMaps.Num();


		if (Impl->FrontalCameraName.IsEmpty())
		{
			if (!InLandmarksDataPerCamera.IsEmpty())
			{
				Impl->FrontalCameraName = InLandmarksDataPerCamera.CreateConstIterator()->Key;
			}
			else
			{
				return false; // no data
			}
		}

		for (const TPair<FString, const float*>& DpthForCamera : InDepthMaps)
		{
			DepthMapDataMap[TCHAR_TO_ANSI(*DpthForCamera.Key)] = DpthForCamera.Value;
		}

		for (const TPair<FString, const FFrameTrackingContourData*>& LandmarksForCamera : InLandmarksDataPerCamera)
		{
			std::map<std::string, TITAN_API_NAMESPACE::FaceTrackingLandmarkData> LandmarkMapForCamera;

			for (const TPair<FString, FTrackingContour>& Landmarks : LandmarksForCamera.Value->TrackingContours)
			{
				if (Landmarks.Value.State.bActive)
				{
					std::vector<float> ComponentValues;
					ComponentValues.reserve(Landmarks.Value.DensePoints.Num() * 2);

					for (const FVector2D& Point : Landmarks.Value.DensePoints)
					{
						ComponentValues.push_back(Point.X);
						ComponentValues.push_back(Point.Y);
					}

					LandmarkMapForCamera[TCHAR_TO_ANSI(*Landmarks.Key)] = TITAN_API_NAMESPACE::FaceTrackingLandmarkData::Create(ComponentValues.data(), nullptr, Landmarks.Value.DensePoints.Num(), 2);
				}
			}

			LandmarkMap.emplace(std::make_pair(TCHAR_TO_ANSI(*LandmarksForCamera.Key), LandmarkMapForCamera));
		}

		return Impl->CreationAPI.SetDepthInputData(LandmarkMap, DepthMapDataMap);
	}

	bool FMetaHumanConformer::SetScanInputData(const TSortedMap<FString, const FFrameTrackingContourData*>& InLandmarks2DData,
		const TSortedMap<FString, const FTrackingContour3D*>& InLandmarks3DData,
		const TArray<int32_t>& InTrianlges, const TArray<float>& InVertices, bool& bOutInvalidMeshTopology)
	{
		const FScopeLock ScopeLock(&AccessMutex);
		std::map<std::string, const std::map<std::string, TITAN_API_NAMESPACE::FaceTrackingLandmarkData>> Landmark2DMap{};
		Impl->NumInputs = 1;

		if (Impl->FrontalCameraName.IsEmpty())
		{
			if (!InLandmarks2DData.IsEmpty())
			{
				Impl->FrontalCameraName = InLandmarks2DData.CreateConstIterator()->Key;
			}
			else if (!InLandmarks3DData.IsEmpty())
			{
				Impl->FrontalCameraName = InLandmarks3DData.CreateConstIterator()->Key;
			}
			else
			{
				return false; // no data
			}
		}

		for (const TPair<FString, const FFrameTrackingContourData*>& LandmarksForCamera : InLandmarks2DData)
		{
			std::map<std::string, TITAN_API_NAMESPACE::FaceTrackingLandmarkData> LandmarkMapForCamera;

			for (const TPair<FString, FTrackingContour>& Landmarks : LandmarksForCamera.Value->TrackingContours)
			{
				if(Landmarks.Value.State.bActive)
				{
					std::vector<float> ComponentValues;
					ComponentValues.reserve(Landmarks.Value.DensePoints.Num() * 2);

					for (const FVector2D& Point : Landmarks.Value.DensePoints)
					{
						ComponentValues.push_back(Point.X);
						ComponentValues.push_back(Point.Y);
					}

					LandmarkMapForCamera[TCHAR_TO_ANSI(*Landmarks.Key)] = TITAN_API_NAMESPACE::FaceTrackingLandmarkData::Create(ComponentValues.data(), nullptr, Landmarks.Value.DensePoints.Num(),2);
				}
			}

			Landmark2DMap.emplace(std::make_pair(TCHAR_TO_ANSI(*LandmarksForCamera.Key), LandmarkMapForCamera));
		}

		std::map<std::string, const TITAN_API_NAMESPACE::FaceTrackingLandmarkData> Landmark3DMap{};

		for (const TPair<FString, const FTrackingContour3D*>& Landmarks : InLandmarks3DData)
		{
			
				std::vector<float> ComponentValues;
				ComponentValues.reserve(Landmarks.Value->DensePoints.Num() * 3);

				for (const FVector3d& Point : Landmarks.Value->DensePoints)
				{
					ComponentValues.push_back(Point.X);
					ComponentValues.push_back(Point.Y);
					ComponentValues.push_back(Point.Z);
				}

				Landmark3DMap.emplace(std::make_pair(TCHAR_TO_ANSI(*Landmarks.Key),
						TITAN_API_NAMESPACE::FaceTrackingLandmarkData::Create(ComponentValues.data(), nullptr, Landmarks.Value->DensePoints.Num(), 3)));
		}
		
		TITAN_API_NAMESPACE::MeshInputData MeshInputData{ InTrianlges.Num() / 3,InTrianlges.GetData(), InVertices.Num() / 3, InVertices.GetData()};
		return Impl->CreationAPI.SetScanInputData(Landmark3DMap, Landmark2DMap, MeshInputData, bOutInvalidMeshTopology);
	}

	bool FMetaHumanConformer::SetCameras(const TArray<FCameraCalibration>& InCalibrations)
	{
		std::map<std::string, TITAN_API_NAMESPACE::OpenCVCamera> Cameras;

		for (const FCameraCalibration& Calibration : InCalibrations)
		{
			TITAN_API_NAMESPACE::OpenCVCamera Camera;
			Camera.width = Calibration.ImageSize.X;
			Camera.height = Calibration.ImageSize.Y;
			Camera.fx = Calibration.FocalLength.X;
			Camera.fy = Calibration.FocalLength.Y;
			Camera.cx = Calibration.PrincipalPoint.X;
			Camera.cy = Calibration.PrincipalPoint.Y;
			Camera.k1 = Calibration.K1;
			Camera.k2 = Calibration.K2;
			Camera.k3 = Calibration.K3;
			Camera.p1 = Calibration.P1;
			Camera.p2 = Calibration.P2;

			//! Transform from world coordinates to camera coordinates in column-major format.
			for (int32 Row = 0; Row < 4; Row++)
			{
				for (int32 Col = 0; Col < 4; Col++)
				{
					Camera.Extrinsics[Row * 4 + Col] = Calibration.Transform.M[Row][Col];
				}
			}

			check(Cameras.find(TCHAR_TO_ANSI(*Calibration.CameraId)) == Cameras.end());
			Cameras[TCHAR_TO_ANSI(*Calibration.CameraId)] = Camera;
		}

		return Impl->CreationAPI.SetCameras(Cameras);
	}



	EIdentityErrorCode FMetaHumanConformer::FitIdentity(TArray<float>& OutVerticesFace, TArray<float>& OutVerticesLeftEye, TArray<float>& OutVerticesRightEye, TArray<float>& OutStackedToScanTransforms, TArray<float>& OutStackedToScanScales, bool bInFitEyes, const FString& InDebuggingDataFolder)
	{
		if (!InDebuggingDataFolder.IsEmpty())
		{
			Impl->CreationAPI.SaveDebuggingData(TCHAR_TO_ANSI(*InDebuggingDataFolder));
		}
	
		if (OutVerticesFace.Num() != NumElementsFace)
		{
			OutVerticesFace.SetNum(NumElementsFace);
		}

		if (OutStackedToScanTransforms.Num() != Impl->NumInputs * 16)
		{
			OutStackedToScanTransforms.SetNum(Impl->NumInputs * 16);
		}

		if (OutStackedToScanScales.Num() != Impl->NumInputs)
		{
			OutStackedToScanScales.SetNum(Impl->NumInputs);
		}

		const int32 NumNonRigidFitIterations = 5;

		// Only execute the next steps if the previous one didn't fail
		// note that when autoMode is true, the number of iterations is chosen by the API and the value passed in does not matter
		bool bResult = Impl->CreationAPI.FitRigid(OutVerticesFace.GetData(), OutStackedToScanTransforms.GetData(), OutStackedToScanScales.GetData(), 10, /*autoMode=*/true);
		EIdentityErrorCode Result = EIdentityErrorCode::None;

		if (bResult)
		{
			bResult = Impl->CreationAPI.FitNonRigid(OutVerticesFace.GetData(), OutStackedToScanTransforms.GetData(), OutStackedToScanScales.GetData(), 10, /*autoMode=*/true);
		}

		Impl->CreationAPI.SetAutoMultiViewLandmarkMasking(true);
		if (bResult)
		{
			bResult = Impl->CreationAPI.FitNonRigid(OutVerticesFace.GetData(), OutStackedToScanTransforms.GetData(), OutStackedToScanScales.GetData(), NumNonRigidFitIterations, /*autoMode=*/false);
		}


		if (bInFitEyes)
		{
			if (OutVerticesLeftEye.Num() != NumElementsEye)
			{
				OutVerticesLeftEye.SetNum(NumElementsEye);
			}
			if (OutVerticesRightEye.Num() != NumElementsEye)
			{
				OutVerticesRightEye.SetNum(NumElementsEye);
			}

			const int32 NumFitEyesIterations = 10;
			// do a two-stage eye fit for robustness
			if (bResult)
			{
				bResult = Impl->CreationAPI.FitEyes(OutVerticesLeftEye.GetData(), OutVerticesRightEye.GetData(), true, NumFitEyesIterations);
			}

			if (bResult)
			{
				bResult = Impl->CreationAPI.CalculateAndUpdateScanMask(TCHAR_TO_ANSI(*Impl->FrontalCameraName), TITAN_API_NAMESPACE::ScanMaskType::EYE_FITTING);
			}

			if (bResult)
			{
				// call eye fitting again with the specific eye mask and save the conformed eye meshes if we have debugging turned on
				bResult = Impl->CreationAPI.FitEyes(OutVerticesLeftEye.GetData(), OutVerticesRightEye.GetData(), true, NumFitEyesIterations, 
					true, TCHAR_TO_ANSI(*InDebuggingDataFolder));
			}

			// set the mask back to the global mask
			if (bResult)
			{
				bResult = Impl->CreationAPI.CalculateAndUpdateScanMask(TCHAR_TO_ANSI(*Impl->FrontalCameraName), TITAN_API_NAMESPACE::ScanMaskType::GLOBAL);
			}

			// finally call PerVertex fit again
			if (bResult)
			{
				bResult = Impl->CreationAPI.FitPerVertex(OutVerticesFace.GetData(), OutStackedToScanTransforms.GetData(), OutStackedToScanScales.GetData(), 15, TCHAR_TO_ANSI(*InDebuggingDataFolder));
			}

			if (!bResult)
			{
				Result = EIdentityErrorCode::FitEyesFailed;
			}
		}
		else
		{
			// perform per-vertex fit here if we aren't fitting eyes
			if (bResult)
			{
				bResult = Impl->CreationAPI.FitPerVertex(OutVerticesFace.GetData(), OutStackedToScanTransforms.GetData(), OutStackedToScanScales.GetData(), 3, TCHAR_TO_ANSI(*InDebuggingDataFolder));
			}

			OutVerticesLeftEye = {};
			OutVerticesRightEye = {};

			if (!bResult)
			{
				Result = EIdentityErrorCode::SolveFailed;
			}
		}

		return Result;
	}

	bool FMetaHumanConformer::UpdateTeethSource(const TArray<uint8>& InDNA)
	{
		pma::ScopedPtr<dna::MemoryStream> InStream = pma::makeScoped<dna::MemoryStream>();
		InStream->write((const char*)InDNA.GetData(), (InDNA.Num() * sizeof(uint8)) / sizeof(char));

		pma::ScopedPtr<dna::BinaryStreamReader> Reader = pma::makeScoped<dna::BinaryStreamReader>(InStream.get());
		Reader->read();
		dna::BinaryStreamReader* InputDna = Reader.get();

		std::vector<dna::Position> TeethVerticesFromDna;
		for (uint32_t Vertex = 0; Vertex < InputDna->getVertexPositionCount(1); ++Vertex) {
			TeethVerticesFromDna.push_back(InputDna->getVertexPosition(1, Vertex));
		}
		return Impl->CreationAPI.UpdateTeethSource((const float*)TeethVerticesFromDna.data());
	}

	bool  FMetaHumanConformer::CalcTeethDepthDelta(float InDeltaDistanceFromCamera, float& OutDx, float& OutDy, float& OutDz)
	{
		return Impl->CreationAPI.CalcTeethDepthDelta(InDeltaDistanceFromCamera, OutDx, OutDy, OutDz);
	}


	bool FMetaHumanConformer::FitTeeth(TArray<float>& OutVerticesTeeth, const FString& InDebuggingDataFolder)
	{
		if (!InDebuggingDataFolder.IsEmpty())
		{
			Impl->CreationAPI.SaveDebuggingData(TCHAR_TO_ANSI(*InDebuggingDataFolder));
		}

		if (OutVerticesTeeth.Num() != NumElementsTeeth)
		{
			OutVerticesTeeth.SetNum(NumElementsTeeth);
		}

		return Impl->CreationAPI.FitTeeth(OutVerticesTeeth.GetData(), 3, TCHAR_TO_ANSI(*InDebuggingDataFolder));
	}

	bool FMetaHumanConformer::UpdateRigWithTeethMeshVertices(const TArray<uint8>& InDNA, const TArray<float>& InVertices, TArray<uint8>& OutDNA)
	{
		pma::ScopedPtr<dna::MemoryStream> InStream = pma::makeScoped<dna::MemoryStream>();
		InStream->write((const char*)InDNA.GetData(), (InDNA.Num() * sizeof(uint8)) / sizeof(char));

		pma::ScopedPtr<dna::BinaryStreamReader> Reader = pma::makeScoped<dna::BinaryStreamReader>(InStream.get());
		Reader->read();

		pma::ScopedPtr<dna::MemoryStream> OutStream = pma::makeScoped<trio::MemoryStream>();
		pma::ScopedPtr<dna::BinaryStreamWriter> Writer = pma::makeScoped<dna::BinaryStreamWriter>(OutStream.get());
		Writer->setFrom(Reader.get());

		const int32_t NumVertices = NumElementsFace / 3;
		std::vector<float> MaskData(NumVertices);
		bool bIsOK = Impl->CreationAPI.GetFittingMask(MaskData.data(), TITAN_API_NAMESPACE::FittingMaskType::MOUTH_SOCKET);

		if (bIsOK)
		{
			bIsOK = Impl->RefinementAPI.SetRefinementMask(NumVertices, MaskData.data(), TITAN_API_NAMESPACE::RefinementMaskType::MOUTH_SOCKET);

			if (bIsOK)
			{
				bIsOK = Impl->RefinementAPI.UpdateRigWithTeethMeshVertices(Reader.get(), InVertices.GetData(), Writer.get());

				if (bIsOK)
				{
					Writer->write();
					OutDNA.SetNumUninitialized((OutStream->size() * sizeof(char)) / sizeof(uint8));
					OutStream->read((char*)OutDNA.GetData(), OutStream->size());
				}
			}
		}

		return bIsOK;
	}

	bool FMetaHumanConformer::ResetInputData()
	{
		Impl->NumInputs = 0;

		return Impl->CreationAPI.ResetInputData();
	}

	bool FMetaHumanConformer::GenerateBrowMeshLandmarks(const FString& InCameraName, TArray<uint8>& OutJsonStream, bool bInConcatenate) const
	{
		std::string JsonString;
		bool bIsOK = Impl->CreationAPI.GenerateBrowMeshLandmarks(TCHAR_TO_UTF8(*InCameraName), JsonString, bInConcatenate);

		if (bIsOK)
		{
			OutJsonStream.SetNum(JsonString.length());
			memcpy(OutJsonStream.GetData(), JsonString.c_str(), JsonString.length());
		}
		else
		{
			OutJsonStream.Reset();
		}

		return bIsOK;
	}

	bool FMetaHumanConformer::CheckPcaModelFromDnaRigConfig(const FString& InConfigurationJson, UDNAAsset* InDNAAsset)
	{
		TArray<uint8> DNA = DNAToBuffer(InDNAAsset);

		pma::ScopedPtr<dna::MemoryStream> InStream = pma::makeScoped<dna::MemoryStream>();
		InStream->write((const char*)DNA.GetData(), (DNA.Num() * sizeof(uint8)) / sizeof(char));
		pma::ScopedPtr<dna::BinaryStreamReader> Reader = pma::makeScoped<dna::BinaryStreamReader>(InStream.get());
		Reader->read();

		return (TITAN_API_NAMESPACE::ActorCreationAPI::CheckPcaModelFromDnaRigConfig(TCHAR_TO_UTF8(*InConfigurationJson), Reader.get()));
	}

	bool FMetaHumanConformer::CalculatePcaModelFromDnaRig(const FString& InConfigurationJson, const TArray<uint8>& InDNABuffer, TArray<uint8>& OutPCARigMemoryBuffer, const FString& InDebuggingDataFolder)
	{
		pma::ScopedPtr<dna::MemoryStream> InStream = pma::makeScoped<dna::MemoryStream>();
		InStream->write((const char*)InDNABuffer.GetData(), (InDNABuffer.Num() * sizeof(uint8)) / sizeof(char));
		pma::ScopedPtr<dna::BinaryStreamReader> Reader = pma::makeScoped<dna::BinaryStreamReader>(InStream.get());
		Reader->read();

		pma::ScopedPtr<dna::MemoryStream> OutStream = pma::makeScoped<trio::MemoryStream>();
		pma::ScopedPtr<dna::BinaryStreamWriter> Writer = pma::makeScoped<dna::BinaryStreamWriter>(OutStream.get());


		if (TITAN_API_NAMESPACE::ActorCreationAPI::CalculatePcaModelFromDnaRig(TCHAR_TO_UTF8(*InConfigurationJson), Reader.get(), Writer.get(), TCHAR_TO_ANSI(*InDebuggingDataFolder)))
		{
			Writer->write();
			OutPCARigMemoryBuffer.SetNumUninitialized((OutStream->size() * sizeof(char)) / sizeof(uint8));
			OutStream->read((char*)OutPCARigMemoryBuffer.GetData(), OutStream->size());
			return true;
		}
		else
		{
			return false;
		}

	}

	bool FMetaHumanConformer::CalculatePcaModelFromDnaRig(const FString& InConfigurationJson, const FString& InDNAFilename, TArray<uint8>& OutPCARigMemoryBuffer)
	{
		std::string DNAFilename = std::string(TCHAR_TO_UTF8(*InDNAFilename));
		pma::ScopedPtr<dna::FileStream> ReaderStream = pma::makeScoped<dna::FileStream>(DNAFilename.c_str(), dna::FileStream::AccessMode::Read, dna::FileStream::OpenMode::Binary);
		pma::ScopedPtr<dna::BinaryStreamReader> Reader = pma::makeScoped<dna::BinaryStreamReader>(ReaderStream.get());
		Reader->read();

		pma::ScopedPtr<dna::MemoryStream> WriterStream = pma::makeScoped<trio::MemoryStream>();
		pma::ScopedPtr<dna::BinaryStreamWriter> Writer = pma::makeScoped<dna::BinaryStreamWriter>(WriterStream.get());

		if (TITAN_API_NAMESPACE::ActorCreationAPI::CalculatePcaModelFromDnaRig(TCHAR_TO_UTF8(*InConfigurationJson), Reader.get(), Writer.get()))
		{
			Writer->write();
			OutPCARigMemoryBuffer.SetNumUninitialized((WriterStream->size() * sizeof(char)) / sizeof(uint8));
			WriterStream->read((char*)OutPCARigMemoryBuffer.GetData(), WriterStream->size());
			return true;
		}
		else
		{
			return false;
		}
	}

	bool FMetaHumanConformer::FitRigid(TArray<float>& OutVerticesFace, TArray<float>& OutStackedToScanTranform, TArray<float>& OutStackedToScanScale, int32 InIterations)
	{
		if (OutVerticesFace.Num() != NumElementsFace)
		{
			OutVerticesFace.SetNum(NumElementsFace);
		}

		if (OutStackedToScanTranform.Num() != Impl->NumInputs * 16)
		{
			OutStackedToScanTranform.SetNum(Impl->NumInputs * 16);
		}

		if (OutStackedToScanScale.Num() != Impl->NumInputs)
		{
			OutStackedToScanScale.SetNum(Impl->NumInputs);
		}

		return Impl->CreationAPI.FitRigid(OutVerticesFace.GetData(), OutStackedToScanTranform.GetData(), OutStackedToScanScale.GetData(), InIterations);
	}

	bool FMetaHumanConformer::FitPcaRig(const TArray<uint8>& InPcaRig, const TArray<uint8>& InNeutralDNABuffer, TArray<float>& OutVerticesFace, TArray<float>& OutStackedToScanTranform, TArray<float>& OutStackedToScanScale, const FString& InDebuggingDataFolder)
	{

		// get the neutral face vertices from the neutral pose DNA
		TArray<float> NeutralFaceVertices;
		pma::ScopedPtr<dna::MemoryStream> InStream = pma::makeScoped<dna::MemoryStream>();
		InStream->write((const char*)InNeutralDNABuffer.GetData(), (InNeutralDNABuffer.Num() * sizeof(uint8)) / sizeof(char));

		pma::ScopedPtr<dna::BinaryStreamReader> Reader = pma::makeScoped<dna::BinaryStreamReader>(InStream.get());
		Reader->read();
		dna::BinaryStreamReader* InputDna = Reader.get();

		NeutralFaceVertices.SetNumUninitialized(NumElementsFace);
		std::vector<dna::Position> TeethVerticesFromDna;
		for (uint32_t Vertex = 0; Vertex < InputDna->getVertexPositionCount(0); ++Vertex) {
			dna::Position CurVertex = InputDna->getVertexPosition(0, Vertex);
			NeutralFaceVertices[Vertex * 3] = CurVertex.x;
			NeutralFaceVertices[Vertex * 3 + 1] = CurVertex.y;
			NeutralFaceVertices[Vertex * 3 + 2] = CurVertex.z;
		}


		pma::ScopedPtr<dna::MemoryStream> DNAStream = pma::makeScoped<dna::MemoryStream>();
		DNAStream->write((const char*)InPcaRig.GetData(), (InPcaRig.Num() * sizeof(uint8)) / sizeof(char));

		pma::ScopedPtr<dna::BinaryStreamReader> DNAReader = pma::makeScoped<dna::BinaryStreamReader>(DNAStream.get());
		DNAReader->read();

		if (OutVerticesFace.Num() != NumElementsFace)
		{
			OutVerticesFace.SetNum(NumElementsFace);
		}

		if (OutStackedToScanTranform.Num() != Impl->NumInputs * 16)
		{
			OutStackedToScanTranform.SetNum(Impl->NumInputs * 16);
		}

		if (OutStackedToScanScale.Num() != Impl->NumInputs)
		{
			OutStackedToScanScale.SetNum(Impl->NumInputs);
		}

		return Impl->CreationAPI.FitPcaRig(DNAReader.get(), OutVerticesFace.GetData(), OutStackedToScanTranform.GetData(), OutStackedToScanScale.GetData(), NeutralFaceVertices.GetData(), 3, TCHAR_TO_ANSI(*InDebuggingDataFolder));
	}

	bool FMetaHumanConformer::SetModelRegularization(float InValue)
	{
		Impl->CreationAPI.SetModelRegularization(InValue);

		return true;
	}


	bool FMetaHumanConformer::ApplyDeltaDNA(const TArray<uint8>& InRawDNABuffer, const TArray<uint8>& InRawDeltaDNABuffer, TArray<uint8>& OutRawCombinedUnscaledDNABuffer) const
	{
		pma::ScopedPtr<dna::MemoryStream> DNAStream = pma::makeScoped<dna::MemoryStream>();
		DNAStream->write((const char*)InRawDNABuffer.GetData(), (InRawDNABuffer.Num() * sizeof(uint8)) / sizeof(char));
		pma::ScopedPtr<dna::BinaryStreamReader> DNAReader = pma::makeScoped<dna::BinaryStreamReader>(DNAStream.get());
		DNAReader->read();

		pma::ScopedPtr<dna::MemoryStream> DeltaDNAStream = pma::makeScoped<dna::MemoryStream>();
		DeltaDNAStream->write((const char*)InRawDeltaDNABuffer.GetData(), (InRawDeltaDNABuffer.Num() * sizeof(uint8)) / sizeof(char));
		pma::ScopedPtr<dna::BinaryStreamReader> DeltaDNAReader = pma::makeScoped<dna::BinaryStreamReader>(DeltaDNAStream.get());
		DeltaDNAReader->read();

		pma::ScopedPtr<dna::MemoryStream> DNAOutStream = pma::makeScoped<dna::MemoryStream>();
		pma::AlignedMemoryResource MemResOut;
		pma::ScopedPtr<dna::BinaryStreamWriter> DNAWriter = pma::makeScoped<dna::BinaryStreamWriter>(DNAOutStream.get(), &MemResOut);

		if (Impl->RefinementAPI.ApplyDNA(DNAReader.get(), DeltaDNAReader.get(), DNAWriter.get()))
		{
			DNAWriter->write();
			OutRawCombinedUnscaledDNABuffer.SetNumUninitialized((DNAOutStream->size() * sizeof(char)) / sizeof(uint8));
			DNAOutStream->read((char*)OutRawCombinedUnscaledDNABuffer.GetData(), DNAOutStream->size());
			return true;
		}
		else
		{
			return false;
		}
	}


	bool FMetaHumanConformer::ApplyScaleToDNA(const TArray<uint8>& InRawDNABuffer, float InScale, const FVector& InScalingPivot, TArray<uint8>& OutRawScaledDNABuffer) const
	{
		pma::ScopedPtr<dna::MemoryStream> DNAStream = pma::makeScoped<dna::MemoryStream>();
		DNAStream->write((const char*)InRawDNABuffer.GetData(), (InRawDNABuffer.Num() * sizeof(uint8)) / sizeof(char));
		pma::ScopedPtr<dna::BinaryStreamReader> DNAReader = pma::makeScoped<dna::BinaryStreamReader>(DNAStream.get());
		DNAReader->read();

		float Pivot[3] = { InScalingPivot.X, InScalingPivot.Y, InScalingPivot.Z };

		pma::ScopedPtr<dna::MemoryStream> DNAOutStream = pma::makeScoped<dna::MemoryStream>();
		pma::AlignedMemoryResource MemResOut;
		pma::ScopedPtr<dna::BinaryStreamWriter> DNAWriter = pma::makeScoped<dna::BinaryStreamWriter>(DNAOutStream.get(), &MemResOut);
		DNAWriter->setFrom(DNAReader.get());

		if (Impl->RefinementAPI.ScaleRig(DNAReader.get(), InScale, &Pivot[0], DNAWriter.get()))
		{
			DNAWriter->write();
			OutRawScaledDNABuffer.SetNumUninitialized((DNAOutStream->size() * sizeof(char)) / sizeof(uint8));
			DNAOutStream->read((char*)OutRawScaledDNABuffer.GetData(), DNAOutStream->size());
			return true;
		}
		else
		{
			return false;
		}
	}

	bool FMetaHumanConformer::TransformRigOrigin(const TArray<uint8>& InRawDNABuffer, const FMatrix44f& InTransformMatrix, TArray<uint8>& OutTransformedDNABuffer) const
	{
		pma::ScopedPtr<dna::MemoryStream> DNAStream = pma::makeScoped<dna::MemoryStream>();
		DNAStream->write((const char*)InRawDNABuffer.GetData(), (InRawDNABuffer.Num() * sizeof(uint8)) / sizeof(char));
		pma::ScopedPtr<dna::BinaryStreamReader> DNAReader = pma::makeScoped<dna::BinaryStreamReader>(DNAStream.get());
		DNAReader->read();

		pma::ScopedPtr<dna::MemoryStream> DNAOutStream = pma::makeScoped<dna::MemoryStream>();
		pma::ScopedPtr<dna::BinaryStreamWriter> DNAWriter = pma::makeScoped<dna::BinaryStreamWriter>(DNAOutStream.get());

		if (Impl->RefinementAPI.TransformRigOrigin(DNAReader.get(), &InTransformMatrix.M[0][0], DNAWriter.get()))
		{
			DNAWriter->write();
			OutTransformedDNABuffer.SetNumUninitialized((DNAOutStream->size() * sizeof(char)) / sizeof(uint8));
			DNAOutStream->read((char*)OutTransformedDNABuffer.GetData(), DNAOutStream->size());
			return true;
		}
		else
		{
			return false;
		}
	}

	bool FMetaHumanConformer::CheckControlsConfig(const FString& InControlsConfigJson) const
	{
		return Impl->RefinementAPI.CheckControlsConfig(TCHAR_TO_ANSI(*InControlsConfigJson));
	}

	bool FMetaHumanConformer::RefineTeethPlacement(const FString& InControlsConfigJson, const TArray<uint8>& InRawDNAPlusDeltaDNABuffer, const TArray<uint8>& InRawDNABuffer, TArray<uint8>& OutRawRefinedDNAPlusDeltaDNABuffer) const
	{
		const int32_t NumVertices = NumElementsFace / 3;
		std::vector<float> TeethRefinementMask(NumVertices);
		bool bIsOK = Impl->CreationAPI.GetFittingMask(TeethRefinementMask.data(), TITAN_API_NAMESPACE::FittingMaskType::TEETH_HEAD_COLLISION_INTERFACE);
		if (!bIsOK)
		{
			return false;
		}

		bIsOK = Impl->RefinementAPI.SetRefinementMask(NumVertices, TeethRefinementMask.data(), TITAN_API_NAMESPACE::RefinementMaskType::TEETH_PLACEMENT);
		if (!bIsOK)
		{
			return false;
		}

		pma::ScopedPtr<dna::MemoryStream> DNAPlusDeltaDNAStream = pma::makeScoped<dna::MemoryStream>();
		DNAPlusDeltaDNAStream->write((const char*)InRawDNAPlusDeltaDNABuffer.GetData(), (InRawDNAPlusDeltaDNABuffer.Num() * sizeof(uint8)) / sizeof(char));
		pma::ScopedPtr<dna::BinaryStreamReader> DNAPlusDeltaDNAReader = pma::makeScoped<dna::BinaryStreamReader>(DNAPlusDeltaDNAStream.get());
		DNAPlusDeltaDNAReader->read();

		pma::ScopedPtr<dna::MemoryStream> DNAStream = pma::makeScoped<dna::MemoryStream>();
		DNAStream->write((const char*)InRawDNABuffer.GetData(), (InRawDNABuffer.Num() * sizeof(uint8)) / sizeof(char));
		pma::ScopedPtr<dna::BinaryStreamReader> DNAReader = pma::makeScoped<dna::BinaryStreamReader>(DNAStream.get());
		DNAReader->read();

		pma::ScopedPtr<dna::MemoryStream> OutRawRefinedDNAPlusDeltaDNABufferStream = pma::makeScoped<trio::MemoryStream>();
		pma::AlignedMemoryResource MemResOut;
		pma::ScopedPtr<dna::BinaryStreamWriter> Writer = pma::makeScoped<dna::BinaryStreamWriter>(OutRawRefinedDNAPlusDeltaDNABufferStream.get(), &MemResOut);
		Writer->setFrom(DNAPlusDeltaDNAReader.get());

		if (Impl->RefinementAPI.RefineTeethPlacement(DNAPlusDeltaDNAReader.get(), DNAReader.get(), TCHAR_TO_ANSI(*InControlsConfigJson), Writer.get()))
		{
			Writer->write();
			OutRawRefinedDNAPlusDeltaDNABuffer.SetNumUninitialized((OutRawRefinedDNAPlusDeltaDNABufferStream->size() * sizeof(char)) / sizeof(uint8));
			OutRawRefinedDNAPlusDeltaDNABufferStream->read((char*)OutRawRefinedDNAPlusDeltaDNABuffer.GetData(), OutRawRefinedDNAPlusDeltaDNABufferStream->size());
			return true;
		}
		else
		{
			return false;
		}
	}


	TArray<uint8> FMetaHumanConformer::DNAToBuffer(UDNAAsset* InDNAAsset)
	{
		TArray<uint8> Buffer;

		dna::FReader Reader(InDNAAsset);

		pma::ScopedPtr<dna::MemoryStream> Stream = pma::makeScoped<trio::MemoryStream>();
		pma::ScopedPtr<dna::BinaryStreamWriter> Writer = pma::makeScoped<dna::BinaryStreamWriter>(Stream.get());

		// Need to call the base class setFrom() since it works the same as in pre 5.2 versions
		// The BinaryStreamWriter::setFrom() is a simple copy which does not work for our custom FReader
		Writer->dna::Writer::setFrom(&Reader);
		Writer->write();

		Buffer.SetNumUninitialized((Stream->size() * sizeof(char)) / sizeof(uint8));
		Stream->read((char*)Buffer.GetData(), Stream->size());

		return Buffer;
	}


}
}
