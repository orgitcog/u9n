// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Engine/Texture.h"

#define UE_API CUSTOMIZABLEOBJECT_API

#if WITH_EDITOR

// Forward declarations
class UTexture2D;
class UAnimInstance;
class USkeletalMesh;
namespace mu
{
	class FImage;
}

struct FMutableSourceTextureData
{
	FMutableSourceTextureData() = default;
	
	UE_API FMutableSourceTextureData(const UTexture& Texture);
	
	UE_API FTextureSource& GetSource();
	
	UE_API bool GetFlipGreenChannel() const;
	
	UE_API bool HasAlphaChannel() const;

	UE_API bool GetCompressionForceAlpha() const;

	UE_API bool IsNormalComposite() const;

private:
	FTextureSource Source;
	bool bFlipGreenChannel = false;
	bool bHasAlphaChannel = false;
	bool bCompressionForceAlpha = false;
	bool bIsNormalComposite = false;
};



// Flags that can influence the mesh conversion
enum class EMutableMeshConversionFlags : uint32
{
	// 
	None = 0,
	// Ignore the skeleton and skinning
	IgnoreSkinning = 1 << 0,

	// Ignore Physics assets
	IgnorePhysics = 1 << 1,

	// Ignore Morphs
	IgnoreMorphs = 1 << 2,

	// Prevent this mesh generation from adding per mesh metadata. 
	DoNotCreateMeshMetadata = 1 << 3
};

ENUM_CLASS_FLAGS(EMutableMeshConversionFlags)

struct FMutableSourceSurfaceMetadata
{
	TSoftObjectPtr<UStreamableRenderAsset> Mesh;
	uint8 LODIndex = 0;
	uint8 SectionIndex = 0;

	bool operator==(const FMutableSourceSurfaceMetadata&) const = default;
};

uint32 CUSTOMIZABLEOBJECT_API GetTypeHash(const FMutableSourceSurfaceMetadata& Key);

struct FMutableSourceMeshData
{
	/** Assets involved in the conversion. */
	TSoftObjectPtr<UStreamableRenderAsset> Mesh;
	TSoftClassPtr<UAnimInstance> AnimInstance;
	TSoftObjectPtr<const USkeletalMesh> TableReferenceSkeletalMesh;
	FName Component = NAME_None;

	/** */
	bool bIsPassthrough = false;

	/** Required for SurfaceMetadataID */

	FMutableSourceSurfaceMetadata Metadata;

	/** Selection of the mesh section*/
	uint8 LODIndex=0;
	uint8 SectionIndex=0;
	bool bMeshMustExist=false;

	/** Required mesh properties. */
	EMutableMeshConversionFlags Flags = EMutableMeshConversionFlags::None;

	/** Required realtime mesh morphs. */
	bool bUseAllRealTimeMorphs = false;
	TArray<FString> UsedRealTimeMorphTargetNames;

	/** Context for log messages. */
	const UObject* MessageContext = nullptr;

	bool operator==(const FMutableSourceMeshData&) const = default;
};


inline uint32 GetTypeHash(const FMutableSourceMeshData& Key)
{
	uint32 GuidHash = GetTypeHash(Key.Mesh);
	GuidHash = HashCombineFast(GuidHash, GetTypeHash(Key.LODIndex));
	GuidHash = HashCombineFast(GuidHash, GetTypeHash(Key.SectionIndex));
	GuidHash = HashCombineFast(GuidHash, GetTypeHash(Key.Component));
	GuidHash = HashCombineFast(GuidHash, GetTypeHash(Key.Flags));
	return GuidHash;
}

enum class EUnrealToMutableConversionError 
{
    Success,
    UnsupportedFormat,
    CompositeImageDimensionMismatch,
    CompositeUnsupportedFormat,
    Unknown
};

CUSTOMIZABLEOBJECT_API EUnrealToMutableConversionError ConvertTextureUnrealSourceToMutable(mu::FImage* OutResult, FMutableSourceTextureData&, uint8 MipmapsToSkip);

#endif

#undef UE_API
