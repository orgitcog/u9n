// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealMutableImageProvider.h"

#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuCO/UnrealConversionUtils.h"
#include "MuCO/LoadUtils.h"
#include "MuCO/MutableMeshBufferUtils.h"
#include "MuR/Parameters.h"
#include "MuR/ImageTypes.h"
#include "MuR/Model.h"
#include "MuR/Mesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/BoneReference.h"
#include "Animation/Skeleton.h"
#include "ProfilingDebugging/IoStoreTrace.h"
#include "ReferenceSkeleton.h"
#include "ImageCoreUtils.h"
#include "TextureResource.h"


namespace
{
	void ConvertTextureUnrealPlatformToMutable(mu::FImage* OutResult, UTexture2D* Texture, uint8 MipmapsToSkip)
	{		
		check(Texture->GetPlatformData()->Mips[MipmapsToSkip].BulkData.IsBulkDataLoaded());

		int32 LODs = 1;
		int32 SizeX = Texture->GetSizeX() >> MipmapsToSkip;
		int32 SizeY = Texture->GetSizeY() >> MipmapsToSkip;
		check(SizeX > 0 && SizeY > 0);

		EPixelFormat Format = Texture->GetPlatformData()->PixelFormat;
		mu::EImageFormat MutableFormat = mu::EImageFormat::None;

		switch (Format)
		{
		case EPixelFormat::PF_B8G8R8A8: MutableFormat = mu::EImageFormat::BGRA_UByte; break;
			// This format is deprecated and using the enum fails to compile in some cases.
			//case ETextureSourceFormat::TSF_RGBA8: MutableFormat = mu::EImageFormat::RGBA_UByte; break;
		case EPixelFormat::PF_G8: MutableFormat = mu::EImageFormat::L_UByte; break;
		default:
			break;
		}

		// If not locked ReadOnly the Texture Source's FGuid can change, invalidating the texture's caching/shaders
		// making shader compile and cook times increase
		const void* pSource = Texture->GetPlatformData()->Mips[MipmapsToSkip].BulkData.LockReadOnly();

		if (pSource)
		{
			OutResult->Init(SizeX, SizeY, LODs, MutableFormat, mu::EInitializationType::NotInitialized);
			FMemory::Memcpy(OutResult->GetLODData(0), pSource, OutResult->GetLODDataSize(0));
			Texture->GetPlatformData()->Mips[MipmapsToSkip].BulkData.Unlock();
		}
		else
		{
			check(false);
			OutResult->Init(SizeX, SizeY, LODs, MutableFormat, mu::EInitializationType::Black);
		}
	}
}


mu::EImageFormat GetMutablePixelFormat(EPixelFormat InTextureFormat)
{
	switch (InTextureFormat)
	{
	case PF_B8G8R8A8: return mu::EImageFormat::BGRA_UByte;
	case PF_R8G8B8A8: return mu::EImageFormat::RGBA_UByte;
	case PF_DXT1: return mu::EImageFormat::BC1;
	case PF_DXT3: return mu::EImageFormat::BC2;
	case PF_DXT5: return mu::EImageFormat::BC3;
	case PF_BC4: return mu::EImageFormat::BC4;
	case PF_BC5: return mu::EImageFormat::BC5;
	case PF_G8: return mu::EImageFormat::L_UByte;
	case PF_ASTC_4x4: return mu::EImageFormat::ASTC_4x4_RGBA_LDR;
	case PF_ASTC_6x6: return mu::EImageFormat::ASTC_6x6_RGBA_LDR;
	case PF_ASTC_8x8: return mu::EImageFormat::ASTC_8x8_RGBA_LDR;
	case PF_ASTC_10x10: return mu::EImageFormat::ASTC_10x10_RGBA_LDR;
	case PF_ASTC_12x12: return mu::EImageFormat::ASTC_12x12_RGBA_LDR;
	default: return mu::EImageFormat::None;
	}
}


//-------------------------------------------------------------------------------------------------
TTuple<UE::Tasks::FTask, TFunction<void()>> FUnrealMutableResourceProvider::GetImageAsync(FName Id, uint8 MipmapsToSkip, TFunction<void(TSharedPtr<mu::FImage>)>& ResultCallback)
{
	// Thread: worker
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetImageAsync);

	// Some data that may have to be copied from the GlobalExternalImages while it's locked
	IBulkDataIORequest* IORequest = nullptr;
	const int32 LODs = 1;

	EPixelFormat Format = EPixelFormat::PF_Unknown;
	int32 BulkDataSize = 0;

	mu::EImageFormat MutImageFormat = mu::EImageFormat::None;
	int32 MutImageDataSize = 0;

	auto TrivialReturn = []() -> TTuple<UE::Tasks::FTask, TFunction<void()>>
	{
		return MakeTuple(UE::Tasks::MakeCompletedTask<void>(), []() -> void {});
	};

	{
		FScopeLock Lock(&ExternalImagesLock);
		// Inside this scope it's safe to access GlobalExternalImages


		if (!GlobalExternalImages.Contains(Id))
		{
			// Null case, no image was provided
			UE_LOG(LogMutable, Warning, TEXT("Failed to get external image [%s]. GlobalExternalImage not found."), *Id.ToString());

			ResultCallback(CreateDummy());
			return Invoke(TrivialReturn);
		}

		const FUnrealMutableImageInfo& ImageInfo = GlobalExternalImages[Id];

		if (ImageInfo.Image)
		{
			// Easy case where the image was directly provided
			ResultCallback(ImageInfo.Image);
			return Invoke(TrivialReturn);
		}

#if WITH_EDITOR
		if (const TSharedPtr<FMutableSourceTextureData>& SourceTextureData = ImageInfo.SourceTextureData)
		{
			const int32 MipIndex = FMath::Min(static_cast<int32>(MipmapsToSkip), SourceTextureData->GetSource().GetNumMips() - 1);
			check(MipIndex >= 0);
			
			// In the editor the src data can be directly accessed
			TSharedPtr<mu::FImage> Image = MakeShared<mu::FImage>();

			EUnrealToMutableConversionError Error = ConvertTextureUnrealSourceToMutable(Image.Get(), *SourceTextureData, MipIndex);
			if (Error != EUnrealToMutableConversionError::Success)
			{
				// This could happen in the editor, because some source textures may have changed while there was a background compilation.
				// We just show a warning and move on. This cannot happen during cooks, so it is fine.
				UE_LOG(LogMutable, Warning, TEXT("Failed to load some source texture data for image [%s]. Some materials may look corrupted."), *Id.ToString());
			}

			ResultCallback(Image);
			return Invoke(TrivialReturn);
		}
#else
		if (UTexture2D* TextureToLoad = ImageInfo.TextureToLoad)
		{
			// It's safe to access TextureToLoad because ExternalImagesLock guarantees that the data in GlobalExternalImages is valid,
			// not being modified by the game thread at the moment and the texture cannot be GCed because of the AddReferencedObjects
			// in the FUnrealMutableImageProvider

			int32 MipIndex = MipmapsToSkip < TextureToLoad->GetPlatformData()->Mips.Num() ? MipmapsToSkip : TextureToLoad->GetPlatformData()->Mips.Num() - 1;
			check (MipIndex >= 0);

			// Mips in the mip tail are inlined and can't be streamed, find the smallest mip available.
			for (; MipIndex > 0; --MipIndex)
			{
				if (TextureToLoad->GetPlatformData()->Mips[MipIndex].BulkData.CanLoadFromDisk())
				{
					break;
				}
			}
			
			// Texture format and the equivalent mutable format
			Format = TextureToLoad->GetPlatformData()->PixelFormat;
			MutImageFormat = GetMutablePixelFormat(Format);

			// Check if it's a format we support
			if (MutImageFormat == mu::EImageFormat::None)
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to get external image [%s]. Unexpected image format. EImageFormat [%s]."), *Id.ToString(), GetPixelFormatString(Format));
				ResultCallback(CreateDummy());
				return Invoke(TrivialReturn);
			}

			int32 SizeX = TextureToLoad->GetSizeX() >> MipIndex;
			int32 SizeY = TextureToLoad->GetSizeY() >> MipIndex;

			check(LODs == 1);
			TSharedPtr<mu::FImage> Image = MakeShared<mu::FImage>(SizeX, SizeY, LODs, MutImageFormat, mu::EInitializationType::NotInitialized);
			TArrayView<uint8> MutImageDataView = Image->DataStorage.GetLOD(0);

			// In a packaged game the bulk data has to be loaded
			// Get the actual file to read the mip 0 data, do not keep any reference to TextureToLoad because once outside of the lock
			// it may be GCed or changed. Just keep the actual file handle and some sizes instead of the texture
			FByteBulkData& BulkData = TextureToLoad->GetPlatformData()->Mips[MipIndex].BulkData;
			BulkDataSize = BulkData.GetBulkDataSize();
			check(BulkDataSize > 0);

			if (BulkDataSize != MutImageDataView.Num())
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to get external image [%s]. Bulk data size is different than the expected size. BulkData size [%d]. Mutable image data size [%d]."),
					*Id.ToString(),	BulkDataSize, MutImageDataSize);

				ResultCallback(CreateDummy());
				return Invoke(TrivialReturn);
			}

			// Create a streaming request if the data is not loaded or copy the mip data
			if (!BulkData.IsBulkDataLoaded())
			{
				UE::Tasks::FTaskEvent IORequestCompletionEvent(TEXT("Mutable_IORequestCompletionEvent"));

				TFunction<void(bool, IBulkDataIORequest*)> IOCallback =
					[
						MutImageDataView,
						MutImageFormat,
						Format,
						Image,
						BulkDataSize,	
						ResultCallback, // Notice ResultCallback is captured by copy
						IORequestCompletionEvent
					](bool bWasCancelled, IBulkDataIORequest* IORequest)
				{
					ON_SCOPE_EXIT
					{
						UE::Tasks::FTaskEvent EventCopy = IORequestCompletionEvent;
						EventCopy.Trigger();
					};
					
					// Should we do someting different than returning a dummy image if cancelled?
					if (bWasCancelled)
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Cancelled IO Request"));
						ResultCallback(CreateDummy());
						return;
					}

					uint8* Results = IORequest->GetReadResults(); // required?

					if (Results && MutImageDataView.Num() == (int32)IORequest->GetSize())
					{
						check(BulkDataSize == (int32)IORequest->GetSize());
						check(Results == MutImageDataView.GetData());

						ResultCallback(Image);
						return;
					}

					if (!Results)
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. IO Request failed. Request results [%hhd]. Format: [%s]. MutableFormat: [%d]."),
							(Results != nullptr),
							GetPixelFormatString(Format),
							(int32)MutImageFormat);
					}
					else if (MutImageDataView.Num() != (int32)IORequest->GetSize())
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Requested size is different than the expected size. RequestSize: [%lld]. ExpectedSize: [%d]. Format: [%s]. MutableFormat: [%d]."),
							IORequest->GetSize(),
							MutImageDataView.Num(),
							GetPixelFormatString(Format),
							(int32)MutImageFormat);
					}
					else
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image."));
					}

					// Something failed when loading the bulk data, just return a dummy
					ResultCallback(CreateDummy());
				};
			
				// Is the resposability of the CreateStreamingRequest caller to delete the IORequest. 
				// This can *not* be done in the IOCallback because it would cause a deadlock so it is deferred to the returned
				// cleanup function. Another solution could be to spwan a new task that depends on the 
				// IORequestComplitionEvent which deletes it.
				TRACE_IOSTORE_METADATA_SCOPE_TAG(Id);
				IORequest = BulkData.CreateStreamingRequest(EAsyncIOPriorityAndFlags::AIOP_High, &IOCallback, MutImageDataView.GetData());

				if (IORequest)
				{
					// Make the lambda mutable and set the IORequest pointer to null when deleted so it is safer 
					// agains multiple calls.
					const auto DeleteIORequest = [IORequest]() mutable -> void
					{
						if (IORequest)
						{
							delete IORequest;
						}
						
						IORequest = nullptr;
					};

					return MakeTuple(IORequestCompletionEvent, DeleteIORequest);

				}
				else
				{
					UE_LOG(LogMutable, Warning, TEXT("Failed to create an IORequest for a UTexture2D BulkData for an application-specific image parameter."));

					IORequestCompletionEvent.Trigger();
					
					ResultCallback(CreateDummy());
					return Invoke(TrivialReturn);
				}
			}
			else
			{
				// Bulk data already loaded
				const void* Data = (!BulkData.IsLocked()) ? BulkData.LockReadOnly() : nullptr; // TODO: Retry if it fails?
				
				if (Data)
				{
					FMemory::Memcpy(MutImageDataView.GetData(), Data, BulkDataSize);

					BulkData.Unlock();
					ResultCallback(Image);
					return Invoke(TrivialReturn);
				}
				else
				{
					UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Bulk data already locked or null."));
					ResultCallback(CreateDummy());
					return Invoke(TrivialReturn);
				}
			}
		}
#endif
		
		// No UTexture2D was provided, cannot do anything, just provide a dummy texture
		UE_LOG(LogMutable, Warning, TEXT("No UTexture2D was provided for an application-specific image parameter."));
		ResultCallback(CreateDummy());
		return Invoke(TrivialReturn);
	}

	// Make sure the returned event is dispatched at some point for all code paths, 
	// in this case returning Invoke(TrivialReturn) or through the IORequest callback.
}


//-------------------------------------------------------------------------------------------------
TTuple<UE::Tasks::FTask, TFunction<void()>> FUnrealMutableResourceProvider::GetReferencedImageAsync(const void* ModelPtr, int32 Id, uint8 MipmapsToSkip, TFunction<void(TSharedPtr<mu::FImage>)>& ResultCallback)
{
	// Thread: worker
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetReferencedImageAsync);

	auto TrivialReturn = []() -> TTuple<UE::Tasks::FTask, TFunction<void()>>
	{
		return MakeTuple(UE::Tasks::MakeCompletedTask<void>(), []() -> void {});
	};

#if WITH_EDITOR

	TSharedPtr<mu::FImage> Image = MakeShared<mu::FImage>();

	FScopeLock Lock(&RuntimeReferencedLock);
	if (!RuntimeReferencedImages.Contains(ModelPtr))
	{
		UE_LOG(LogMutable, Error, TEXT("Failed to load image [%i]. Model not registered in the provider."), Id);
		ResultCallback(CreateDummy());
		return Invoke(TrivialReturn);
	}

	TArray<FMutableSourceTextureData>& SourceTextures = RuntimeReferencedImages[ModelPtr].SourceTextures;
	if (!SourceTextures.IsValidIndex(Id))
	{
		// This could happen in the editor, because some source textures may have changed while there was a background compilation.
		// We just show a warning and move on. This cannot happen during cooks, so it is fine.
		UE_LOG(LogMutable, Warning, TEXT("Failed to load image [%i]."), Id);

		ResultCallback(CreateDummy());
		return Invoke(TrivialReturn);
	}

	FMutableSourceTextureData& SourceTextureData = SourceTextures[Id];
	
	const int32 MipIndex = FMath::Min(static_cast<int32>(MipmapsToSkip), SourceTextureData.GetSource().GetNumMips() - 1);
	check(MipIndex >= 0);
	
	EUnrealToMutableConversionError Error = ConvertTextureUnrealSourceToMutable(Image.Get(), SourceTextureData, MipIndex);
	if (Error != EUnrealToMutableConversionError::Success)
	{
		// This could happen in the editor, because some source textures may have changed while updating.
		// We just show a warning and move on. This cannot happen during cooks, so it is fine.
		UE_LOG(LogMutable, Warning, TEXT("Failed to load some source texture data for image [%i]. Some textures may be corrupted."), Id);
		
		ResultCallback(CreateDummy());
		return Invoke(TrivialReturn);
	}

	ResultCallback(Image);
	return Invoke(TrivialReturn);
#else // WITH_EDITOR

	// Not supported outside editor yet.
	UE_LOG(LogMutable, Warning, TEXT("Failed to get reference image. Only supported in editor."));

	ResultCallback(CreateDummy());
	return Invoke(TrivialReturn);

#endif
}


// This should mantain parity with the descriptor of the images generated by GetImageAsync 
mu::FExtendedImageDesc FUnrealMutableResourceProvider::GetImageDesc(FName Id)
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetImageDesc);

	FScopeLock Lock(&ExternalImagesLock);
	// Inside this scope it's safe to access GlobalExternalImages

	if (!GlobalExternalImages.Contains(Id))
	{
		// Null case, no image was provided
		return CreateDummyDesc();
	}

	const FUnrealMutableImageInfo& ImageInfo = GlobalExternalImages[Id];

	if (ImageInfo.Image)
	{
		// Easy case where the image was directly provided
		
		const mu::FImageDesc ImageDesc = mu::FImageDesc 
				{ ImageInfo.Image->GetSize(), ImageInfo.Image->GetFormat(), (uint8)ImageInfo.Image->GetLODCount() };
		return mu::FExtendedImageDesc { ImageDesc, 0 }; 
	}

#if WITH_EDITOR
	if (const TSharedPtr<FMutableSourceTextureData>& SourceTextureData = ImageInfo.SourceTextureData)
	{
		const FTextureSource& Source = SourceTextureData->GetSource();

		const mu::FImageSize ImageSize = mu::FImageSize(Source.GetSizeX(), Source.GetSizeY());
		const uint8 LODs = 1;

		return mu::FExtendedImageDesc { mu::FImageDesc { ImageSize, mu::EImageFormat::None, LODs }, 0 };
	}
#else
	if (UTexture2D* TextureToLoad = ImageInfo.TextureToLoad)
	{
		mu::FExtendedImageDesc Result;	

		// It's safe to access TextureToLoad because ExternalImagesLock guarantees that the data in GlobalExternalImages is valid,
		// not being modified by the game thread at the moment and the texture cannot be GCed because of the AddReferencedObjects
		// in the FUnrealMutableImageProvider

		const int32 TextureToLoadNumMips = TextureToLoad->GetPlatformData()->Mips.Num();

		int32 FirstLODAvailable = 0;
		for (; FirstLODAvailable < TextureToLoadNumMips; ++FirstLODAvailable)
		{
			if (TextureToLoad->GetPlatformData()->Mips[FirstLODAvailable].BulkData.DoesExist())
			{
				break;
			}
		}
	
		// Texture format and the equivalent mutable format
		const EPixelFormat Format = TextureToLoad->GetPlatformData()->PixelFormat;
		const mu::EImageFormat MutableFormat = GetMutablePixelFormat(Format);

		// Check if it's a format we support
		if (MutableFormat == mu::EImageFormat::None)
		{
			UE_LOG(LogMutable, Warning, TEXT("Failed to get external image descriptor. Unexpected image format. EImageFormat [%s]."), GetPixelFormatString(Format));
			return CreateDummyDesc();
		}

		const mu::FImageDesc ImageDesc = mu::FImageDesc 
			{ mu::FImageSize(TextureToLoad->GetSizeX(), TextureToLoad->GetSizeY()), MutableFormat, 1 }; 

		Result = mu::FExtendedImageDesc { ImageDesc, (uint8)FirstLODAvailable }; 

		return Result;
	}
#endif
	
	// No UTexture2D was provided, cannot do anything, just provide a dummy texture
	UE_LOG(LogMutable, Warning, TEXT("No UTexture2D was provided for an application-specific image parameter descriptor."));
	return CreateDummyDesc();
}


TTuple<UE::Tasks::FTask, TFunction<void()>> FUnrealMutableResourceProvider::GetMeshAsync(FName Id, int32 LODIndex, int32 SectionIndex, TFunction<void(TSharedPtr<mu::FMesh>)>& ResultCallback)
{
	// Thread: worker
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetMeshAsync);

	TSharedPtr<mu::FMesh> Result = MakeShared<mu::FMesh>();

	UE::Tasks::FTaskEvent Completion(TEXT("MutableMeshParameterLoadInGameThread"));

	auto MeshLoadCallback = [Id, LODIndex, SectionIndex, Result, Completion, Provider = this](const FSoftObjectPath&, UObject* LoadedObject) mutable
		{
			check(IsInGameThread());

			MUTABLE_CPUPROFILER_SCOPE(ActualLoad);

			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadedObject);
			if (!SkeletalMesh)
			{
				UE_LOG(LogMutable, Error, TEXT("Failed to load the skeletal mesh [%s] set for a mesh parameter. Please check that it was packaged."), *Id.ToString());
				Completion.Trigger();
				return;
			}

			// Add a reference to the skeletalmesh
			{
				bool bFound = false;
				for (FReferencedSkeletalMesh& Entry : Provider->ReferencedSkeletalMeshes)
				{
					if (Entry.SkeletalMesh == SkeletalMesh)
					{
						bFound = true;
						++Entry.ReferenceCount;
						break;
					}
				}

				if (!bFound)
				{
					Provider->ReferencedSkeletalMeshes.Add({ SkeletalMesh,1 });
				}
			}

			UModelResources* ModelResources = nullptr;

			TStrongObjectPtr<UCustomizableObject> CO = Provider->CurrentCustomizableObject.Pin();
			if (CO)
			{
				ModelResources = CO->GetPrivate()->GetModelResources();
			}

			// It is valid not to have a CO or MutableStreamedResources. It is only used for skeleton data. 
			// This may happen when updating mips, and for those operations we don't need skeleton data.
			//ensure(ModelResources);

			UE::Tasks::FTask ConversionTask = UnrealConversionUtils::ConvertSkeletalMeshFromRuntimeData(SkeletalMesh, LODIndex, SectionIndex, ModelResources, Result.Get());

			// The rest of the conversion may happen in a worker thread.
			UE::Tasks::Launch(TEXT("MeshParameterLoadFinalize"),
				[Completion, Provider, SkeletalMesh]() mutable
				{					
					// Signal
					Completion.Trigger();

					// Remove a reference to the skeletalmesh
					ExecuteOnGameThread(TEXT("MeshParameterLoadReleaseMesh"),
						[Provider, SkeletalMesh]() mutable
						{
							bool bFound = false;
							for (int32 RefIndex = 0; RefIndex < Provider->ReferencedSkeletalMeshes.Num(); ++RefIndex)
							{
								FReferencedSkeletalMesh& Entry = Provider->ReferencedSkeletalMeshes[RefIndex];
								if (Entry.SkeletalMesh == SkeletalMesh)
								{
									bFound = true;
									--Entry.ReferenceCount;
									if (Entry.ReferenceCount == 0)
									{
										Provider->ReferencedSkeletalMeshes.RemoveAtSwap(RefIndex);
									}
									break;
								}
							}

							check(bFound);
						});
				},
				// Dependencies
				ConversionTask
			);
		};

	// LoadAsync is only thread-safe when using the zenloader
	ExecuteOnGameThread(TEXT("MeshParameterLoad"),
		[Id, MeshLoadCallback]() mutable
		{
			check(IsInGameThread());

			MUTABLE_CPUPROFILER_SCOPE(MutableMeshParameterLoadInGameThread);
		
			FSoftObjectPath(Id.ToString()).LoadAsync( FLoadSoftObjectPathAsyncDelegate::CreateLambda( MeshLoadCallback ));
		}
	);

	return MakeTuple(
		// Some post-game conversion stuff can happen here in a worker thread
		UE::Tasks::Launch(TEXT("MutableMeshParameterLoadPostGame"),
			[ResultCallback, Result]()
			{
				ResultCallback(Result);
			},
			Completion ),

		// Cleanup code that will be called after the result is received in calling code.
		[]()
		{
		}
	);
}


void FUnrealMutableResourceProvider::CacheImage(FName Id, bool bUser)
{
	if (Id == NAME_None)
	{
		return;	
	}	

	{
		FScopeLock Lock(&ExternalImagesLock);

		if (FUnrealMutableImageInfo* Result = GlobalExternalImages.Find(Id))
		{
			if (bUser)
			{
				Result->ReferencesUser = true;			
			}
			else
			{
				Result->ReferencesSystem++;							
			}
		}
		else
		{
			bool bFound = false;

			FUnrealMutableImageInfo ImageInfo;

			// See if any provider provides this id.
			for (const TWeakObjectPtr<UCustomizableSystemImageProvider>& Provider : ImageProviders)
			{
				if (bFound)
				{
					break;
				}
				
				if (!Provider.IsValid())
				{
					continue;
				}

				// \TODO: all these queries could probably be optimized into a single call.
				switch (Provider->HasTextureParameterValue(Id))
				{
				case UCustomizableSystemImageProvider::ValueType::Raw:
					{
						FIntVector desc = Provider->GetTextureParameterValueSize(Id);
						TSharedPtr<mu::FImage> pResult = MakeShared<mu::FImage>(desc[0], desc[1], 1, mu::EImageFormat::RGBA_UByte, mu::EInitializationType::Black);
						Provider->GetTextureParameterValueData(Id, pResult->GetLODData(0));

						ImageInfo = FUnrealMutableImageInfo(pResult);
						
						bFound = true;
						break;
					}

				case UCustomizableSystemImageProvider::ValueType::Unreal:
					{
						UTexture2D* UnrealTexture = Provider->GetTextureParameterValue(Id);
						TSharedPtr<mu::FImage> pResult = MakeShared<mu::FImage>();

#if WITH_EDITOR
						FMutableSourceTextureData Tex(*UnrealTexture);
						EUnrealToMutableConversionError Error = ConvertTextureUnrealSourceToMutable(pResult.Get(), Tex, 0);
						if (Error != EUnrealToMutableConversionError::Success)
						{
							// This could happen in the editor, because some source textures may have changed while there was a background compilation.
							// We just show a warning and move on. This cannot happen during cooks, so it is fine.
							UE_LOG(LogMutable, Warning, TEXT("Failed to load some source texture data for [%s]. Some textures may be corrupted."), *UnrealTexture->GetName());
						}
#else
						ConvertTextureUnrealPlatformToMutable(pResult.Get(), UnrealTexture, 0);
#endif

						ImageInfo = FUnrealMutableImageInfo(pResult);

						bFound = true;
						break;
					}

				case UCustomizableSystemImageProvider::ValueType::Unreal_Deferred:
					{
						if (UTexture2D* UnrealDeferredTexture = Provider->GetTextureParameterValue(Id))
						{
							ImageInfo = FUnrealMutableImageInfo(*UnrealDeferredTexture);

							bFound = true;
						}

						break;
					}

				default:
					break;
				}
			}

			if (!bFound)
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to cache external image %s. Missing result and source texture."), *Id.ToString());
				return;
			}
			
			if (bUser)
			{
				ImageInfo.ReferencesUser = true;			
			}
				else
			{
				ImageInfo.ReferencesSystem++;							
			}
			
			GlobalExternalImages.Add(Id, ImageInfo);
		}
	}
}


void FUnrealMutableResourceProvider::UnCacheImage(FName Id, bool bUser)
{
	if (Id == NAME_None)
	{
		return;	
	}
	
	FScopeLock Lock(&ExternalImagesLock);
	if (FUnrealMutableImageInfo* Result = GlobalExternalImages.Find(Id))
	{
		if (bUser)
		{
			Result->ReferencesUser = false;			
		}
		else
		{
			--Result->ReferencesSystem;
			check(Result->ReferencesSystem >= 0); // Something went wrong. The image has been uncached more times than it has been cached.
		}
		
		if (Result->ReferencesUser + Result->ReferencesSystem == 0)
		{
			GlobalExternalImages.Remove(Id);		
		}
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to uncache external image %s. Possible double free!"), *Id.ToString());
	}
}


void FUnrealMutableResourceProvider::ClearCache(bool bUser)
{
	FScopeLock Lock(&ExternalImagesLock);
	for (TTuple<FName, FUnrealMutableImageInfo> Tuple : GlobalExternalImages)
	{
		UnCacheImage(Tuple.Key, bUser);
	}	
}


void FUnrealMutableResourceProvider::CacheImages(const mu::FParameters& Parameters)
{
	const int32 NumParams = Parameters.GetCount();
	for (int32 ParamIndex = 0; ParamIndex < NumParams; ++ParamIndex)
	{
		if (Parameters.GetType(ParamIndex) != mu::EParameterType::Image)
		{
			continue;
		}
	
		{
			const FName TextureId = Parameters.GetImageValue(ParamIndex);
			CacheImage(TextureId, false);
		}

		const int32 NumValues = Parameters.GetValueCount(ParamIndex);
		for (int32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
		{
			TSharedPtr<const mu::FRangeIndex> Range = Parameters.GetValueIndex(ParamIndex, ValueIndex);
			const FName TextureId = Parameters.GetImageValue(ParamIndex, Range.Get());

			CacheImage(TextureId, false);
		}
	}
}


void FUnrealMutableResourceProvider::UnCacheImages(const mu::FParameters& Parameters)
{
	const int32 NumParams = Parameters.GetCount();
	for (int32 ParamIndex = 0; ParamIndex < NumParams; ++ParamIndex)
	{
		if (Parameters.GetType(ParamIndex) != mu::EParameterType::Image)
		{
			continue;
		}
	
		{
			const FName TextureId = Parameters.GetImageValue(ParamIndex);
			UnCacheImage(TextureId, false);				
		}

		const int32 NumValues = Parameters.GetValueCount(ParamIndex);
		for (int32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
		{
			TSharedPtr<const mu::FRangeIndex> Range = Parameters.GetValueIndex(ParamIndex, ValueIndex);
			const FName TextureId = Parameters.GetImageValue(ParamIndex, Range.Get());

			UnCacheImage(TextureId, false);
		}
	}
}


#if WITH_EDITOR
void FUnrealMutableResourceProvider::CacheRuntimeReferencedImages(const TSharedRef<const mu::FModel>& Model, const TArray<TSoftObjectPtr<const UTexture>>& RuntimeReferencedTextures)
{
	check(IsInGameThread());
	
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::CacheRuntimeReferencedImages);
	
	FScopeLock Lock(&RuntimeReferencedLock);

	FRuntimeReferencedImages& ModelImages = RuntimeReferencedImages.Add(&Model.Get());
	ModelImages.Model = Model.ToWeakPtr();

	ModelImages.SourceTextures.Reset();
	for (const TSoftObjectPtr<const UTexture>& RuntimeReferencedTexture : RuntimeReferencedTextures)
	{
		const UTexture* Texture = RuntimeReferencedTexture.Get(); // Is already loaded.
		if (!Texture)
		{
			UE_LOG(LogMutable, Warning, TEXT("Runtime Referenced Texture [%s] was not async loaded. Forcing load sync."), *RuntimeReferencedTexture->GetPathName());
			
			Texture = MutablePrivate::LoadObject(RuntimeReferencedTexture);
			if (!Texture)
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to force load sync [%s]."), *RuntimeReferencedTexture->GetPathName());
				continue;
			}
		}

		ModelImages.SourceTextures.Emplace(*Texture); // Perform a CopyTornOff. Once done, we no longer need the texture loaded.
	}
}
#endif


TSharedPtr<mu::FImage> FUnrealMutableResourceProvider::CreateDummy()
{
	// Create a dummy image
	const int32 Size = DUMMY_IMAGE_DESC.m_size[0];
	const int32 CheckerSize = 4;
	constexpr int32 CheckerTileCount = 2;
	
#if !UE_BUILD_SHIPPING
	uint8 Colors[CheckerTileCount][4] = {{255, 255, 0, 255}, {0, 0, 255, 255}};
#else
	uint8 Colors[CheckerTileCount][4] = {{255, 255, 0, 0}, {0, 0, 255, 0}};
#endif

	TSharedPtr<mu::FImage> pResult = MakeShared<mu::FImage>(Size, Size, DUMMY_IMAGE_DESC.m_lods, DUMMY_IMAGE_DESC.m_format, mu::EInitializationType::NotInitialized);

	check(pResult->GetLODCount() == 1);
	check(pResult->GetFormat() == mu::EImageFormat::RGBA_UByte || pResult->GetFormat() == mu::EImageFormat::BGRA_UByte);
	uint8* pData = pResult->GetLODData(0);
	for (int32 X = 0; X < Size; ++X)
	{
		for (int32 Y = 0; Y < Size; ++Y)
		{
			int32 CheckerIndex = ((X / CheckerSize) + (Y / CheckerSize)) % CheckerTileCount;
			pData[0] = Colors[CheckerIndex][0];
			pData[1] = Colors[CheckerIndex][1];
			pData[2] = Colors[CheckerIndex][2];
			pData[3] = Colors[CheckerIndex][3];
			pData += 4;
		}
	}

	return pResult;
}


mu::FExtendedImageDesc FUnrealMutableResourceProvider::CreateDummyDesc()
{
	return mu::FExtendedImageDesc{ {DUMMY_IMAGE_DESC}, 0 };
}


TAutoConsoleVariable<bool> CVarMutableLockExternalImagesDuringGC(
	TEXT("Mutable.LockExternalImagesDuringGC"),
	true,
	TEXT("If true, GlobalExternalImages where all texture parameters are stored will be locked from concurrent access during the AddReferencedObjects phase of GC."),
	ECVF_Default);


void FUnrealMutableResourceProvider::AddReferencedObjects(FReferenceCollector& Collector)
{
#if WITH_EDITOR
	{
		FScopeLock Lock(&RuntimeReferencedLock);

		for (TMap<const void*, FRuntimeReferencedImages>::TIterator It = RuntimeReferencedImages.CreateIterator(); It; ++It)
		{
			if (!It.Value().Model.IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
#else
	bool bDoLock = CVarMutableLockExternalImagesDuringGC.GetValueOnAnyThread();

	if (bDoLock)
	{
		ExternalImagesLock.Lock();
	}

	for (TPair<FName, FUnrealMutableImageInfo>& Image : GlobalExternalImages)
	{
		if (Image.Value.TextureToLoad)
		{
			Collector.AddReferencedObject(Image.Value.TextureToLoad);
		}
	}

	if (bDoLock)
	{
		ExternalImagesLock.Unlock();
	}
#endif

	for (FReferencedSkeletalMesh& Mesh : ReferencedSkeletalMeshes)
	{
		Collector.AddReferencedObject(Mesh.SkeletalMesh);
	}
}


void FUnrealMutableResourceProvider::SetCurrentObject(const TWeakObjectPtr<UCustomizableObject>& InObject)
{
	check(IsInGameThread());
	CurrentCustomizableObject = InObject;
}


FUnrealMutableResourceProvider::FUnrealMutableImageInfo::FUnrealMutableImageInfo(const TSharedPtr<mu::FImage>& InImage)
{
	check(IsInGameThread())
	
	Image = InImage;
}


FUnrealMutableResourceProvider::FUnrealMutableImageInfo::FUnrealMutableImageInfo(UTexture2D& Texture)
{
	check(IsInGameThread())

#if WITH_EDITOR
	SourceTextureData = MakeShared<FMutableSourceTextureData>(Texture);
#else
	TextureToLoad = &Texture;
#endif
}
