// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORT.h"

#include "Misc/SecureHash.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeORTModel.h"
#include "NNERuntimeORTModelFormat.h"
#include "NNERuntimeORTUtils.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if PLATFORM_WINDOWS
#include "ID3D12DynamicRHI.h"
#endif // PLATFORM_WINDOWS

#include UE_INLINE_GENERATED_CPP_BY_NAME(NNERuntimeORT)

DEFINE_LOG_CATEGORY(LogNNERuntimeORT);

FGuid UNNERuntimeORTCpu::GUID = FGuid((int32)'O', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeORTCpu::Version = 0x00000004;

namespace UE::NNERuntimeORT::Private::Details
{ 
	//Should be kept in sync with OnnxFileLoaderHelper::InitUNNEModelDataFromFile()
	static FString OnnxExternalDataDescriptorKey(TEXT("OnnxExternalDataDescriptor"));
	static FString OnnxExternalDataBytesKey(TEXT("OnnxExternalDataBytes"));

	FOnnxDataDescriptor MakeOnnxDataDescriptor(TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData)
	{
		FOnnxDataDescriptor OnnxDataDescriptor = {};
		OnnxDataDescriptor.OnnxModelDataSize = FileData.Num();

		if (AdditionalFileData.Contains(OnnxExternalDataDescriptorKey))
		{
			TConstArrayView<uint8> OnnxExternalDataDescriptorBuffer = AdditionalFileData[OnnxExternalDataDescriptorKey];
			FMemoryReaderView OnnxExternalDataDescriptorReader(OnnxExternalDataDescriptorBuffer, /*bIsPersistent = */true);
			TMap<FString, int64> ExternalDataSizes;

			OnnxExternalDataDescriptorReader << ExternalDataSizes;

			int64 CurrentBucketOffset = OnnxDataDescriptor.OnnxModelDataSize;
			for (const auto& Element : ExternalDataSizes)
			{
				const FString DataFilePath = Element.Key;
				FOnnxAdditionalDataDescriptor DataDescriptor;
				DataDescriptor.Path = DataFilePath;
				DataDescriptor.Offset = CurrentBucketOffset;
				DataDescriptor.Size = Element.Value;

				OnnxDataDescriptor.AdditionalDataDescriptors.Emplace(DataDescriptor);
				CurrentBucketOffset += Element.Value;
			}
		}
		return OnnxDataDescriptor;
	}

	void WriteOnnxModelData(FMemoryWriter64 Writer, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData)
	{
		FOnnxDataDescriptor Descriptor = MakeOnnxDataDescriptor(FileData, AdditionalFileData);
		check(FileData.Num() == Descriptor.OnnxModelDataSize);
		Writer << Descriptor;

		Writer.Serialize(const_cast<uint8*>(FileData.GetData()), FileData.Num());

		if (!Descriptor.AdditionalDataDescriptors.IsEmpty())
		{
			

			check(AdditionalFileData.Contains(OnnxExternalDataBytesKey));
			Writer.Serialize(const_cast<uint8*>(AdditionalFileData[OnnxExternalDataBytesKey].GetData()), AdditionalFileData[OnnxExternalDataBytesKey].Num());
		}
	}

	bool IsAvailableGPU(bool bDirectMLAvailable, bool bD3D12Available)
	{
		return bDirectMLAvailable && bD3D12Available;
	}

	bool IsAvailableRDG(bool bDirectMLAvailable, bool bRHID3D12Available)
	{
		return bDirectMLAvailable && bRHID3D12Available;
	}

	bool IsAvailableNPU(bool bDirectMLAvailable, bool bD3D12DeviceNPUAvailable)
	{
		return bDirectMLAvailable && bD3D12DeviceNPUAvailable;
	}
} // namespace UE::NNERuntimeORT::Private::Details

UNNERuntimeORTCpu::ECanCreateModelDataStatus UNNERuntimeORTCpu::CanCreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return (!FileData.IsEmpty() && FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0) ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTCpu::CreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	using namespace UE::NNERuntimeORT::Private;

	if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
	{
		UE_LOG(LogNNERuntimeORT, Error, TEXT("Cannot create the CPU model data with id %s (Filetype: %s)"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType);
		return {};
	}

	TConstArrayView64<uint8> OptimizedModelView = FileData;
	TArray64<uint8> OptimizedModelBuffer;

	//For now only optimize model if there is no external data (as additional data are serialized from the unoptimized model below)
	if (AdditionalFileData.IsEmpty())
	{
		
		if (GraphOptimizationLevel OptimizationLevel = GetGraphOptimizationLevelForCPU(false, IsRunningCookCommandlet()); OptimizationLevel > GraphOptimizationLevel::ORT_DISABLE_ALL)
		{
			TUniquePtr<Ort::SessionOptions> SessionOptions = CreateSessionOptionsDefault(Environment.ToSharedRef());
			SessionOptions->SetGraphOptimizationLevel(OptimizationLevel);
			SessionOptions->EnableCpuMemArena();

			if (!OptimizeModel(Environment.ToSharedRef(), *SessionOptions, FileData, OptimizedModelBuffer))
			{
				UE_LOG(LogNNERuntimeORT, Error, TEXT("Failed to optimize model for CPU with id %s, model data will not be available"), *FileId.ToString(EGuidFormats::Digits).ToLower());
				return {};
			}

			OptimizedModelView = OptimizedModelBuffer;
		}
	}

	TArray64<uint8> Result;
	FMemoryWriter64 Writer(Result, /*bIsPersitent =*/ true);
	Writer << UNNERuntimeORTCpu::GUID;
	Writer << UNNERuntimeORTCpu::Version;

	Details::WriteOnnxModelData(Writer, OptimizedModelView, AdditionalFileData);

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
}

FString UNNERuntimeORTCpu::GetModelDataIdentifier(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeORTCpu::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeORTCpu::Version);
}

void UNNERuntimeORTCpu::Init(TSharedRef<UE::NNERuntimeORT::Private::FEnvironment> InEnvironment)
{
	Environment = InEnvironment;
}

FString UNNERuntimeORTCpu::GetRuntimeName() const
{
	return TEXT("NNERuntimeORTCpu");
}

UNNERuntimeORTCpu::ECanCreateModelCPUStatus UNNERuntimeORTCpu::CanCreateModelCPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	constexpr int32 GuidSize = sizeof(UNNERuntimeORTCpu::GUID);
	constexpr int32 VersionSize = sizeof(UNNERuntimeORTCpu::Version);
	const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

	if (!SharedData.IsValid())
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	TConstArrayView64<uint8> Data = SharedData->GetView();

	if (Data.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTCpu::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTCpu::Version), VersionSize) == 0;

	return bResult ? ECanCreateModelCPUStatus::Ok : ECanCreateModelCPUStatus::Fail;
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeORTCpu::CreateModelCPU(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	if (CanCreateModelCPU(ModelData) != ECanCreateModelCPUStatus::Ok)
	{
		UE_LOG(LogNNERuntimeORT, Error, TEXT("Cannot create a CPU model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	const TSharedRef<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName()).ToSharedRef();

	return MakeShared<UE::NNERuntimeORT::Private::FModelORTCpu>(Environment.ToSharedRef(), SharedData);
}

/*
 * FNNERuntimeORTDmlImpl
 */
class FNNERuntimeORTDmlImpl : public INNERuntimeORTDmlImpl
{

	using ECanCreateModelCommonStatus = UE::NNE::EResultStatus;

private:
	TSharedPtr<UE::NNERuntimeORT::Private::FEnvironment> Environment;

public:
	static FGuid GUID;
	static int32 Version;

	FNNERuntimeORTDmlImpl() = default;
	virtual ~FNNERuntimeORTDmlImpl() = default;

virtual void Init(TSharedRef<UE::NNERuntimeORT::Private::FEnvironment> InEnvironment, bool bInDirectMLAvailable) override
{
	using namespace UE::NNERuntimeORT::Private;

	Environment = InEnvironment;
	
	bIsAvailableGPU = Details::IsAvailableGPU(bInDirectMLAvailable, IsD3D12Available());
	bIsAvailableRDG = Details::IsAvailableRDG(bInDirectMLAvailable, IsRHID3D12Available());
	bIsAvailableNPU = Details::IsAvailableNPU(bInDirectMLAvailable, IsD3D12DeviceNPUAvailable());
}

virtual FString GetRuntimeName() const override
{
	return TEXT("NNERuntimeORTDml");
}

virtual ECanCreateModelDataStatus CanCreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override
{
	return (!FileData.IsEmpty() && FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0) ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
}

virtual TSharedPtr<UE::NNE::FSharedModelData> CreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) override
{
	using namespace UE::NNERuntimeORT::Private;

	if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
	{
		UE_LOG(LogNNERuntimeORT, Error, TEXT("Cannot create the Dml model data with id %s (Filetype: %s)"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType);
		return {};
	}

	TConstArrayView64<uint8> OptimizedModelView = FileData;
	TArray64<uint8> OptimizedModelBuffer;

	//For now only optimize model if there is no external data (as additional data are serialized from the unoptimized model below)
	if (AdditionalFileData.IsEmpty())
	{
		if (GraphOptimizationLevel OptimizationLevel = GetGraphOptimizationLevelForDML(false, IsRunningCookCommandlet()); OptimizationLevel > GraphOptimizationLevel::ORT_DISABLE_ALL)
		{
			TUniquePtr<Ort::SessionOptions> SessionOptions = CreateSessionOptionsDefault(Environment.ToSharedRef());
			SessionOptions->SetGraphOptimizationLevel(OptimizationLevel);
			SessionOptions->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
			SessionOptions->DisableMemPattern();

			if (!OptimizeModel(Environment.ToSharedRef(), *SessionOptions, FileData, OptimizedModelBuffer))
			{
				UE_LOG(LogNNERuntimeORT, Error, TEXT("Failed to optimize model for DirectML with id %s, model data will not be available"), *FileId.ToString(EGuidFormats::Digits).ToLower());

				return {};
			}

			OptimizedModelView = OptimizedModelBuffer;
		}
	}

	TArray64<uint8> Result;
	FMemoryWriter64 Writer(Result, /*bIsPersitent =*/ true);
	Writer << GUID;
	Writer << Version;

	Details::WriteOnnxModelData(Writer, OptimizedModelView, AdditionalFileData);

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
}

virtual FString GetModelDataIdentifier(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(Version);
}

virtual ECanCreateModelGPUStatus CanCreateModelGPU(const TObjectPtr<UNNEModelData> ModelData) const override
{
	if (!ensureMsgf(bIsAvailableGPU, TEXT("GPU interface should not be available!")))
	{
		return ECanCreateModelCommonStatus::Fail;
	}

	return CanCreateModelCommon(ModelData, false) == ECanCreateModelCommonStatus::Ok ? ECanCreateModelGPUStatus::Ok : ECanCreateModelGPUStatus::Fail;
}

virtual TSharedPtr<UE::NNE::IModelGPU> CreateModelGPU(const TObjectPtr<UNNEModelData> ModelData) override
{
#if PLATFORM_WINDOWS
	check(ModelData);

	if (CanCreateModelGPU(ModelData) != ECanCreateModelGPUStatus::Ok)
	{
		UE_LOG(LogNNERuntimeORT, Error, TEXT("Cannot create a GPU model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return {};
	}

	const TSharedRef<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName()).ToSharedRef();

	return MakeShared<UE::NNERuntimeORT::Private::FModelORTDmlGPU>(Environment.ToSharedRef(), SharedData);
#else // PLATFORM_WINDOWS
	return TSharedPtr<UE::NNE::IModelGPU>();
#endif // PLATFORM_WINDOWS
}

virtual ECanCreateModelRDGStatus CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const override
{
	if (!ensureMsgf(bIsAvailableRDG, TEXT("RDG interface should not be available!")))
	{
		return ECanCreateModelCommonStatus::Fail;
	}

	return CanCreateModelCommon(ModelData) == ECanCreateModelCommonStatus::Ok ? ECanCreateModelRDGStatus::Ok : ECanCreateModelRDGStatus::Fail;
}

virtual TSharedPtr<UE::NNE::IModelRDG> CreateModelRDG(TObjectPtr<UNNEModelData> ModelData) override
{
#if PLATFORM_WINDOWS
	check(ModelData);

	if (CanCreateModelRDG(ModelData) != ECanCreateModelRDGStatus::Ok)
	{
		UE_LOG(LogNNERuntimeORT, Error, TEXT("Cannot create a RDG model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return {};
	}

	const TSharedRef<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName()).ToSharedRef();

	return MakeShared<UE::NNERuntimeORT::Private::FModelORTDmlRDG>(Environment.ToSharedRef(), SharedData);
#else // PLATFORM_WINDOWS
	return {};
#endif // PLATFORM_WINDOWS
}

virtual ECanCreateModelNPUStatus CanCreateModelNPU(const TObjectPtr<UNNEModelData> ModelData) const override
{
	if (!ensureMsgf(bIsAvailableNPU, TEXT("NPU interface should not be available!")))
	{
		return ECanCreateModelCommonStatus::Fail;
	}

	return CanCreateModelCommon(ModelData) == ECanCreateModelCommonStatus::Ok ? ECanCreateModelRDGStatus::Ok : ECanCreateModelRDGStatus::Fail;
}

virtual TSharedPtr<UE::NNE::IModelNPU> CreateModelNPU(const TObjectPtr<UNNEModelData> ModelData) override
{
#if PLATFORM_WINDOWS
	check(ModelData);

	if (CanCreateModelNPU(ModelData) != ECanCreateModelRDGStatus::Ok)
	{
		UE_LOG(LogNNERuntimeORT, Error, TEXT("Cannot create a model NPU from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return {};
	}

	const TSharedRef<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName()).ToSharedRef();

	return MakeShared<UE::NNERuntimeORT::Private::FModelORTNpu>(Environment.ToSharedRef(), SharedData);
#else // PLATFORM_WINDOWS
	return {};
#endif // PLATFORM_WINDOWS
}

private:
ECanCreateModelCommonStatus CanCreateModelCommon(const TObjectPtr<UNNEModelData> ModelData, bool bRHID3D12Required = true) const
{
#if PLATFORM_WINDOWS
	check(ModelData != nullptr);

	constexpr int32 GuidSize = sizeof(GUID);
	constexpr int32 VersionSize = sizeof(Version);
	const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

	if (!SharedData.IsValid())
	{
		return ECanCreateModelCommonStatus::Fail;
	}

	TConstArrayView64<uint8> Data = SharedData->GetView();

	if (Data.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelCommonStatus::Fail;
	}

	static const FGuid DeprecatedGUID = FGuid((int32)'O', (int32)'G', (int32)'P', (int32)'U');

	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(GUID), GuidSize) == 0;
	bResult |= FGenericPlatformMemory::Memcmp(&(Data[0]), &(DeprecatedGUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(Version), VersionSize) == 0;

	return bResult ? ECanCreateModelCommonStatus::Ok : ECanCreateModelCommonStatus::Fail;
#else // PLATFORM_WINDOWS
	return ECanCreateModelCommonStatus::Fail;
#endif // PLATFORM_WINDOWS
}

	bool bIsAvailableGPU = false;
	bool bIsAvailableRDG = false;
	bool bIsAvailableNPU = false;
};

FGuid FNNERuntimeORTDmlImpl::GUID = FGuid((int32)'O', (int32)'D', (int32)'M', (int32)'L');
int32 FNNERuntimeORTDmlImpl::Version = 0x00000004;

UNNERuntimeORTDmlProxy::UNNERuntimeORTDmlProxy()
{
	Impl = MakeUnique<FNNERuntimeORTDmlImpl>();
}

void UNNERuntimeORTDmlProxy::Init(TSharedRef<UE::NNERuntimeORT::Private::FEnvironment> InEnvironment, bool bInDirectMLAvailable) { Impl->Init(InEnvironment, bInDirectMLAvailable); }

FString UNNERuntimeORTDmlProxy::GetRuntimeName() const { return Impl->GetRuntimeName(); }

UNNERuntimeORTDmlProxy::ECanCreateModelDataStatus UNNERuntimeORTDmlProxy::CanCreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const { return Impl->CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform); }
TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTDmlProxy::CreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) { return Impl->CreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform); }
FString UNNERuntimeORTDmlProxy::GetModelDataIdentifier(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const { return Impl->GetModelDataIdentifier(FileType, FileData, AdditionalFileData, FileId, TargetPlatform); }

namespace UE::NNERuntimeORT::Private
{

TWeakObjectPtr<UNNERuntimeORTDmlProxy> MakeRuntimeDml(bool bDirectMLAvailable)
{
	const bool bRHID3D12Available = IsRHID3D12Available();
	const bool bD3D12Available = IsD3D12Available();
	const bool bD3D12DeviceNPUAvailable = IsD3D12DeviceNPUAvailable();

	const bool bIsAvailableGPU = Details::IsAvailableGPU(bDirectMLAvailable, bD3D12Available);
	const bool bIsAvailableRDG = Details::IsAvailableRDG(bDirectMLAvailable, bRHID3D12Available);
	const bool bIsAvailableNPU = Details::IsAvailableNPU(bDirectMLAvailable, bD3D12DeviceNPUAvailable);

	UE_LOG(LogNNERuntimeORT, Log, TEXT("MakeRuntimeORTDml:"));
	UE_LOG(LogNNERuntimeORT, Log, TEXT("  DirectML:  %s"), (bDirectMLAvailable ? TEXT("yes") : TEXT("no")));
	UE_LOG(LogNNERuntimeORT, Log, TEXT("  RHI D3D12: %s"), (bRHID3D12Available ? TEXT("yes") : TEXT("no")));
	UE_LOG(LogNNERuntimeORT, Log, TEXT("  D3D12:     %s"), (bD3D12Available ? TEXT("yes") : TEXT("no")));
	UE_LOG(LogNNERuntimeORT, Log, TEXT("  NPU:       %s"), (bD3D12DeviceNPUAvailable ? TEXT("yes") : TEXT("no")));

	UE_LOG(LogNNERuntimeORT, Log, TEXT("Interface availability:"));
	UE_LOG(LogNNERuntimeORT, Log, TEXT("  GPU: %s"), (bIsAvailableGPU ? TEXT("yes") : TEXT("no")));
	UE_LOG(LogNNERuntimeORT, Log, TEXT("  RDG: %s"), (bIsAvailableRDG ? TEXT("yes") : TEXT("no")));
	UE_LOG(LogNNERuntimeORT, Log, TEXT("  NPU: %s"), (bIsAvailableNPU ? TEXT("yes") : TEXT("no")));

	if (bIsAvailableGPU && bIsAvailableRDG && bIsAvailableNPU) return NewObject<UNNERuntimeORTDml_GPU_RDG_NPU>();
	else if (bIsAvailableGPU && bIsAvailableRDG) return NewObject<UNNERuntimeORTDml_GPU_RDG>();
	else if (bIsAvailableGPU && bIsAvailableNPU) return NewObject<UNNERuntimeORTDml_GPU_NPU>();
	else if (bIsAvailableRDG && bIsAvailableNPU) return NewObject<UNNERuntimeORTDml_RDG_NPU>();
	else if (bIsAvailableGPU) return NewObject<UNNERuntimeORTDml_GPU>();
	else if (bIsAvailableRDG) return NewObject<UNNERuntimeORTDml_RDG>();
	else if (bIsAvailableNPU) return NewObject<UNNERuntimeORTDml_NPU>();

#if WITH_EDITOR
	UE_LOG(LogNNERuntimeORT, Log, TEXT("NNERuntimeORTDml can only cook!"));
	return NewObject<UNNERuntimeORTDmlProxy>();
#else
	UE_LOG(LogNNERuntimeORT, Log, TEXT("NNERuntimeORTDml is not available!"));
	return {};
#endif
}

} // namespace UE::NNERuntimeORT::Private