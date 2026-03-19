// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREE.h"

#ifdef WITH_NNE_RUNTIME_IREE

#if WITH_EDITOR
#include "Containers/StringConv.h"
#include "HAL/PlatformFileManager.h"
#include "Memory/SharedBuffer.h"
#include "Misc/FileHelper.h"
#endif // WITH_EDITOR

#include "HAL/Platform.h"
#include "Interfaces/ITargetPlatform.h"
#include "IREECompilerRDG.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeIREECompiler.h"
#include "NNERuntimeIREELog.h"
#include "NNERuntimeIREEMetaData.h"
#include "NNERuntimeIREEModel.h"
#include "NNERuntimeIREEModelData.h"
#include "RHIGlobals.h"
#include "RHIStrings.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace UE::NNERuntimeIREE::CPU::Private
{
	FString GetTargetPlatformName(const ITargetPlatform* TargetPlatform)
	{
		return TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	}

	FString GetBinariesSubdirectory(const FString& PlatformName)
	{
		if (PlatformName.Equals("Windows"))
		{
			if (PLATFORM_64BITS)
			{
				return TEXT("Win64");
			}
			return TEXT("Win32");
		}
		else
		{
			return PlatformName;
		}
	}

	FString GetModelDataIdentifier(const FString& RuntimeName, const FGuid& Guid, int32 Version, const FString& FileIdString, const FString& PlatformName, const FString& Architecture)
	{
		return RuntimeName + "-" + Guid.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(Version) + "-" + FileIdString + "-" + PlatformName + (!Architecture.IsEmpty() ? ("-" + Architecture) : "");
	}

	FString GuidToString(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::Digits).ToLower();
	}

	FString GetRuntimeIdAndVersionString(const FGuid& RuntimeGuid, int32 Version)
	{
		return GuidToString(RuntimeGuid) + "-" + FString::FromInt(Version);
	}

	FString GetModelDataDirectoryName(const FString& RuntimeIdAndVersionString, const FString& FileIdString)
	{
		return RuntimeIdAndVersionString + "_" + FileIdString;
	}

	FString GetIntermediateModelDirPath(const FString& PlatformName, const FString& RuntimeName, const FString& ModelName)
	{
		return FPaths::Combine("Intermediate", "Build", GetBinariesSubdirectory(PlatformName), RuntimeName, ModelName);
	}

	FString GetStagedModelDirPath(const FString& PlatformName, const FString& RuntimeName, const FString& ModelName)
	{
		return FPaths::Combine("Binaries", GetBinariesSubdirectory(PlatformName), RuntimeName, ModelName);
	}

	FString GetPackagedModelDirPath(const FString& PlatformName, const FString& RuntimeName, const FString& ModelName)
	{
		return GetStagedModelDirPath(PlatformName, RuntimeName, ModelName);
	}

	FString GetSharedLibDirPath(const FString& PlatformName, const FString& RuntimeName, const FString& ModelName)
	{
#if WITH_EDITOR
	return GetIntermediateModelDirPath(PlatformName, RuntimeName, ModelName);
#else
	return GetPackagedModelDirPath(PlatformName, RuntimeName, ModelName);
#endif // WITH_EDITOR
	}

	FString GetRuntimeSubdir(bool bIsCooking = false)
	{
#if WITH_EDITOR
		if (bIsCooking)
		{
			return TEXT("Cooked");
		}
		else
		{
			return TEXT("Editor");
		}
#else
		check(!bIsCooking);
		return {};
#endif
	}
} // UE::NNERuntimeIREE::CPU::Private

FGuid UNNERuntimeIREECpu::GUID = FGuid((int32)'I', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeIREECpu::Version = 0x00000005;

FString UNNERuntimeIREECpu::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREECpu");
}

UNNERuntimeIREECpu::ECanCreateModelDataStatus UNNERuntimeIREECpu::CanCreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	// Check model is not > 2GB
	if ((TArray<uint8>::SizeType)FileData.Num() != FileData.Num())
	{
		return ECanCreateModelDataStatus::Fail;
	}


	return (FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0 || FileType.Compare(TEXT("onnx"), ESearchCase::IgnoreCase) == 0) ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
#else
	return ECanCreateModelDataStatus::Fail;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREECpu::CreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	SCOPED_NAMED_EVENT_TEXT("UNNERuntimeIREECpu::CreateModelData", FColor::Magenta);

#if WITH_EDITOR
	using namespace UE::NNERuntimeIREE::CPU::Private;

	const FString TargetPlatformName = GetTargetPlatformName(TargetPlatform);
	if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu cannot create the model data with id %s (Filetype: %s) for platform %s"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType, *TargetPlatformName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const bool bIsCooking = TargetPlatform != nullptr;

	const FString FileIdString = FileId.ToString(EGuidFormats::Digits).ToLower();
	const FString IntermediateDirFullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetIntermediateModelDirPath(TargetPlatformName, GetRuntimeName(), FileIdString)));
	const FString SharedLibraryDirFullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetSharedLibDirPath(TargetPlatformName, GetRuntimeName(), FileIdString)));

	FString IREEModelDataFilePath = FPaths::Combine(IntermediateDirFullPath, FileIdString) + ".ireemodeldata";

	TArray64<uint8> ResultData;
	TWeakObjectPtr<UNNERuntimeIREEModelData> IREEModelData = NewObject<UNNERuntimeIREEModelData>();
	FNNERuntimeIREECompilerResultCPU CompilerResult{};

	bool bNeedCompileMlir = true;
	if (PlatformFile.FileExists(*IREEModelDataFilePath))
	{
		SCOPED_NAMED_EVENT_TEXT("Validate", FColor::Magenta);

		FFileHelper::LoadFileToArray(ResultData, *IREEModelDataFilePath);
		
		{
			FMemoryReaderView Reader(ResultData, /*bIsPersitent =*/ true);
			IREEModelData->Serialize(Reader);
		}

		check(FileIdString.Equals(IREEModelData->FileId.ToString(EGuidFormats::Digits).ToLower()));

		{
			FMemoryReaderView Reader(IREEModelData->CompilerResult, /*bIsPersitent =*/ true);
			FNNERuntimeIREECompilerResultCPU::StaticStruct()->SerializeBin(Reader, &CompilerResult);
		}

		bNeedCompileMlir = false;
		for (int32 i = 0; i < CompilerResult.ArchitectureInfos.Num(); i++)
		{
			const FNNERuntimeIREEArchitectureInfoCPU& Info = CompilerResult.ArchitectureInfos[i];
			const FString SharedLibrarySubDirFullPath = FPaths::Combine(SharedLibraryDirFullPath, Info.RelativeDirPath);

			const FString SharedLibraryFilePath = FPaths::Combine(SharedLibrarySubDirFullPath, Info.SharedLibraryFileName);
			const FString VmfbFilePath = FPaths::Combine(SharedLibrarySubDirFullPath, Info.VmfbFileName);

			bNeedCompileMlir |= !PlatformFile.FileExists(*SharedLibraryFilePath);
			bNeedCompileMlir |= !PlatformFile.FileExists(*VmfbFilePath);
		}
	}
	
	if (bNeedCompileMlir || bIsCooking)
	{
		SCOPED_NAMED_EVENT_TEXT("Compile", FColor::Magenta);

		PlatformFile.DeleteDirectoryRecursively(*IntermediateDirFullPath);
		PlatformFile.CreateDirectoryTree(*IntermediateDirFullPath);

		TUniquePtr<UE::NNERuntimeIREE::CPU::FCompiler> Compiler = UE::NNERuntimeIREE::CPU::FCompiler::Make(TargetPlatformName);
		if (!Compiler.IsValid())
		{
			UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu failed to create a compiler to compile for platform %s"), *TargetPlatformName);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}

		TArray64<uint8> ImporterOutputData;
		if(FileType.Compare(TEXT("onnx"), ESearchCase::IgnoreCase) == 0)
		{
			if(!Compiler->ImportOnnx(FileData, FileIdString, IntermediateDirFullPath, ImporterOutputData))
			{
				UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu failed to import ONNX model %s"), *FileIdString);
				return TSharedPtr<UE::NNE::FSharedModelData>();
			}
			FileData = ImporterOutputData;
		}

		// NOTE: From this point FileData refers to mlir data in any case
		
		TWeakObjectPtr<UNNERuntimeIREEModuleMetaData> CompilerModuleMetaData = NewObject<UNNERuntimeIREEModuleMetaData>();

		if (!Compiler->CompileMlir(FileData, FileIdString, IntermediateDirFullPath, CompilerResult, *CompilerModuleMetaData))
		{
			UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu failed to compile model %s"), *FileIdString);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}

		IREEModelData->GUID = UNNERuntimeIREECpu::GUID;
		IREEModelData->Version = UNNERuntimeIREECpu::Version;
		IREEModelData->FileId = FileId;
		if (AdditionalFileData.Contains("IREEModuleMetaData"))
		{
			IREEModelData->ModuleMetaData = AdditionalFileData["IREEModuleMetaData"];
		}
		if (IREEModelData->ModuleMetaData.IsEmpty())
		{
			FMemoryWriter64 Writer(IREEModelData->ModuleMetaData, /*bIsPersitent =*/ true);
			CompilerModuleMetaData->Serialize(Writer);
		}
		{
			FMemoryWriter64 Writer(IREEModelData->CompilerResult, /*bIsPersitent =*/ true);
			FNNERuntimeIREECompilerResultCPU::StaticStruct()->SerializeBin(Writer, &CompilerResult);
		}

		{
			FMemoryWriter64 Writer(ResultData, /*bIsPersitent =*/ true);
			IREEModelData->Serialize(Writer);
		}

		FFileHelper::SaveArrayToFile(ResultData, *IREEModelDataFilePath);
	}

	// Only stage when cooking
	if (bIsCooking)
	{
		// Copy files for staging
		FString StagingDirFullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetPackagedModelDirPath(TargetPlatformName, GetRuntimeName(), FileIdString)));
		for (int32 i = 0; i < CompilerResult.ArchitectureInfos.Num(); i++)
		{
			SCOPED_NAMED_EVENT_TEXT("Copy", FColor::Magenta);

			const FNNERuntimeIREEArchitectureInfoCPU& Info = CompilerResult.ArchitectureInfos[i];
			const FString SharedLibrarySubDirFullPath = FPaths::Combine(SharedLibraryDirFullPath, Info.RelativeDirPath);
			const FString StagingSubDirFullPath = FPaths::Combine(StagingDirFullPath, Info.Architecture);
			
			const FString SharedLibraryFilePathSrc = FPaths::Combine(SharedLibrarySubDirFullPath, Info.SharedLibraryFileName);
			const FString VmfbFilePathSrc = FPaths::Combine(SharedLibrarySubDirFullPath, Info.VmfbFileName);

			const FString SharedLibraryFilePathDest = FPaths::Combine(StagingSubDirFullPath, Info.SharedLibraryFileName);
			const FString VmfbFilePathDest = FPaths::Combine(StagingSubDirFullPath, Info.VmfbFileName);

			IFileManager::Get().Copy(*SharedLibraryFilePathDest, *SharedLibraryFilePathSrc);
			IFileManager::Get().Copy(*VmfbFilePathDest, *VmfbFilePathSrc);
		}
	}

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(ResultData)), 0);
#else
	return TSharedPtr<UE::NNE::FSharedModelData>();
#endif // WITH_EDITOR
}

FString UNNERuntimeIREECpu::GetModelDataIdentifier(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	// Leave architecture blank as there is only one model data for all architectures of a given platform, only the vmfb and shared lib are different
	FString PlatformName = UE::NNERuntimeIREE::CPU::Private::GetTargetPlatformName(TargetPlatform);
	return UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), UNNERuntimeIREECpu::GUID, UNNERuntimeIREECpu::Version, FileId.ToString(EGuidFormats::Digits), PlatformName, "");
}

UNNERuntimeIREECpu::ECanCreateModelCPUStatus UNNERuntimeIREECpu::CanCreateModelCPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	if (!UNNERuntimeIREEModelData::IsSameGuidAndVersion(SharedData->GetView(), UNNERuntimeIREECpu::GUID, UNNERuntimeIREECpu::Version))
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	return ECanCreateModelCPUStatus::Ok;
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeIREECpu::CreateModelCPU(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	using namespace UE::NNERuntimeIREE::CPU::Private;

	if (CanCreateModelCPU(ModelData) != ECanCreateModelCPUStatus::Ok)
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	FString CurrentArchitecture = "";
#if PLATFORM_CPU_X86_FAMILY
	CurrentArchitecture = "x86_64";
#elif PLATFORM_CPU_ARM_FAMILY
	CurrentArchitecture = "arm64";
#endif

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	check(SharedData.IsValid());

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();

	TWeakObjectPtr<UNNERuntimeIREEModelData> IREEModelData = NewObject<UNNERuntimeIREEModelData>();
	{
		FMemoryReaderView Reader(SharedDataView, /*bIsPersitent =*/ true);
		IREEModelData->Serialize(Reader);
	}

	if (IREEModelData->ModuleMetaData.IsEmpty())
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu failed to find any module meta data, please reimport the original model"));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	TWeakObjectPtr<UNNERuntimeIREEModuleMetaData> ModuleMetaData = NewObject<UNNERuntimeIREEModuleMetaData>();
	{
		FMemoryReaderView Reader(IREEModelData->ModuleMetaData, /*bIsPersitent =*/ true);
		ModuleMetaData->Serialize(Reader);
	}

	if (ModuleMetaData->FunctionMetaData.IsEmpty())
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu failed to parse the module meta data, please reimport the original model"));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	FNNERuntimeIREECompilerResultCPU CompilerResult{};
	{
		FMemoryReaderView Reader(IREEModelData->CompilerResult, /*bIsPersitent =*/ true);
		FNNERuntimeIREECompilerResultCPU::StaticStruct()->SerializeBin(Reader, &CompilerResult);
	}

	int32 ArchitectureIndex = -1;
	for (int32 i = 0; i < CompilerResult.ArchitectureInfos.Num(); i++)
	{
		if ((CompilerResult.ArchitectureInfos[i].Architecture.IsEmpty() && ArchitectureIndex < 0) || CompilerResult.ArchitectureInfos[i].Architecture.Equals(CurrentArchitecture))
		{
			ArchitectureIndex = i;
		}
	}

	if (ArchitectureIndex < 0)
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu failed to find a matching architecture for \'%s\'"), *CurrentArchitecture);
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	const FNNERuntimeIREEArchitectureInfoCPU& ArchitectureInfo = CompilerResult.ArchitectureInfos[ArchitectureIndex];

	const FString FileIdString = IREEModelData->FileId.ToString(EGuidFormats::Digits).ToLower();
	const FString SharedLibraryDirFullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetSharedLibDirPath(UGameplayStatics::GetPlatformName(), GetRuntimeName(), FileIdString)));
	const FString SharedLibrarySubDirFullPath = FPaths::Combine(SharedLibraryDirFullPath, ArchitectureInfo.RelativeDirPath);

	TSharedPtr<UE::NNE::IModelCPU> Model = UE::NNERuntimeIREE::CPU::FModel::Make(SharedLibrarySubDirFullPath, ArchitectureInfo.SharedLibraryFileName, ArchitectureInfo.VmfbFileName, ArchitectureInfo.SharedLibraryEntryPointName, *ModuleMetaData);
	if (!Model.IsValid())
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu could not initialize the model created from model data with id %s"), *FileIdString);
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	return Model;
}

FString UNNERuntimeIREEGpu::GetRuntimeName() const
{
	return TEXT("");
}

UNNERuntimeIREEGpu::ECanCreateModelDataStatus UNNERuntimeIREEGpu::CanCreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	// Check model is not > 2GB
	if ((TArray<uint8>::SizeType)FileData.Num() != FileData.Num())
	{
		return ECanCreateModelDataStatus::Fail;
	}

	return FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0 ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
#else
	return ECanCreateModelDataStatus::Fail;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREEGpu::CreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	return TSharedPtr<UE::NNE::FSharedModelData>();
}

FString UNNERuntimeIREEGpu::GetModelDataIdentifier(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	FString PlatformName = UE::NNERuntimeIREE::CPU::Private::GetTargetPlatformName(TargetPlatform);
	return UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), GetGUID(), UNNERuntimeIREEGpu::GetVersion(), FileId.ToString(EGuidFormats::Digits), PlatformName, "");
}

UNNERuntimeIREEGpu::ECanCreateModelGPUStatus UNNERuntimeIREEGpu::CanCreateModelGPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return ECanCreateModelGPUStatus::Fail;
	}

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	FGuid Guid = GetGUID();
	int32 Version = GetVersion();
	int32 GuidSize = sizeof(Guid);
	int32 VersionSize = sizeof(Version);
	if (SharedDataView.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelGPUStatus::Fail;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(SharedDataView[0]), &(Guid), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(SharedDataView[GuidSize]), &(Version), VersionSize) == 0;

	return bResult ? ECanCreateModelGPUStatus::Ok : ECanCreateModelGPUStatus::Fail;
}

TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeIREEGpu::CreateModelGPU(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	if (CanCreateModelGPU(ModelData) != ECanCreateModelGPUStatus::Ok)
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREEGpu cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelGPU>();
	}

	check(ModelData->GetModelData(GetRuntimeName()).IsValid());

	UE::NNE::IModelGPU* IModel = nullptr;
	TConstArrayView<uint8> SharedDataView = ModelData->GetModelData(GetRuntimeName())->GetView();

	return TSharedPtr<UE::NNE::IModelGPU>(IModel);
}

bool UNNERuntimeIREEGpu::IsAvailable() const
{
	return false;
}

FGuid UNNERuntimeIREEGpu::GetGUID() const
{
	return FGuid();
}

int32 UNNERuntimeIREEGpu::GetVersion() const
{
	return 0;
}

FGuid UNNERuntimeIREECuda::GUID = FGuid((int32)'I', (int32)'G', (int32)'C', (int32)'U');
int32 UNNERuntimeIREECuda::Version = 0x00000001;

FString UNNERuntimeIREECuda::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREECuda");
}

bool UNNERuntimeIREECuda::IsAvailable() const
{
	return false;
}

FGuid UNNERuntimeIREECuda::GetGUID() const
{
	return GUID;
}

int32 UNNERuntimeIREECuda::GetVersion() const
{
	return Version;
}

FGuid UNNERuntimeIREEVulkan::GUID = FGuid((int32)'I', (int32)'G', (int32)'V', (int32)'U');
int32 UNNERuntimeIREEVulkan::Version = 0x00000001;

FString UNNERuntimeIREEVulkan::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREEVulkan");
}

bool UNNERuntimeIREEVulkan::IsAvailable() const
{
	return false;
}

FGuid UNNERuntimeIREEVulkan::GetGUID() const
{
	return GUID;
}

int32 UNNERuntimeIREEVulkan::GetVersion() const
{
	return Version;
}

#ifdef WITH_NNE_RUNTIME_IREE_RDG

FGuid UNNERuntimeIREERdg::GUID = FGuid((int32)'I', (int32)'R', (int32)'D', (int32)'G');
int32 UNNERuntimeIREERdg::Version = 0x00000001;

FString UNNERuntimeIREERdg::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREERdg");
}

UNNERuntimeIREERdg::ECanCreateModelDataStatus UNNERuntimeIREERdg::CanCreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	// Check model is not > 2GB
	if ((TArray<uint8>::SizeType)FileData.Num() != FileData.Num())
	{
		return ECanCreateModelDataStatus::Fail;
	}

	return	(FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0 || FileType.Compare(TEXT("onnx"), ESearchCase::IgnoreCase) == 0) ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
#else
	return ECanCreateModelDataStatus::Fail;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREERdg::CreateModelData(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	SCOPED_NAMED_EVENT_TEXT("UNNERuntimeIREERdg::CreateModelData", FColor::Magenta);

#if WITH_EDITOR
	using namespace UE::NNERuntimeIREE::CPU::Private;

	const FString TargetPlatformName = GetTargetPlatformName(TargetPlatform);
	if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREERdg cannot create the model data with id %s (Filetype: %s) for platform %s"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType, *TargetPlatformName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const bool bIsCooking = TargetPlatform != nullptr;
	const FString RuntimeSubdir = GetRuntimeSubdir(bIsCooking);

	const FString FileIdString = FileId.ToString(EGuidFormats::Digits).ToLower();
	const FString IntermediateDirFullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetIntermediateModelDirPath(TargetPlatformName, GetRuntimeName(), FileIdString), RuntimeSubdir));
	const FString SharedLibraryDirFullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetSharedLibDirPath(TargetPlatformName, GetRuntimeName(), FileIdString), RuntimeSubdir));

	FString IREEModelDataFilePath = FPaths::Combine(IntermediateDirFullPath, FileIdString) + ".ireemodeldata";

	TArray64<uint8> ResultData;
	TWeakObjectPtr<UNNERuntimeIREEModelData> IREEModelData = NewObject<UNNERuntimeIREEModelData>();
	FIREECompilerRDGResult CompilerResult{};

	TArray<EShaderPlatform> ShaderPlatforms;
	if (TargetPlatform)
	{
		TArray<FName> DesiredShaderFormats;
		TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

		for (const FName& ShaderFormatName : DesiredShaderFormats)
		{
			ShaderPlatforms.Add(ShaderFormatToLegacyShaderPlatform(ShaderFormatName));
		}
	}
	else
	{
		const ERHIFeatureLevel::Type CacheFeatureLevel = GMaxRHIFeatureLevel;
		const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[CacheFeatureLevel];

		ShaderPlatforms.Add(ShaderPlatform);
	}

	bool bNeedCompileMlir = true;
	if (PlatformFile.FileExists(*IREEModelDataFilePath))
	{
		SCOPED_NAMED_EVENT_TEXT("Validate", FColor::Magenta);

		FFileHelper::LoadFileToArray(ResultData, *IREEModelDataFilePath);
		
		{
			FMemoryReaderView Reader(ResultData, /*bIsPersitent =*/ true);
			IREEModelData->Serialize(Reader);
		}

		check(FileIdString.Equals(IREEModelData->FileId.ToString(EGuidFormats::Digits).ToLower()));

		{
			FMemoryReaderView Reader(IREEModelData->CompilerResult, /*bIsPersitent =*/ true);
			FIREECompilerRDGResult::StaticStruct()->SerializeBin(Reader, &CompilerResult);
		}

		bNeedCompileMlir = false;
		for (EShaderPlatform ShaderPlatform : ShaderPlatforms)
		 {
			const FString ShaderPlatformName = LexToString(ShaderPlatform);

			const FIREECompilerRDGBuildTargetResult* Info = CompilerResult.BuildTargetResult.FindByPredicate([ShaderPlatformName](const FIREECompilerRDGBuildTargetResult &Element) { return Element.ShaderPlatform == ShaderPlatformName; });
			if (!Info)
			{
				bNeedCompileMlir = true;
				continue;
			}

			const FString SharedLibrarySubDirFullPath = FPaths::Combine(SharedLibraryDirFullPath, Info->RelativeDirPath);

			const FString VmfbFilePath = FPaths::Combine(SharedLibrarySubDirFullPath, Info->VmfbFileName);

			bNeedCompileMlir |= !PlatformFile.FileExists(*VmfbFilePath);

			for (const FString& ShaderMapFilename : Info->SharedLibraryFileNames)
			{
				const FString SharedLibraryFilePath = FPaths::Combine(SharedLibrarySubDirFullPath, ShaderMapFilename);

				bNeedCompileMlir |= !PlatformFile.FileExists(*SharedLibraryFilePath);
			}
		}
	}
	
	if (bNeedCompileMlir || bIsCooking)
	{
		SCOPED_NAMED_EVENT_TEXT("Compile", FColor::Magenta);

		PlatformFile.DeleteDirectoryRecursively(*IntermediateDirFullPath);
		PlatformFile.CreateDirectoryTree(*IntermediateDirFullPath);

		CompilerResult = FIREECompilerRDGResult();

		TUniquePtr<UE::IREE::Compiler::RDG::FCompiler> Compiler = UE::IREE::Compiler::RDG::FCompiler::Make(TargetPlatform);
		if (!Compiler.IsValid())
		{
			UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREERdg failed to create a compiler to compile for platform %s"), *TargetPlatformName);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}

		TArray64<uint8> ImporterOutputData;
		if(FileType.Compare(TEXT("onnx"), ESearchCase::IgnoreCase) == 0)
		{
			if(!Compiler->ImportOnnx(FileData, FileIdString, IntermediateDirFullPath, ImporterOutputData))
			{
				UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREECpu failed to import ONNX model %s"), *FileIdString);
				return TSharedPtr<UE::NNE::FSharedModelData>();
			}
			FileData = ImporterOutputData;
		}

		// NOTE: From this point FileData refers to mlir data in any case

		if (!Compiler->CompileMlir(FileData, FileIdString, IntermediateDirFullPath, ShaderPlatforms, CompilerResult))
		{
			UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREERdg failed to compile model %s"), *FileIdString);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}

		IREEModelData->GUID = UNNERuntimeIREERdg::GUID;
		IREEModelData->Version = UNNERuntimeIREERdg::Version;
		IREEModelData->FileId = FileId;
		if (AdditionalFileData.Contains("IREEModuleMetaData"))
		{
			IREEModelData->ModuleMetaData = AdditionalFileData["IREEModuleMetaData"];
		}
		if (IREEModelData->ModuleMetaData.IsEmpty())
		{
			TWeakObjectPtr<UNNERuntimeIREEModuleMetaData> CompilerModuleMetaData = NewObject<UNNERuntimeIREEModuleMetaData>();

			FString FileDataString = "";
			FileDataString.AppendChars((char*)FileData.GetData(), FileData.Num());
			CompilerModuleMetaData->ParseFromString(FileDataString);

			FMemoryWriter64 Writer(IREEModelData->ModuleMetaData, /*bIsPersitent =*/ true);
			CompilerModuleMetaData->Serialize(Writer);
		}
		{
			FMemoryWriter64 Writer(IREEModelData->CompilerResult, /*bIsPersitent =*/ true);
			FIREECompilerRDGResult::StaticStruct()->SerializeBin(Writer, &CompilerResult);
		}

		{
			FMemoryWriter64 Writer(ResultData, /*bIsPersitent =*/ true);
			IREEModelData->Serialize(Writer);
		}

		FFileHelper::SaveArrayToFile(ResultData, *IREEModelDataFilePath);
	}

	// Only stage when cooking
	if (bIsCooking)
	{
		// Copy files for staging
		FString StagingDirFullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetPackagedModelDirPath(TargetPlatformName, GetRuntimeName(), FileIdString)));
		for (int32 i = 0; i < CompilerResult.BuildTargetResult.Num(); i++)
		{
			SCOPED_NAMED_EVENT_TEXT("Copy", FColor::Magenta);

			const FIREECompilerRDGBuildTargetResult& Info = CompilerResult.BuildTargetResult[i];
			const FString SharedLibrarySubDirFullPath = FPaths::Combine(SharedLibraryDirFullPath, Info.RelativeDirPath);
			const FString StagingSubDirFullPath = FPaths::Combine(StagingDirFullPath, Info.RelativeDirPath /* Info.Architecture */);
			
			const FString VmfbFilePathSrc = FPaths::Combine(SharedLibrarySubDirFullPath, Info.VmfbFileName);
			const FString VmfbFilePathDest = FPaths::Combine(StagingSubDirFullPath, Info.VmfbFileName);

			IFileManager::Get().Copy(*VmfbFilePathDest, *VmfbFilePathSrc);

			for (const FString& ShaderMapFilename : Info.SharedLibraryFileNames)
			{
				const FString SharedLibraryFilePathSrc = FPaths::Combine(SharedLibrarySubDirFullPath, ShaderMapFilename);
				const FString SharedLibraryFilePathDest = FPaths::Combine(StagingSubDirFullPath, ShaderMapFilename);

				IFileManager::Get().Copy(*SharedLibraryFilePathDest, *SharedLibraryFilePathSrc);
			}
		}
	}

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(ResultData)), 0);
#else
	return TSharedPtr<UE::NNE::FSharedModelData>();
#endif // WITH_EDITOR
}

FString UNNERuntimeIREERdg::GetModelDataIdentifier(const FString& FileType, TConstArrayView64<uint8> FileData, const TMap<FString, TConstArrayView64<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	FString PlatformName = UE::NNERuntimeIREE::CPU::Private::GetTargetPlatformName(TargetPlatform);
	return UE::NNERuntimeIREE::CPU::Private::GetModelDataIdentifier(GetRuntimeName(), UNNERuntimeIREERdg::GUID, UNNERuntimeIREERdg::Version, FileId.ToString(EGuidFormats::Digits), PlatformName, "");
}

UNNERuntimeIREERdg::ECanCreateModelRDGStatus UNNERuntimeIREERdg::CanCreateModelRDG(const TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return ECanCreateModelRDGStatus::Fail;
	}

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	int32 GuidSize = sizeof(UNNERuntimeIREERdg::GUID);
	int32 VersionSize = sizeof(UNNERuntimeIREERdg::Version);
	if (SharedDataView.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelRDGStatus::Fail;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(SharedDataView[0]), &(UNNERuntimeIREERdg::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(SharedDataView[GuidSize]), &(UNNERuntimeIREERdg::Version), VersionSize) == 0;

	return bResult ? ECanCreateModelRDGStatus::Ok : ECanCreateModelRDGStatus::Fail;
}

TSharedPtr<UE::NNE::IModelRDG> UNNERuntimeIREERdg::CreateModelRDG(const TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	using namespace UE::NNERuntimeIREE::CPU::Private;

	if (CanCreateModelRDG(ModelData) != ECanCreateModelRDGStatus::Ok)
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREERdg cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelRDG>();
	}

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	check(SharedData.IsValid());

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();

	TWeakObjectPtr<UNNERuntimeIREEModelData> IREEModelData = NewObject<UNNERuntimeIREEModelData>();
	{
		FMemoryReaderView Reader(SharedDataView, /*bIsPersitent =*/ true);
		IREEModelData->Serialize(Reader);
	}

	if (IREEModelData->ModuleMetaData.IsEmpty())
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREERdg failed to find any module meta data, please reimport the original model"));
		return {};
	}

	TWeakObjectPtr<UNNERuntimeIREEModuleMetaData> ModuleMetaData = NewObject<UNNERuntimeIREEModuleMetaData>();
	{
		FMemoryReaderView Reader(IREEModelData->ModuleMetaData, /*bIsPersitent =*/ true);
		ModuleMetaData->Serialize(Reader);
	}

	if (ModuleMetaData->FunctionMetaData.IsEmpty())
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREERdg failed to parse the module meta data, please reimport the original model"));
		return {};
	}

	FIREECompilerRDGResult CompilerResult{};
	{
		FMemoryReaderView Reader(IREEModelData->CompilerResult, /*bIsPersitent =*/ true);
		FIREECompilerRDGResult::StaticStruct()->SerializeBin(Reader, &CompilerResult);
	}
	
	const ERHIFeatureLevel::Type CacheFeatureLevel = GMaxRHIFeatureLevel;
	const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[CacheFeatureLevel];
	const FString ShaderPlatformName = LexToString(ShaderPlatform);

	const FIREECompilerRDGBuildTargetResult* BuildTargetResult = CompilerResult.BuildTargetResult.FindByPredicate([ShaderPlatformName](const FIREECompilerRDGBuildTargetResult &Element) { return Element.ShaderPlatform == ShaderPlatformName; });
	if (!BuildTargetResult)
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREERdg failed to find a matching shader platform for \'%s\'"), *ShaderPlatformName);
		return {};
	}

	const FString FileIdString = IREEModelData->FileId.ToString(EGuidFormats::Digits).ToLower();
	const FString RuntimeSubdir = GetRuntimeSubdir();
	const FString SharedLibraryDirFullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetSharedLibDirPath(UGameplayStatics::GetPlatformName(), GetRuntimeName(), FileIdString), RuntimeSubdir));
	const FString SharedLibrarySubDirFullPath = FPaths::Combine(SharedLibraryDirFullPath, BuildTargetResult->RelativeDirPath);

	TSharedPtr<UE::NNE::IModelRDG> Model = UE::NNERuntimeIREE::RDG::FModel::Make(SharedLibrarySubDirFullPath, BuildTargetResult->VmfbFileName, *ModuleMetaData);
	if (!Model.IsValid())
	{
		UE_LOG(LogNNERuntimeIREE, Warning, TEXT("UNNERuntimeIREERdg could not initialize the model created from model data with id %s"), *FileIdString);
		return {};
	}

	return Model;
}

bool UNNERuntimeIREERdg::IsAvailable() const
{
#if !WITH_EDITOR
	if(GMaxRHIFeatureLevel != ERHIFeatureLevel::SM6)
	{
		UE_LOG(LogNNERuntimeIREE, Log, TEXT("Minimum feature level required is SM6 for current RHI platform."));
		return false;
	}

	if(!GRHIGlobals.SupportsNative16BitOps)
	{
		UE_LOG(LogNNERuntimeIREE, Log, TEXT("Current RHI platform doesn't support native 16-bit operations."));
		return false;
	}
#endif

	return true;
}

#endif // WITH_NNE_RUNTIME_IREE_RDG

#endif // WITH_NNE_RUNTIME_IREE