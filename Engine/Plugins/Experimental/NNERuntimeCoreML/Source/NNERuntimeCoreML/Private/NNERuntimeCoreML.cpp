// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef WITH_NNE_RUNTIME_COREML

#include "NNERuntimeCoreML.h"

#include "CoreMinimal.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCoreMLModel.h"
#include "NNERuntimeCoreMLNPUHelper.h"
#include "Serialization/MemoryWriter.h"

FGuid UNNERuntimeCoreML::GUID = FGuid((int32)'C', (int32)'O', (int32)'M', (int32)'L');
int32 UNNERuntimeCoreML::Version = 0x00000001;

namespace UE::NNERuntimeCoreML::Private::Details
{
	template <class CanCreateModelStatus> CanCreateModelStatus CanCreateModel(const TObjectPtr<UNNEModelData> ModelData, const FString& RuntimeName)
	{
		check(ModelData != nullptr);

		constexpr int32 GuidSize = sizeof(UNNERuntimeCoreML::GUID);
		constexpr int32 VersionSize = sizeof(UNNERuntimeCoreML::Version);
		const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(RuntimeName);

		if (!SharedData.IsValid())
		{
			return CanCreateModelStatus::Fail;
		}

		TConstArrayView64<uint8> Data = SharedData->GetView();

		if (Data.Num() <= GuidSize + VersionSize)
		{
			return CanCreateModelStatus::Fail;
		}

		bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeCoreML::GUID), GuidSize) == 0;
		bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeCoreML::Version), VersionSize) == 0;

		return bResult ? CanCreateModelStatus::Ok : CanCreateModelStatus::Fail;
	}
} // UE::NNERuntimeCoreML::Private::Details

UNNERuntimeCoreML::ECanCreateModelDataStatus UNNERuntimeCoreML::CanCreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return (!FileData.IsEmpty() && FileType.Compare("mlmodel", ESearchCase::IgnoreCase) == 0) ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeCoreML::CreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	using namespace UE::NNERuntimeCoreML::Private;

	if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
	{
		UE_LOG(LogNNERuntimeCoreML, Error, TEXT("Cannot create the model data with id %s (Filetype: %s)"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType);
		return {};
	}

	//Note: here the model should be optimized and the related mlmodelc binary blob saved instead of source model

	TArray64<uint8> Result;
	FMemoryWriter64 Writer(Result, /*bIsPersitent =*/ true);
	Writer << UNNERuntimeCoreML::GUID;
	Writer << UNNERuntimeCoreML::Version;

	Writer.Serialize(const_cast<uint8*>(FileData.GetData()), FileData.Num());

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
}

FString UNNERuntimeCoreML::GetModelDataIdentifier(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeCoreML::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeCoreML::Version);
}

FString UNNERuntimeCoreML::GetRuntimeName() const
{
	return TEXT("NNERuntimeCoreML");
}

UNNERuntimeCoreMLCpuGpu::ECanCreateModelCPUStatus UNNERuntimeCoreMLCpuGpu::CanCreateModelCPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	return UE::NNERuntimeCoreML::Private::Details::CanCreateModel<UNNERuntimeCoreMLCpuGpu::ECanCreateModelCPUStatus>(ModelData, GetRuntimeName());
}

UNNERuntimeCoreMLCpuGpu::ECanCreateModelGPUStatus UNNERuntimeCoreMLCpuGpu::CanCreateModelGPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	return UE::NNERuntimeCoreML::Private::Details::CanCreateModel<UNNERuntimeCoreMLCpuGpu::ECanCreateModelGPUStatus>(ModelData, GetRuntimeName());
}

UNNERuntimeCoreMLCpuGpuNpu::ECanCreateModelNPUStatus UNNERuntimeCoreMLCpuGpuNpu::CanCreateModelNPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	if (!UE::NNERuntimeCoreML::Private::IsNPUAvailable())
		return UNNERuntimeCoreMLCpuGpuNpu::ECanCreateModelNPUStatus::Fail;

	return UE::NNERuntimeCoreML::Private::Details::CanCreateModel<UNNERuntimeCoreMLCpuGpuNpu::ECanCreateModelNPUStatus>(ModelData, GetRuntimeName());
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeCoreMLCpuGpu::CreateModelCPU(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	if (CanCreateModelCPU(ModelData) != ECanCreateModelCPUStatus::Ok)
	{
		UE_LOG(LogNNERuntimeCoreML, Error, TEXT("Cannot create a CPU model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	const TSharedRef<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName()).ToSharedRef();

	return MakeShared<UE::NNERuntimeCoreML::Private::FModelCoreMLCpu>(SharedData);
}

TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeCoreMLCpuGpu::CreateModelGPU(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	if (CanCreateModelGPU(ModelData) != ECanCreateModelGPUStatus::Ok)
	{
		UE_LOG(LogNNERuntimeCoreML, Error, TEXT("Cannot create a GPU model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelGPU>();
	}

	const TSharedRef<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName()).ToSharedRef();

	return MakeShared<UE::NNERuntimeCoreML::Private::FModelCoreMLGpu>(SharedData);
}

TSharedPtr<UE::NNE::IModelNPU> UNNERuntimeCoreMLCpuGpuNpu::CreateModelNPU(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	if (CanCreateModelNPU(ModelData) != ECanCreateModelNPUStatus::Ok)
	{
		UE_LOG(LogNNERuntimeCoreML, Error, TEXT("Cannot create a NPU model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelNPU>();
	}

	const TSharedRef<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName()).ToSharedRef();

	return MakeShared<UE::NNERuntimeCoreML::Private::FModelCoreMLNpu>(SharedData);
}

#endif // WITH_NNE_RUNTIME_COREML
