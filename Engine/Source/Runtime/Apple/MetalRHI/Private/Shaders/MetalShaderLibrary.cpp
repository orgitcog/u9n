// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaderLibrary.cpp: Metal RHI Shader Library Class Implementation.
=============================================================================*/

#include "MetalShaderLibrary.h"
#include "MetalRHIPrivate.h"
#if !UE_BUILD_SHIPPING
#include "Debugging/MetalShaderDebugCache.h"
#include "Debugging/MetalShaderDebugZipFile.h"
#endif // !UE_BUILD_SHIPPING
#include "MetalShaderTypes.h"

static bool GPersistentMappingMetallib = false;
static FAutoConsoleVariableRef CVarPersistentMappingMetallib(
	TEXT("r.Metal.PersistentMappingMetallib"),
	GPersistentMappingMetallib,
	TEXT("Makes the metallib file mapping persistent."),
	ECVF_Default
);

static void ReleaseLibraryMemory(FMetalShaderLibrary::FShaderLibDataOwner* LibraryMemOwner)
{
	if (!GPersistentMappingMetallib)
	{
		delete LibraryMemOwner;
	}
}

//------------------------------------------------------------------------------

#pragma mark - Metal Shader Library Class Support Routines


template<typename ShaderType>
static TRefCountPtr<FRHIShader> CreateMetalShader(FMetalDevice& Device, TArrayView<const uint8> InCode, MTLLibraryPtr InLibrary)
{
	ShaderType* Shader = new ShaderType(Device, InCode, InLibrary);
	if (!Shader->GetFunction())
	{
		delete Shader;
		Shader = nullptr;
	}

	return TRefCountPtr<FRHIShader>(Shader);
}


//------------------------------------------------------------------------------

#pragma mark - Metal Shader Library Class Public Static Members


FCriticalSection FMetalShaderLibrary::LoadedShaderLibraryMutex;
TMap<FString, FRHIShaderLibrary*> FMetalShaderLibrary::LoadedShaderLibraryMap;


//------------------------------------------------------------------------------

#pragma mark - Metal Shader Library Class


FMetalShaderLibrary::FMetalShaderLibrary(FMetalDevice& MetalDevice,
										 EShaderPlatform Platform,
										 FString const& Name,
										 const FString& InShaderLibraryFilename,
										 const FMetalShaderLibraryHeader& InHeader,
										 FSerializedShaderArchive&& InSerializedShaders,
										 FShaderCodeArrayType&& InShaderCode,
										 TArray<TUniquePtr<FLazyMetalLib>>&& InLazyLibraries)
	: FRHIShaderLibrary(Platform, Name)
	, Device(MetalDevice)
	, ShaderLibraryFilename(InShaderLibraryFilename)
	, Header(InHeader)
	, SerializedShaders(MoveTemp(InSerializedShaders))
	, ShaderCode(MoveTemp(InShaderCode))
	, LazyLibraries(MoveTemp(InLazyLibraries))
{
#if !UE_BUILD_SHIPPING
	DebugFile = nullptr;

	FName PlatformName = LegacyShaderPlatformToShaderFormat(Platform);
	FString LibName = FString::Printf(TEXT("%s_%s"), *Name, *PlatformName.GetPlainNameString());
	LibName.ToLowerInline();
	FString Path = FPaths::ProjectContentDir() / LibName + TEXT(".zip");

	if (IFileManager::Get().FileExists(*Path))
	{
		DebugFile = FMetalShaderDebugCache::Get().GetDebugFile(Path);
	}
#endif // !UE_BUILD_SHIPPING
}

FMetalShaderLibrary::~FMetalShaderLibrary()
{
	FScopeLock Lock(&LoadedShaderLibraryMutex);
	LoadedShaderLibraryMap.Remove(ShaderLibraryFilename);
}

bool FMetalShaderLibrary::IsNativeLibrary() const
{
	return true;
}

int32 FMetalShaderLibrary::GetNumShaders() const
{
	return SerializedShaders.GetShaderEntries().Num();
}

int32 FMetalShaderLibrary::GetNumShaderMaps() const
{
	return SerializedShaders.GetShaderMapEntries().Num();
}

uint32 FMetalShaderLibrary::GetSizeBytes() const
{
#if USE_MMAPPED_SHADERARCHIVE
	return SerializedShaders.GetAllocatedSize() + ShaderCode.Num() * ShaderCode.GetTypeSize();
#else
	return SerializedShaders.GetAllocatedSize() + ShaderCode.GetAllocatedSize();
#endif
}

int32 FMetalShaderLibrary::GetNumShadersForShaderMap(int32 ShaderMapIndex) const
{
	return SerializedShaders.GetShaderMapEntries()[ShaderMapIndex].NumShaders;
}

int32 FMetalShaderLibrary::GetShaderIndex(int32 ShaderMapIndex, int32 i) const
{
	const FShaderMapEntry& ShaderMapEntry = SerializedShaders.GetShaderMapEntries()[ShaderMapIndex];
	return SerializedShaders.GetShaderIndices()[ShaderMapEntry.ShaderIndicesOffset + i];
}

int32 FMetalShaderLibrary::FindShaderMapIndex(const FSHAHash& Hash)
{
	return SerializedShaders.FindShaderMap(Hash);
}

int32 FMetalShaderLibrary::FindShaderIndex(const FSHAHash& Hash)
{
	return SerializedShaders.FindShader(Hash);
}

TRefCountPtr<FRHIShader> FMetalShaderLibrary::CreateShader(int32 Index, bool bRequired)
{
	const FShaderCodeEntry& ShaderEntry = SerializedShaders.GetShaderEntries()[Index];

	// We don't handle compressed shaders here, since typically these are just tiny headers.
	check(ShaderEntry.Size == ShaderEntry.UncompressedSize);

	const TArrayView<const uint8> Code = MakeArrayView(ShaderCode.GetData() + ShaderEntry.Offset, ShaderEntry.Size);
	const int32 LibraryIndex = Index / Header.NumShadersPerLibrary;

	{
		TUniquePtr<FLazyMetalLib>& LazyLibrary = LazyLibraries[LibraryIndex];
		UE::TScopeLock Lock(LazyLibrary->LibraryLock);
		
		if (LazyLibrary->Library.get() == nullptr)
		{
			check(LazyLibrary->Data.Get() != nullptr);
			
			NS::Error* Error;
			if (LazyLibrary->Data->MappedRegion.IsValid())
			{
				UE_LOG(LogMetal, Display, TEXT("mmapping %s, %d bytes"), *LazyLibrary->MetalLibraryFilePath, LazyLibrary->Data->MappedCacheFile->GetFileSize());
				FMetalShaderLibrary::FShaderLibDataOwner* MyData = LazyLibrary->Data.Release();
				dispatch_data_t data = dispatch_data_create(MyData->MappedRegion->GetMappedPtr(), MyData->MappedRegion->GetMappedSize(), nil, ^ {
					ReleaseLibraryMemory(MyData);
				});
				LazyLibrary->Library = NS::TransferPtr(Device.GetDevice()->newLibrary(data, &Error));
				dispatch_release(data);
			}
			else
			{
				TArray<uint8>& FileData = LazyLibrary->Data->Mem;
				if (FFileHelper::LoadFileToArray(FileData, *LazyLibrary->MetalLibraryFilePath))
				{
					UE_LOG(LogMetal, Display, TEXT("emulating mmapping %s, %d bytes!"), *LazyLibrary->MetalLibraryFilePath, FileData.Num());
					FMetalShaderLibrary::FShaderLibDataOwner* MyData = LazyLibrary->Data.Release();
					dispatch_data_t data = dispatch_data_create(FileData.GetData(), FileData.Num(), nil, ^ {
						ReleaseLibraryMemory(MyData);
					});
					LazyLibrary->Library = NS::TransferPtr(Device.GetDevice()->newLibrary(data, &Error));
					dispatch_release(data);
				}
			}
			
			if (LazyLibrary->Library.get() == nullptr)
			{
				UE_LOG(LogMetal, Error, TEXT("Metal library creationg error: %s"), *NSStringToFString(Error->description()));
				UE_LOG(LogMetal, Fatal, TEXT("Failed to create library: %s"), *LazyLibrary->MetalLibraryFilePath);
			}
		}
	}
	
	check(LazyLibraries[LibraryIndex]->Library.get() != nullptr);
	
	TRefCountPtr<FRHIShader> Shader;
	switch (ShaderEntry.Frequency)
	{
		case SF_Vertex:
			Shader = CreateMetalShader<FMetalVertexShader>(Device, Code, LazyLibraries[LibraryIndex]->Library);
			break;

		case SF_Pixel:
			Shader = CreateMetalShader<FMetalPixelShader>(Device, Code, LazyLibraries[LibraryIndex]->Library);
			break;
 
 		case SF_Geometry:
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
            Shader = CreateMetalShader<FMetalGeometryShader>(Device, Code, LazyLibraries[LibraryIndex]->Library);
#else
            checkf(false, TEXT("Geometry shaders not supported"));
#endif
            break;


        case SF_Mesh:
#if PLATFORM_SUPPORTS_MESH_SHADERS
            Shader = CreateMetalShader<FMetalMeshShader>(Device, Code, LazyLibraries[LibraryIndex]->Library);
#else
			checkf(false, TEXT("Mesh shaders not supported"));
#endif
            break;

        case SF_Amplification:
#if PLATFORM_SUPPORTS_MESH_SHADERS
            Shader = CreateMetalShader<FMetalAmplificationShader>(Device, Code, LazyLibraries[LibraryIndex]->Library);
#else
			checkf(false, TEXT("Amplification shaders not supported"));
#endif
            break;

		case SF_Compute:
			Shader = CreateMetalShader<FMetalComputeShader>(Device, Code, LazyLibraries[LibraryIndex]->Library);
			break;

		default:
			checkNoEntry();
			break;
	}

	if (Shader)
	{
		Shader->SetHash(SerializedShaders.GetShaderHashes()[Index]);
	}

	return Shader;
}
