// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuR/Image.h"
#include "MuR/System.h"
#include "UObject/GCObject.h"
#include "Containers/Ticker.h"
#include "Containers/Map.h"
#include "Containers/Queue.h"
#include "PixelFormat.h"
#include "Tasks/Task.h"

class UCustomizableObject;
class UTexture2D;


/** Implementation of a mutable core provider for image parameters that are application-specific. */
class FUnrealMutableResourceProvider : public TSharedFromThis<FUnrealMutableResourceProvider>, public mu::FExternalResourceProvider, public FGCObject
{

public:
	// mu::ExternalResourceProvider interface
	// Thread: worker
	virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetImageAsync(FName Id, uint8 MipmapsToSkip, TFunction<void(TSharedPtr<mu::FImage>)>& ResultCallback) override;
	virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetReferencedImageAsync(const void* ModelPtr, int32 Id, uint8 MipmapsToSkip, TFunction<void(TSharedPtr<mu::FImage>)>& ResultCallback) override;
	virtual mu::FExtendedImageDesc GetImageDesc(FName Id) override;
	virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetMeshAsync(FName Id, int32 LODIndex, int32 SectionIndex, TFunction<void(TSharedPtr<mu::FMesh>)>& ResultCallback) override;

	
	// Own interface	
	// Thread: Game
	/** Add a reference to the image. If it was not cached it caches it.
	 * @param bUser if true, adds a reference to the user reference counter. If false, adds a reference to the system reference counter. */
	void CacheImage(FName Id, bool bUser);

	/** Removes a reference to the image. If all references are removed, it uncaches the image.
	 * @param bUser if true, removes a reference from the user reference counter. If false, removes a reference from the system reference counter. */
	void UnCacheImage(FName Id, bool bUser);

	/** Removes a reference to all images. All images which no longer have references will be uncached.
	 * @param bUser if true, removes a references from the user reference counter. If false, removes a reference from the system reference counter. */
	void ClearCache(bool bUser);

	void CacheImages(const mu::FParameters& Parameters);
	void UnCacheImages(const mu::FParameters& Parameters);

#if WITH_EDITOR
	void CacheRuntimeReferencedImages(const TSharedRef<const mu::FModel>& Model, const TArray<TSoftObjectPtr<const UTexture>>& RuntimeReferencedTextures);
#endif
	
	/** List of actual image providers that have been registered to the CustomizableObjectSystem. */
	TArray< TWeakObjectPtr<class UCustomizableSystemImageProvider> > ImageProviders;

	// Register and unregister the current CO that we are working on. This is called by internal code when building instances.
	void SetCurrentObject(const TWeakObjectPtr<UCustomizableObject>&);

	struct FReferencedSkeletalMesh
	{
		TObjectPtr<class USkeletalMesh> SkeletalMesh;
		uint32 ReferenceCount = 0;
	};
	TArray<FReferencedSkeletalMesh> ReferencedSkeletalMeshes;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FUnrealMutableImageProvider");
	}

private:
	struct FUnrealMutableImageInfo
	{
		FUnrealMutableImageInfo() = default;

		FUnrealMutableImageInfo(const TSharedPtr<mu::FImage>& InImage);

		FUnrealMutableImageInfo(UTexture2D& Texture);

		TSharedPtr<mu::FImage> Image;

#if WITH_EDITOR
		TSharedPtr<FMutableSourceTextureData> SourceTextureData;
#else
		/** If the above Image has not been loaded in the game thread, the TextureToLoad bulk data will be loaded
		* from the Mutable thread when it's needed */
		TObjectPtr<UTexture2D> TextureToLoad = nullptr;
#endif
		
		/** true of the reference maintained by the user. */
		bool ReferencesUser = false;
		
		/** Number of reference maintained by the system. */
		int32 ReferencesSystem = 0;
	};

	/** Runtime reference to the CO being updated. It is needed for example for mesh conversion, to access to the bone id map.  */
	TWeakObjectPtr<UCustomizableObject> CurrentCustomizableObject;

	static inline const mu::FImageDesc DUMMY_IMAGE_DESC = 
			mu::FImageDesc {mu::FImageSize(32, 32), mu::EImageFormat::RGBA_UByte, 1};

	/** This will be called if an image Id has been requested by Mutable core but it has not been provided by any provider. */
	static TSharedPtr<mu::FImage> CreateDummy();
	static mu::FExtendedImageDesc CreateDummyDesc();

	/** Map of Ids to external textures that may be required for any instance or Mutable texture mip under construction.
	* This is only safely written from the game thread protected by the following critical section, and it
	* is safely read from the mutable thread during the update of the instance or texture mips. */
	TMap<FName, FUnrealMutableImageInfo> GlobalExternalImages;

#if WITH_EDITOR
	struct FRuntimeReferencedImages
	{
		TArray<FMutableSourceTextureData> SourceTextures;
		TWeakPtr<const mu::FModel> Model;
	};
	
	TMap<const void*, FRuntimeReferencedImages> RuntimeReferencedImages;
#endif
	
	/** Access to GlobalExternalImages must be protected with this because it may be accessed concurrently from the 
	Game thread to modify it and from the Mutable thread to read it. */
	FCriticalSection ExternalImagesLock;

#if WITH_EDITOR
	FCriticalSection RuntimeReferencedLock;
#endif
};
