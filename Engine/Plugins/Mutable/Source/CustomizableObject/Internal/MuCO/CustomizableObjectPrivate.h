// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectCompilerTypes.h"
#include "MuCO/CustomizableObjectClothingTypes.h"
#include "MuCO/CustomizableObjectStreamedResourceData.h"
#include "MuCO/StateMachine.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCO/CustomizableObjectIdentifier.h"
#include "Serialization/BulkData.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "MuR/Types.h"

#if WITH_EDITOR
#include "Misc/Guid.h"
#include "Engine/DataTable.h"
#endif

#include "CustomizableObjectPrivate.generated.h"

namespace mu 
{ 
	class FModel;
	struct FBoneName;
}

#if WITH_EDITOR
namespace MutablePrivate
{
	struct FClassifyNode;
}

namespace UE::DerivedData
{
	struct FValueId;
	struct FCacheKey;
	enum class ECachePolicy : uint32;
}
#endif

class USkeletalMesh;
class USkeleton;
class UPhysicsAsset;
class UMaterialInterface;
class UTexture;
class UAnimInstance;
class UAssetUserData;
class USkeletalMeshLODSettings;
class UModelResources;
struct FModelStreamableBulkData;
struct FObjectAndNameAsStringProxyArchive;
struct FCustomizableObjectInstanceDescriptor;


FGuid CUSTOMIZABLEOBJECT_API GenerateIdentifier(const UCustomizableObject& CustomizableObject);

FString CUSTOMIZABLEOBJECT_API GetModelResourcesNameForPlatform(const UCustomizableObject& CustomizableObject, const ITargetPlatform& Platform);

// A USTRUCT version of FMeshToMeshVertData in SkeletalMeshTypes.h
// We are taking advantage of the padding data to store from which asset this data comes from
// maintaining the same memory footprint than the original.
USTRUCT()
struct FCustomizableObjectMeshToMeshVertData
{
	GENERATED_USTRUCT_BODY()

	FCustomizableObjectMeshToMeshVertData() = default;

	explicit FCustomizableObjectMeshToMeshVertData(const FMeshToMeshVertData& Original)
		: Weight(Original.Weight)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			PositionBaryCoordsAndDist[i] = Original.PositionBaryCoordsAndDist[i];
			NormalBaryCoordsAndDist[i] = Original.NormalBaryCoordsAndDist[i];
			TangentBaryCoordsAndDist[i] = Original.TangentBaryCoordsAndDist[i];
			SourceMeshVertIndices[i] = Original.SourceMeshVertIndices[i];
		}
	}
		

	explicit operator FMeshToMeshVertData() const
	{
		FMeshToMeshVertData ReturnValue;

		for (int32 i = 0; i < 4; ++i)
		{
			ReturnValue.PositionBaryCoordsAndDist[i] = PositionBaryCoordsAndDist[i];
			ReturnValue.NormalBaryCoordsAndDist[i] = NormalBaryCoordsAndDist[i];
			ReturnValue.TangentBaryCoordsAndDist[i] = TangentBaryCoordsAndDist[i];
			ReturnValue.SourceMeshVertIndices[i] = SourceMeshVertIndices[i];
		}
		ReturnValue.Weight = Weight;
		ReturnValue.Padding = 0;

		return ReturnValue;
	}

	
	// Barycentric coords and distance along normal for the position of the final vert
	UPROPERTY()
	float PositionBaryCoordsAndDist[4] = {0.f};

	// Barycentric coords and distance along normal for the location of the unit normal endpoint
	// Actual normal = ResolvedNormalPosition - ResolvedPosition
	UPROPERTY()
	float NormalBaryCoordsAndDist[4] = {0.f};

	// Barycentric coords and distance along normal for the location of the unit Tangent endpoint
	// Actual normal = ResolvedNormalPosition - ResolvedPosition
	UPROPERTY()
	float TangentBaryCoordsAndDist[4] = {0.f};

	// Contains the 3 indices for verts in the source mesh forming a triangle, the last element
	// is a flag to decide how the skinning works, 0xffff uses no simulation, and just normal
	// skinning, anything else uses the source mesh and the above skin data to get the final position
	UPROPERTY()
	uint16	 SourceMeshVertIndices[4] = {0u, 0u, 0u, 0u};

	UPROPERTY()
	float Weight = 0.0f;

	// Non serialized, unused padding. This is present in the FMeshToMeshVertData struct as Padding for alignment.
	uint32 UnusedPadding = 0;

	/**
	 * Serializer
	 *
	 * @param Ar - archive to serialize with
	 * @param V - vertex to serialize
	 * @return archive that was used
	 */
	friend FArchive& operator<<(FArchive& Ar, FCustomizableObjectMeshToMeshVertData& V)
	{
		Ar << V.PositionBaryCoordsAndDist[0];
		Ar << V.PositionBaryCoordsAndDist[1];
		Ar << V.PositionBaryCoordsAndDist[2];
		Ar << V.PositionBaryCoordsAndDist[3];
		Ar << V.NormalBaryCoordsAndDist[0];
		Ar << V.NormalBaryCoordsAndDist[1];
		Ar << V.NormalBaryCoordsAndDist[2];
		Ar << V.NormalBaryCoordsAndDist[3];
		Ar << V.TangentBaryCoordsAndDist[0];
		Ar << V.TangentBaryCoordsAndDist[1];
		Ar << V.TangentBaryCoordsAndDist[2];
		Ar << V.TangentBaryCoordsAndDist[3];

		Ar << V.SourceMeshVertIndices[0];
		Ar << V.SourceMeshVertIndices[1];
		Ar << V.SourceMeshVertIndices[2];
		Ar << V.SourceMeshVertIndices[3];

		Ar << V.Weight;

		return Ar;
	}
};
static_assert(sizeof(FCustomizableObjectMeshToMeshVertData) == sizeof(float)*4*3 + sizeof(uint16)*4 + sizeof(float) + sizeof(uint32));
template<> struct TCanBulkSerialize<FCustomizableObjectMeshToMeshVertData> { enum { Value = true }; };


// Warning! MutableCompiledDataHeader must be the first data serialized in a stream
struct MutableCompiledDataStreamHeader
{
	int32 InternalVersion=0;
	FGuid VersionId;

	MutableCompiledDataStreamHeader() { }
	MutableCompiledDataStreamHeader(int32 InInternalVersion, FGuid InVersionId) : InternalVersion(InInternalVersion), VersionId(InVersionId) { }

	friend FArchive& operator<<(FArchive& Ar, MutableCompiledDataStreamHeader& Header)
	{
		Ar << Header.InternalVersion;
		Ar << Header.VersionId;

		return Ar;
	}
};

struct FCustomizableObjectStreameableResourceId
{
	enum class EType : uint8
	{
		None                  = 0,
		AssetUserData         = 1,
		RealTimeMorphTarget   = 2,
		Clothing              = 3,
	};

	uint64 Id   : 64 - 8;
	uint64 Type : 8;

	friend bool operator==(FCustomizableObjectStreameableResourceId A, FCustomizableObjectStreameableResourceId B)
	{
		return BitCast<uint64>(A) == BitCast<uint64>(B);
	}
};
static_assert(sizeof(FCustomizableObjectStreameableResourceId) == sizeof(uint64));


USTRUCT()
struct FMutableRemappedBone
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName Name;
	
	UPROPERTY()
	uint32 Hash = 0;
	
	bool operator==(const FName& InName)
	{
		return Name == InName;
	}
};


USTRUCT()
struct FMutableModelParameterValue
{
	GENERATED_USTRUCT_BODY()

	FMutableModelParameterValue() = default;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	int Value = 0;
};


USTRUCT()
struct FMutableModelParameterProperties
{
	GENERATED_USTRUCT_BODY()

	FMutableModelParameterProperties() = default;
	FString Name;

	UPROPERTY()
	EMutableParameterType Type = EMutableParameterType::None;

	UPROPERTY()
	TArray<FMutableModelParameterValue> PossibleValues;
};


class FMeshCache
{
public:
	USkeletalMesh* Get(const TArray<mu::FResourceID>& Key);

	void Add(const TArray<mu::FResourceID>& Key, USkeletalMesh* Value);

private:
	TMap<TArray<mu::FResourceID>, TWeakObjectPtr<USkeletalMesh>> GeneratedMeshes;
};


class FSkeletonCache
{
public:
	USkeleton* Get(const TArray<uint16>& Key);

	void Add(const TArray<uint16>& Key, USkeleton* Value);

private:
	TMap<TArray<uint16>, TWeakObjectPtr<USkeleton>> MergedSkeletons;
};


struct FCustomizableObjectStatusTypes
{
	enum class EState : uint8
	{
		Loading = 0, // Waiting for PostLoad and Asset Registry to finish.
		ModelLoaded, // Model loaded correctly.
		NoModel, // No model (due to no model not found and automatic compilations disabled).
		// Compiling, // Compiling the CO. Equivalent to UCustomizableObject::IsLocked = true.

		Count,
	};
	
	static constexpr EState StartState = EState::NoModel;

	static constexpr bool ValidTransitions[3][3] =
	{
		// TO
		// Loading, ModelLoaded, NoModel // FROM
		{false,   true,        true},  // Loading
		{false,   true,        true},  // ModelLoaded
		{true,    true,        true},  // NoModel
	};
};

using FCustomizableObjectStatus = FStateMachine<FCustomizableObjectStatusTypes>;


USTRUCT()
struct FMutableModelImageProperties
{
	GENERATED_USTRUCT_BODY()

	FMutableModelImageProperties()
		: Filter(TF_Default)
		, SRGB(0)
		, FlipGreenChannel(0)
		, IsPassThrough(0)
		, LODBias(0)
		, MipGenSettings(TextureMipGenSettings::TMGS_FromTextureGroup)
		, LODGroup(TEXTUREGROUP_World)
		, AddressX(TA_Clamp)
		, AddressY(TA_Clamp)
	{}

	FMutableModelImageProperties(const FString& InTextureParameterName, TextureFilter InFilter, uint32 InSRGB,
		uint32 InFlipGreenChannel, uint32 bInIsPassThrough, int32 InLODBias, TEnumAsByte<TextureMipGenSettings> InMipGenSettings, 
		TEnumAsByte<enum TextureGroup> InLODGroup, TEnumAsByte<enum TextureAddress> InAddressX, TEnumAsByte<enum TextureAddress> InAddressY)
		: TextureParameterName(InTextureParameterName)
		, Filter(InFilter)
		, SRGB(InSRGB)
		, FlipGreenChannel(InFlipGreenChannel)
		, IsPassThrough(bInIsPassThrough)
		, LODBias(InLODBias)
		, MipGenSettings(InMipGenSettings)
		, LODGroup(InLODGroup)
		, AddressX(InAddressX)
		, AddressY(InAddressY)
	{}

	// Name in the material.
	UPROPERTY()
	FString TextureParameterName;

	UPROPERTY()
	TEnumAsByte<enum TextureFilter> Filter;

	UPROPERTY()
	uint32 SRGB : 1;

	UPROPERTY()
	uint32 FlipGreenChannel : 1;

	UPROPERTY()
	uint32 IsPassThrough : 1;

	UPROPERTY()
	int32 LODBias;

	UPROPERTY()
	TEnumAsByte<TextureMipGenSettings> MipGenSettings;

	UPROPERTY()
	TEnumAsByte<enum TextureGroup> LODGroup;

	UPROPERTY()
	TEnumAsByte<enum TextureAddress> AddressX;

	UPROPERTY()
	TEnumAsByte<enum TextureAddress> AddressY;

	CUSTOMIZABLEOBJECT_API bool operator!=(const FMutableModelImageProperties& rhs) const;
};


USTRUCT()
struct FMutableRefSocket
{
	GENERATED_BODY()

	UPROPERTY()
	FName SocketName;
	UPROPERTY()
	FName BoneName;

	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;
	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;
	UPROPERTY()
	FVector RelativeScale = FVector::ZeroVector;

	UPROPERTY()
	bool bForceAlwaysAnimated = false;

	// When two sockets have the same name, the one with higher priority will be picked and the other discarded
	UPROPERTY()
	int32 Priority = -1;

	CUSTOMIZABLEOBJECT_API bool operator ==(const FMutableRefSocket& Other) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSocket& Data);
#endif
};


USTRUCT()
struct FMutableRefLODInfo
{
	GENERATED_BODY()

	UPROPERTY()
	float ScreenSize = 0.f;

	UPROPERTY()
	float LODHysteresis = 0.f;

	UPROPERTY()
	bool bSupportUniformlyDistributedSampling = false;

	UPROPERTY()
	bool bAllowCPUAccess = false;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODInfo& Data);
#endif
};


USTRUCT()
struct FMutableRefLODRenderData
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsLODOptional = false;

	UPROPERTY()
	bool bStreamedDataInlined = false;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODRenderData& Data);
#endif
};


USTRUCT()
struct FMutableRefLODData
{
	GENERATED_BODY()

	UPROPERTY()
	FMutableRefLODInfo LODInfo;

	UPROPERTY()
	FMutableRefLODRenderData RenderData;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefLODData& Data);
#endif
};


USTRUCT()
struct FMutableRefSkeletalMeshSettings
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnablePerPolyCollision = false;

	UPROPERTY()
	float DefaultUVChannelDensity = 0.f;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshSettings& Data);
#endif
};


USTRUCT()
struct FMutableRefSkeletalMeshData
{
	GENERATED_BODY()

	// Reference Skeletal Mesh
	UPROPERTY()
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	// Path to load the ReferenceSkeletalMesh
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> SoftSkeletalMesh;

	// Optional USkeletalMeshLODSettings
	UPROPERTY()
	TObjectPtr<USkeletalMeshLODSettings> SkeletalMeshLODSettings;

	// LOD info
	UPROPERTY()
	TArray<FMutableRefLODData> LODData;

	// Sockets
	UPROPERTY()
	TArray<FMutableRefSocket> Sockets;

	// Bounding Box
	UPROPERTY()
	FBoxSphereBounds Bounds = FBoxSphereBounds(ForceInitToZero);

	// Settings
	UPROPERTY()
	FMutableRefSkeletalMeshSettings Settings;

	// Skeleton
	UPROPERTY()
	TObjectPtr<USkeleton> Skeleton;

	// PhysicsAsset
	UPROPERTY()
	TObjectPtr<UPhysicsAsset> PhysicsAsset;

	// Post Processing AnimBP
	UPROPERTY()
	TSoftClassPtr<UAnimInstance> PostProcessAnimInst;

	// Shadow PhysicsAsset
	UPROPERTY()
	TObjectPtr<UPhysicsAsset> ShadowPhysicsAsset;

	// Asset user data
	UPROPERTY()
	TArray<int32> AssetUserDataIndices;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshData& Data);
#endif
};


USTRUCT()
struct FAnimBpOverridePhysicsAssetsInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftClassPtr<UAnimInstance> AnimInstanceClass;

	UPROPERTY()
	TSoftObjectPtr<UPhysicsAsset> SourceAsset;

	UPROPERTY()
	int32 PropertyIndex = -1;

	CUSTOMIZABLEOBJECT_API bool operator==(const FAnimBpOverridePhysicsAssetsInfo& Rhs) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FAnimBpOverridePhysicsAssetsInfo& Info);
#endif
};


USTRUCT()
struct FMutableSkinWeightProfileInfo
{
	GENERATED_USTRUCT_BODY()

	FMutableSkinWeightProfileInfo() {};

	FMutableSkinWeightProfileInfo(FName InName, uint32 InNameId, bool InDefaultProfile, int8 InDefaultProfileFromLODIndex) : Name(InName),
		NameId(InNameId), DefaultProfile(InDefaultProfile), DefaultProfileFromLODIndex(InDefaultProfileFromLODIndex) {};

	UPROPERTY()
	FName Name;

	UPROPERTY()
	uint32 NameId = 0;

	UPROPERTY()
	bool DefaultProfile = false;

	UPROPERTY(meta = (ClampMin = 0))
	int8 DefaultProfileFromLODIndex = 0;

	CUSTOMIZABLEOBJECT_API bool operator==(const FMutableSkinWeightProfileInfo& Other) const;

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FMutableSkinWeightProfileInfo& Info);
#endif
};


// TODO: Optimize this struct
USTRUCT()
struct FMutableStreamableBlock
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint32 FileId = 0;

	/** Used to store properties of the data, necessary for its recovery. For instance if it is high-res. */
	UPROPERTY()
	uint16 Flags = 0;
	
	uint16 IsPrefetched = 0;

	UPROPERTY()
	uint64 Offset = 0;

	friend FArchive& operator<<(FArchive& Ar, FMutableStreamableBlock& Data)
	{
		Ar << Data.FileId;
		Ar << Data.Flags;
		Ar << Data.Offset;
		return Ar;
	}
};
static_assert(sizeof(FMutableStreamableBlock) == 8 * 2);


USTRUCT()
struct FRealTimeMorphStreamable
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	TArray<FName> NameResolutionMap;

	UPROPERTY()
	FMutableStreamableBlock Block;

	UPROPERTY()
	uint32 Size = 0;

	UPROPERTY()
	uint32 SourceId = 0;

	friend FArchive& operator<<(FArchive& Ar, FRealTimeMorphStreamable& Elem)
	{
		Ar << Elem.NameResolutionMap;
		Ar << Elem.Size;
		Ar << Elem.Block;
		Ar << Elem.SourceId;

		return Ar;
	}
};

USTRUCT()
struct FMutableMeshMetadata
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	uint32 MorphMetadataId = 0;

	UPROPERTY()
	uint32 ClothingMetadataId = 0;

	UPROPERTY()
	uint32 SurfaceMetadataId = 0;

	friend FArchive& operator<<(FArchive& Ar, FMutableMeshMetadata& Elem)
	{
		Ar << Elem.MorphMetadataId;
		Ar << Elem.ClothingMetadataId;
		Ar << Elem.SurfaceMetadataId;

		return Ar;
	}
};


USTRUCT()
struct FMutableSurfaceMetadata
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	FName MaterialSlotName = FName{};
	
	UPROPERTY()
	bool bCastShadow = true;

	friend FArchive& operator<<(FArchive& Ar, FMutableSurfaceMetadata& Elem)
	{
		Ar << Elem.MaterialSlotName;
		Ar << Elem.bCastShadow;

		return Ar;
	}
};


USTRUCT()
struct FClothingStreamable
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	int32 ClothingAssetIndex = INDEX_NONE;
	
	UPROPERTY()
	int32 ClothingAssetLOD = INDEX_NONE;
	
	UPROPERTY()
	int32 PhysicsAssetIndex = INDEX_NONE;

	UPROPERTY()
	uint32 Size = 0;

	UPROPERTY()
	FMutableStreamableBlock Block;

	UPROPERTY()
	uint32 SourceId = 0;

	friend FArchive& operator<<(FArchive& Ar, FClothingStreamable& Elem)
	{
		Ar << Elem.ClothingAssetIndex;
		Ar << Elem.ClothingAssetLOD;
		Ar << Elem.PhysicsAssetIndex;
		Ar << Elem.Size;
		Ar << Elem.Block;
		Ar << Elem.SourceId;
		return Ar;
	}
};

USTRUCT()
struct FMorphTargetVertexData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector3f PositionDelta = FVector3f::ZeroVector;

	UPROPERTY()
	FVector3f TangentZDelta = FVector3f::ZeroVector;
	
	UPROPERTY()
	uint32 MorphNameIndex = 0;

	friend FArchive& operator<<(FArchive& Ar, FMorphTargetVertexData& Data)
	{
		Ar << Data.PositionDelta;
		Ar << Data.TangentZDelta;
		Ar << Data.MorphNameIndex;

		return Ar;
	}
};
static_assert(sizeof(FMorphTargetVertexData) == sizeof(FVector3f)*2 + sizeof(uint32)); // Make sure no padding is present.
template<> struct TCanBulkSerialize<FMorphTargetVertexData> { enum { Value = true }; };


struct FMutableParameterIndex
{
	FMutableParameterIndex(int32 InIndex, int32 InTypedIndex)
	{
		Index = InIndex;
		TypedIndex = InTypedIndex;
	}

	int32 Index = INDEX_NONE;
	int32 TypedIndex = INDEX_NONE;
};


USTRUCT()
struct FIntegerParameterOptionKey
{
	GENERATED_BODY()

	UPROPERTY()
	FString ParameterName;

	UPROPERTY()
	FString ParameterOption;

#if WITH_EDITOR
	friend CUSTOMIZABLEOBJECT_API uint32 GetTypeHash(const FIntegerParameterOptionKey& Key);
	bool operator==(const FIntegerParameterOptionKey& Other) const = default;
#endif
};


USTRUCT()
struct FIntegerParameterOptionDataTable
{
	GENERATED_BODY()

	UPROPERTY()
	TSet<TSoftObjectPtr<UDataTable>> DataTables;
};


USTRUCT()
struct FIntegerParameterUIData
{
	GENERATED_BODY()

	FIntegerParameterUIData() = default;
	
	CUSTOMIZABLEOBJECT_API FIntegerParameterUIData(const FMutableParamUIMetadata& InParamUIMetadata);
	
	UPROPERTY()
	FMutableParamUIMetadata ParamUIMetadata;

	friend FArchive& operator<<(FArchive& Ar, FIntegerParameterUIData& Struct);
};


USTRUCT()
struct FMutableParameterData
{
	GENERATED_BODY()

	FMutableParameterData() = default;
	
	CUSTOMIZABLEOBJECT_API FMutableParameterData(const FMutableParamUIMetadata& InParamUIMetadata, EMutableParameterType InType);

	UPROPERTY()
	FMutableParamUIMetadata ParamUIMetadata;

	/** Parameter type */
	UPROPERTY()
	EMutableParameterType Type = EMutableParameterType::None;

	/** In the case of an integer parameter, store here all options */
	UPROPERTY()
	TMap<FString, FIntegerParameterUIData> ArrayIntegerParameterOption;

	/** How are the different options selected (one, one or none, etc...) */
	UPROPERTY()
	ECustomizableObjectGroupType IntegerParameterGroupType = ECustomizableObjectGroupType::COGT_ONE_OR_NONE;
	
	friend FArchive& operator<<(FArchive& Ar, FMutableParameterData& Struct);
};


USTRUCT()
struct FMutableStateData
{
	GENERATED_BODY()

	UPROPERTY()
	FMutableStateUIMetadata StateUIMetadata;

	/** In this mode instances and their temp data will be reused between updates. It will be much faster but spend as much as ten times the memory.
	 * Useful for customization lockers with few characters that are going to have their parameters changed many times, not for in-game */
	UPROPERTY()
	bool bLiveUpdateMode = false;

	/** If this is enabled, texture streaming won't be used for this state, and full images will be generated when an instance is first updated. */
	UPROPERTY()
	bool bDisableTextureStreaming = false;

	UPROPERTY()
	bool bReuseInstanceTextures = false;

	UPROPERTY()
	TMap<FString, FString> ForcedParameterValues;

	friend FArchive& operator<<(FArchive& Ar, FMutableStateData& Struct);
};


/** This is encoded in exact bits so if extended, review its uses everywhere. */
enum class EMutableFileFlags : uint8
{
	None	= 0,
	HighRes = 1 << 0
};


namespace MutablePrivate
{
	enum class EStreamableDataType : uint32 // uint32 for padding and DDC purposes
	{
		None = 0,
		Model,
		RealTimeMorph,
		Clothing,

		DataTypeCount
	};

#if WITH_EDITOR
	struct FBlock
	{
		/** Used on some data types as the index to the block stored in the CustomizableObject */
		uint32 Id;

		/** Used on some data types to group blocks. */
		uint32 SourceId;

		/** Size of the data block. */
		uint32 Size;

		uint32 Padding = 0;

		/** Offset in the full source streamed data file that is created when compiling. */
		uint64 Offset;

		friend FArchive& operator<<(FArchive& Ar, FBlock& Data)
		{
			Ar << Data.Id;
			Ar << Data.SourceId;
			Ar << Data.Size;
			Ar << Data.Offset;
			return Ar;
		};
	};
	//template<> struct TCanBulkSerialize<FBlock> { enum { Value = true }; };

	struct FFile
	{
		EStreamableDataType  DataType = EStreamableDataType::None;

		/** Rom ResourceType. */
		uint16 ResourceType = 0;

		/** Common flags of the data stored in this file. See EMutableFileFlags.*/
		uint16 Flags = 0;

		/** Id generated from a hash of the file content + offset to avoid collisions. */
		uint32 Id = 0;

		/** File size */
		uint64 Size = 0;

		/** List of blocks that are contained in the file, in order. */
		TArray<FBlock> Blocks;

		/** Get the total size of blocks in this file. */
		CUSTOMIZABLEOBJECT_API uint64 GetSize() const;

		/** Copy the requested block to the requested buffer and return its size. */
		CUSTOMIZABLEOBJECT_API void GetFileData(struct FMutableCachedPlatformData*, TArray64<uint8>& DataDestination, bool bDropData);

		friend FArchive& operator<<(FArchive& Ar, FFile& Data)
		{
			Ar << Data.DataType;
			Ar << Data.ResourceType;
			Ar << Data.Flags;
			Ar << Data.Id;
			Ar << Data.Blocks;
			return Ar;
		};
	};

	struct FFileCategoryID
	{
		FFileCategoryID(EStreamableDataType DataType, uint16 ResourceType, uint16 Flags);

		FFileCategoryID() = default;

		// EDataType
		EStreamableDataType DataType = EStreamableDataType::None;

		/** Rom ResourceType. */
		uint16 ResourceType = 0;

		/** Rom flags  */
		uint16 Flags = 0;

		friend uint32 GetTypeHash(const FFileCategoryID& Key);
		bool operator==(const FFileCategoryID& Other) const = default;
	};


	struct FFileCategory
	{
		FFileCategoryID Id;

		// Accumulated size of resources from this category
		uint64 DataSize = 0;

		// Categories within a bucket with a limited number of files will use sequential ID starting at FirstFile
		// and up to FirstFile + NumFiles.
		uint32 FirstFile = 0;
		uint32 NumFiles = 0;
	};


	/** Group bulk data by categories. */
	struct FFileBucket
	{
		// Resources belonging to these categories will be added to the bucket.
		TArray<FFileCategory> Categories;

		// Accumulated size of the resources of all categories within this bucket
		uint64 DataSize = 0;
	};

	struct FModelStreamableData
	{
		void Get(uint32 Key, TArrayView64<uint8> Destination, bool bDropData)
		{
			TArray64<uint8>* Buffer = Data.Find(Key);
			check(Buffer);
			check(Destination.Num() == Buffer->Num());
			FMemory::Memcpy(Destination.GetData(), Buffer->GetData(), Buffer->Num());

			if (bDropData)
			{
				Buffer->Empty();
			}
		}

		void Set(uint32 Key, const uint8* Source, int64 Size)
		{
			check(Source);
			check(Size);
			TArray64<uint8>& Buffer = Data.Add(Key);
			check(Buffer.Num() == 0);
			Buffer.SetNumUninitialized(Size);
			FMemory::Memcpy(Buffer.GetData(), Source, Size);
		}

		// Temp, to be replaced with disk storage
		TMap<uint32, TArray64<uint8> > Data;
	};


	struct FMutableCachedPlatformData
	{
		/** mu::Model */
		TSharedPtr<mu::FModel, ESPMode::ThreadSafe> Model;
		
		/** UModelResources */
		TStrongObjectPtr<UModelResources> ModelResources;

		/** Streamable resources info such as files and offsets. */
		TSharedPtr<FModelStreamableBulkData> ModelStreamableBulkData;

		/** Struct containing map of RomId to RomBytes. */
		FModelStreamableData ModelStreamableData;

		/** */
		FModelStreamableData MorphStreamableData;

		/** */
		FModelStreamableData ClothingStreamableData;

		/** List of files to serialize. Each file has a list of binary blocks to be serialized. */
		TArray<FFile> BulkDataFiles;
	};


	/** Generate the list of BulkData files with a restriction to the number of files to generate per bucket.
	 *  Resources will be split into two buckets for non-optional and optional BulkData.
	 */
	void CUSTOMIZABLEOBJECT_API GenerateBulkDataFilesListWithFileLimit(
		TSharedPtr<const mu::FModel, ESPMode::ThreadSafe> Model,
		FModelStreamableBulkData& ModelStreamableBulkData,
		uint32 NumFilesPerBucket,
		bool bAllowSplit,
		TArray<FFile>& OutBulkDataFiles);

	/** Generate the list of BulkData files with a soft restriction to the size of the files.
	 */
	void CUSTOMIZABLEOBJECT_API GenerateBulkDataFilesListWithSizeLimit(
		TSharedPtr<const mu::FModel, ESPMode::ThreadSafe> Model,
		FModelStreamableBulkData& ModelStreamableBulkData,
		const ITargetPlatform* TargetPlatform,
		uint64 TargetBulkDataFileBytes,
		TArray<FFile>& OutBulkDataFiles);

	/** Compute the number of files and sizes the BulkData will be split into and update
	 * the streamable's FileIds and Offsets.
	 */
	void GenerateBulkDataFilesList(
		TSharedPtr<const mu::FModel, ESPMode::ThreadSafe> Model,
		FModelStreamableBulkData& StreamableBulkData,
		bool bUseRomTypeAndFlagsToFilter,
		TFunctionRef<void(const FFileCategoryID&, const FClassifyNode&, TArray<FFile>&)> CreateFileList,
		TArray<FFile>& OutBulkDataFiles);

	void CUSTOMIZABLEOBJECT_API SerializeBulkDataFiles(
		FMutableCachedPlatformData& CachedPlatformData,
		TArray<FFile>& BulkDataFiles,
		TFunctionRef<void(FFile&, TArray64<uint8>&, uint32 FileIndex)> WriteFile,
		bool bDropData);

	UE::DerivedData::FValueId CUSTOMIZABLEOBJECT_API GetDerivedDataModelId();
	UE::DerivedData::FValueId CUSTOMIZABLEOBJECT_API GetDerivedDataModelResourcesId();
	UE::DerivedData::FValueId CUSTOMIZABLEOBJECT_API GetDerivedDataModelStreamableBulkDataId();
	UE::DerivedData::FValueId CUSTOMIZABLEOBJECT_API GetDerivedDataBulkDataFilesId();
#endif
}

struct FModelStreamableBulkData
{
	/** Map of Hash to Streaming blocks, used to stream a block of data representing a resource from the BulkData */
	TMap<uint32, FMutableStreamableBlock> ModelStreamables;

	TMap<uint32, FClothingStreamable> ClothingStreamables;

	TMap<uint32, FRealTimeMorphStreamable> RealTimeMorphStreamables;

	TArray<FByteBulkData> StreamableBulkData;

#if WITH_EDITORONLY_DATA
	// Used to know if roms and other resources must be streamed from the DDC.
	bool bIsStoredInDDC = false;
	UE::DerivedData::FCacheKey DDCKey = UE::DerivedData::FCacheKey::Empty;
	UE::DerivedData::ECachePolicy DDCDefaultPolicy = UE::DerivedData::ECachePolicy::Default;
	TArray<FIoHash> DDCValues;
#endif

	// File path to stream resources from when not using FByteBulkData or DDC.
	FString FullFilePath;

	CUSTOMIZABLEOBJECT_API void Serialize(FArchive& Ar, UObject* Owner, bool bCooked);

#if WITH_EDITORONLY_DATA
	friend FArchive& operator<<(FArchive& Ar, FModelStreamableBulkData& Data)
	{
		Ar << Data.ModelStreamables;
		Ar << Data.ClothingStreamables;
		Ar << Data.RealTimeMorphStreamables;
		Ar << Data.DDCValues;
		// Don't serialize FByteBulkData manually, the data will be skipped.
		
		Ar << Data.FullFilePath;

		return Ar;
	}
#endif
};

/** Interface class to allow custom serialization of FModelStreamableBulkData and its FBulkData. */
UCLASS()
class UModelStreamableData : public UObject
{
	GENERATED_BODY()

	UModelStreamableData();

public:
	virtual void Serialize(FArchive& Ar) override;

	virtual void PostLoad() override;

	TSharedPtr<FModelStreamableBulkData> StreamingData;
};

USTRUCT()
struct FMutableParamNameSet
{
	GENERATED_BODY()

	TSet<FString> ParamNames;
};


/** Class containing all UE resources derived from a CO compilation. These resources will be embedded in the CO at cook time but not in the editor.
  * Editor compilations will serialize this class to disk using the Serialize methods. Ensure new fields are serialized, too.
  * Variables and settings that should not change until the CO is re-compiled should be stored here. */
UCLASS(MinimalAPI)
class UModelResources : public UObject
{
	GENERATED_BODY()

public:
	/** 
	 * All the SkeletalMeshes generated for this CustomizableObject instances will use the Reference Skeletal Mesh
	 * properties for everything that Mutable doesn't create or modify. This struct stores the information used from
	 * the Reference Skeletal Meshes to avoid having them loaded at all times. This includes data like LOD distances,
	 * LOD render data settings, Mesh sockets, Bounding volumes, etc.
	 * 
	 * Index with CustomizableObject Component index
	 */
	UPROPERTY()
	TArray<FMutableRefSkeletalMeshData> ReferenceSkeletalMeshesData;	

	/** Skeletons used by the compiled mu::FModel. */
	UPROPERTY()
	TArray<TSoftObjectPtr<USkeleton>> Skeletons;

	/** Materials used by the compiled mu::FModel */
	UPROPERTY()
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	/** PassThrough textures used by the mu::FModel. */
	UPROPERTY()
	TArray<TSoftObjectPtr<UTexture>> PassThroughTextures;

	/** PassThrough meshes used by the mu::FModel. */
	UPROPERTY()
	TArray<TSoftObjectPtr<UStreamableRenderAsset>> PassThroughMeshes;

#if WITH_EDITORONLY_DATA
	/** Runtime referenced textures used by the mu::FModel. */
	UPROPERTY()
	TArray<TSoftObjectPtr<const UTexture>> RuntimeReferencedTextures;
	
	/** Runtime referenced meshes used by the mu::FModel.
	* TODO: Move to FMutableSourceMeshData when runtime is really implemented.
	*/
	UPROPERTY()
	TArray<TSoftObjectPtr<const UStreamableRenderAsset>> RuntimeReferencedMeshes;
#endif
	
	/** Physics assets gathered from the SkeletalMeshes, to be used in mesh generation in-game */
	UPROPERTY()
	TArray<TSoftObjectPtr<UPhysicsAsset>> PhysicsAssets;

	/** UAnimBlueprint assets gathered from the SkeletalMesh, to be used in mesh generation in-game */
	UPROPERTY()
	TArray<TSoftClassPtr<UAnimInstance>> AnimBPs;

	/** */
	UPROPERTY()
	TArray<FAnimBpOverridePhysicsAssetsInfo> AnimBpOverridePhysiscAssetsInfo;

	/** Material slot names for the materials referenced by the surfaces. */
	UPROPERTY()
	TArray<FName> MaterialSlotNames;

	UPROPERTY()
	TMap<FString, uint32> BoneNamesMap;

	/** Mesh sockets provided by the part skeletal meshes, to be merged in the generated meshes */
	UPROPERTY()
	TArray<FMutableRefSocket> SocketArray;

	UPROPERTY()
	TArray<FMutableSkinWeightProfileInfo> SkinWeightProfilesInfo;

	UPROPERTY()
	TArray<FMutableModelImageProperties> ImageProperties;

	UPROPERTY()
	TMap<uint32, FMutableMeshMetadata> MeshMetadata;

	UPROPERTY()
	TMap<uint32, FMutableSurfaceMetadata> SurfaceMetadata;

	/** Parameter UI metadata information for all the dependencies of this Customizable Object. */
	UPROPERTY()
	TMap<FString, FMutableParameterData> ParameterUIDataMap;

	/** State UI metadata information for all the dependencies of this Customizable Object */
	UPROPERTY()
	TMap<FString, FMutableStateData> StateUIDataMap;

#if WITH_EDITORONLY_DATA
	/** DataTable used by an int parameter and its value. */
	UPROPERTY()
	TMap<FIntegerParameterOptionKey, FIntegerParameterOptionDataTable> IntParameterOptionDataTable;
#endif
	
	UPROPERTY()
	TArray<FCustomizableObjectClothConfigData> ClothSharedConfigsData;	

	UPROPERTY()
	TArray<FCustomizableObjectClothingAssetData> ClothingAssetsData;


	/** Currently not used, this option should be selectable from editor maybe as a compilation flag */
	UPROPERTY()
	bool bAllowClothingPhysicsEditsPropagation = true;

#if WITH_EDITORONLY_DATA

	// Stores what param names use a certain table as a table can be used from multiple table nodes, useful for partial compilations to restrict params
	UPROPERTY()
	TMap<FString, FMutableParamNameSet> TableToParamNames;

	/** Map to identify what CustomizableObject owns a parameter. Used to display a tooltip when hovering a parameter
	 * in the Prev. instance panel */
	UPROPERTY()
	TMap<FString, FString> CustomizableObjectPathMap;

	UPROPERTY()
	TMap<FString, FCustomizableObjectIdPair> GroupNodeMap;

	/** If the object is compiled with maximum optimizations. */
	UPROPERTY()
	bool bIsCompiledWithOptimization = true;

	/** This is a non-user-controlled flag to disable streaming (set at object compilation time, depending on optimization). */
	UPROPERTY()
	bool bIsTextureStreamingDisabled = false;

	/** List of external packages that if changed, a compilation is required.
	  * Key is the package name. Value is the the UPackage::Guid, which is regenerated each time the packages is saved.
	  *
	  * Updated each time the CO is compiled and saved in the Derived Data. */
	UPROPERTY()
	TMap<FName, FGuid> ParticipatingObjects;

	UPROPERTY()
	TArray<FCustomizableObjectResourceData> StreamedResourceDataEditor;

	UPROPERTY()
	TArray<FCustomizableObjectResourceData> StreamedExtensionDataEditor;
#endif

	// Constant Resources streamed in on demand when generating meshes
	UPROPERTY()
	TArray<FCustomizableObjectStreamedResourceData> StreamedResourceData;

	// mu::FExtensionData::Index is an index into this array when mu::ExtensionData::Origin is ConstantAlwaysLoaded
	UPROPERTY()
	TArray<FCustomizableObjectResourceData> AlwaysLoadedExtensionData;

	// mu::FExtensionData::Index is an index into this array when mu::ExtensionData::Origin is ConstantStreamed
	UPROPERTY()
	TArray<FCustomizableObjectStreamedResourceData> StreamedExtensionData;
	
	/** Max number of LODs in the compiled Model. */
	UPROPERTY()
	TMap<FName, uint8> NumLODsAvailable;

	/** Max number of LODs to stream. Mutable will always generate at least one LOD. */
	UPROPERTY()
	TMap<FName, uint8> NumLODsToStream;

	/** First LOD available, some platforms may remove lower LODs when cooking, this MinLOD represents the first LOD we can generate */
	UPROPERTY()
	TMap<FName, uint8> FirstLODAvailable;

	/** Name of all possible components. Index is the ObjectComponentIndex. */
	UPROPERTY()
	TArray<FName> ComponentNamesPerObjectComponent;

	/** Minimum LOD to render per Platform. */
	UPROPERTY()
	TMap<FName, FPerPlatformInt> MinLODPerComponent;

	/** Minimum LOD to render per Quality level. */
	UPROPERTY()
	TMap<FName, FPerQualityLevelInt> MinQualityLevelLODPerComponent;
	
	UPROPERTY()
	FString ReleaseVersion;

	UPROPERTY()
	int32 CodeVersion = 0;

#if WITH_EDITORONLY_DATA
	/** Value of the variable TextureCompression in the last compilation of this CO.
		* this is needed since we can compile a CO through blueprints with a different 
		* compilation setting than the stored in the COE.*/
	UPROPERTY()
	bool bCompiledWithHDTextureCompression = false;
	
	CUSTOMIZABLEOBJECT_API void InitCookData(UObject& InCustomizableObject);
#endif
};


UCLASS(MinimalAPI, config = Engine)
class UCustomizableObjectBulk : public UObject
{
public:
	GENERATED_BODY()

	//~ Begin UObject Interface
	CUSTOMIZABLEOBJECT_API virtual void PostLoad() override;
	//~ End UObject Interface

	/**  */
	const FString& GetBulkFilePrefix() const { return BulkFilePrefix; }

	CUSTOMIZABLEOBJECT_API TUniquePtr<IAsyncReadFileHandle> OpenFileAsyncRead(uint32 FileId, uint32 Flags) const;

#if WITH_EDITOR

	//~ Begin UObject Interface
	CUSTOMIZABLEOBJECT_API virtual void CookAdditionalFilesOverride(const TCHAR*, const ITargetPlatform*, TFunctionRef<void(const TCHAR*, void*, int64)>) override;
	//~ End UObject Interface
#endif

#if WITH_EDITOR
private:
#endif

	/** Prefix to locate bulkfiles for loading, using the file ids in each FMutableStreamableBlock. */
	FString BulkFilePrefix;
};


USTRUCT()
struct FMutableMeshComponentData
{
	GENERATED_USTRUCT_BODY()

	/** Name to identify this component. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FName Name;

	/** All the SkeletalMeshes generated for this CustomizableObject instances will use the Reference Skeletal Mesh
	* properties for everything that Mutable doesn't create or modify. This includes data like LOD distances, Physics
	* properties, Bounding Volumes, Skeleton, etc.
	*
	* While a CustomizableObject instance is being created for the first time, and in some situation with lots of
	* objects this may require some seconds, the Reference Skeletal Mesh is used for the actor. This works as a better
	* solution than the alternative of not showing anything, although this can be disabled with the function
	* "SetReplaceDiscardedWithReferenceMeshEnabled" (See the c++ section). */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TObjectPtr<USkeletalMesh> ReferenceSkeletalMesh;
};



/** Strongly typed index for the index of a component in a UCustomizableObject. */
USTRUCT()
struct FCustomizableObjectComponentIndex
{
	GENERATED_BODY()

private:

	int32 Index = 0;

public:

	explicit inline FCustomizableObjectComponentIndex(int32 InIndex=0)
		: Index(InIndex)
	{
	}

	inline void Invalidate()
	{
		Index = INDEX_NONE;
	}

	inline bool IsValid() const
	{
		return Index != INDEX_NONE;
	}

	inline int32 GetValue() const
	{
		return Index;
	}

	inline bool operator==(const FCustomizableObjectComponentIndex&) const = default;
};


#if WITH_EDITORONLY_DATA 
// This is a manual version number for the binary blobs in this asset.
// Increasing it invalidates all the previously compiled models.
UENUM()
enum class ECustomizableObjectVersions : int32
{
	FirstEnumeratedVersion = 450,

	DeterminisiticMeshVertexIds,

	NumRuntimeReferencedTextures,
	
	DeterminisiticLayoutBlockIds,

	BackoutDeterminisiticLayoutBlockIds,

	FixWrappingProjectorLayoutBlockId,

	MeshReferenceSupport,

	ImproveMemoryUsageForStreamableBlocks,

	FixClipMeshWithMeshCrash,

	SkeletalMeshLODSettingsSupport,

	RemoveCustomCurve,

	AddEditorGamePlayTags,

	AddedParameterThumbnailsToEditor,

	ComponentsLODsRedesign,

	ComponentsLODsRedesign2,

	LayoutToPOD,

	AddedRomFlags,

	LayoutNodeCleanup,

	AddSurfaceAndMeshMetadata,

	TablesPropertyNameBug,

	DataTablesParamTrackingForCompileOnlySelected,

	CompilationOptimizationsMeshFormat,

	ModelStreamableBulkData,

	LayoutBlocksAsInt32,
	
	IntParameterOptionDataTable,

	RemoveLODCountLimit,

	IntParameterOptionDataTablePartialBackout,

	IntParameterOptionDataTablePartialRestore,

	CorrectlySerializeTableToParamNames,
	
	AddMaterialSlotNameIndexToSurfaceMetadata,

	NodeComponentMesh,
	
	MoveEditNodesToModifiers,

	DerivedDataCache,

	ComponentsArray,

	FixComponentNames,

	AddedFaceCullStrategyToSomeOperations,

	DDCParticipatingObjects,

	GroupRomsBySource,
	
	RemovedGroupRomsBySource,

	ReGroupRomsBySource,

	UIMetadataGameplayTags,

	TransformInMeshModifier,
	
	SurfaceMetadataSlotNameIndexToName,

	BulkDataFilesNumFilesLimit,

	RemoveModifiersHack,

	SurfaceMetadataSerialized,

	FixesForMeshSectionMultipleOutputs,

	ImageParametersInServerBuilds,

	RemovedUnnecessarySerializationVersioning,

	AddTextureCompressionSettingCompilationInfo,

	RestructureConstantImageData,

	RestructureConstantMeshData,

	RestructureRomData,

	RestructureRomDataRemovingRomHash,

	ModifiedRomCompiledDataSerialization,

	ModelResourcesExtensionData,

	LODsPerComponent,

	LODsPerComponentTypeMismatch,

	ImageHiResLODsUseLODGroupInfo,

	MovedTableRowNoneGenerationToUnreal,

	RemoveObsoletMeshInterpolateAndGeometryOp,

	RemoveObsoleteDataTypesFromEnum,

	ConvertModelResourcesToUObject,

	RemoveObsoletImageGradientOp,

	MeshReferencesExtendedForCompilation,

	RemoveObsoleteBoolOps,

	AddOverlayMaterials,

	PrefetchHighQualityMipMaps,

	AddedMeshParameterOp,

	AddedMeshParameterOpForDDCPollution,

	ExtendedMeshParameterArgumentsWithLODAndSection,
	
	AddAssetUserDataEditor,
	
	MeshDataRomSplit,

	MeshDataRomSplitBackout,

	MovedLODSettingsToMeshComponentNode,
	
	AddedMeshPrepareLayoutOp,

	AddedMeshIDToMeshParamOp,

	ClothMorphMeshMetadata,

	AddedMeshIDToMeshParamOpBackout,

	MeshDataRomSplitSerializationFix,

	ReaddAddedMeshIDToMeshParamOp,

	AddConnectedChildObjectComponentsToPrepass,

	FixMeshReusalDueToLayouts,

	IncorrectBonePoseMerging,

	FixDDCCrashesAndLoadTimes,
	
	// -----<new versions can be added above this line>--------
	LastCustomizableObjectVersion
};
#endif


UCLASS(MinimalAPI)
class UCustomizableObjectPrivate : public UObject
{
	GENERATED_BODY()

	TSharedPtr<mu::FModel, ESPMode::ThreadSafe> MutableModel;

	/** Stores streamable data info to be used by MutableModel In-Game. Cooked resources. */
	UPROPERTY()
	TObjectPtr<UModelStreamableData> ModelStreamableData;

	/** Stores resources to be used by MutableModel In-Game. Cooked resources. */
	UPROPERTY()
	TObjectPtr<UModelResources> ModelResources;

#if WITH_EDITORONLY_DATA
	/** 
	 * Stores resources to be used by MutableModel in the Editor. Editor resources.
	 * Editor-Only to avoid packaging assets referenced by editor compilations. 
	 */
	UPROPERTY(Transient)
	TObjectPtr<UModelResources> ModelResourcesEditor;

	/**
	 * Stores streamable data info to be used by MutableModel in the Editor. Editor streaming.
	 */
	TSharedPtr<FModelStreamableBulkData> ModelStreamableDataEditor;
#endif

public:
	/** Must be called after unlocking the CustomizableObject. */
	CUSTOMIZABLEOBJECT_API void SetModel(const TSharedPtr<mu::FModel, ESPMode::ThreadSafe>& Model, const FGuid Identifier);
	CUSTOMIZABLEOBJECT_API const TSharedPtr<mu::FModel, ESPMode::ThreadSafe>& GetModel();
	CUSTOMIZABLEOBJECT_API const TSharedPtr<const mu::FModel, ESPMode::ThreadSafe> GetModel() const;

	CUSTOMIZABLEOBJECT_API UModelResources* GetModelResources();
	CUSTOMIZABLEOBJECT_API const UModelResources* GetModelResources() const;
	CUSTOMIZABLEOBJECT_API const UModelResources& GetModelResourcesChecked() const;


#if WITH_EDITORONLY_DATA
	CUSTOMIZABLEOBJECT_API UModelResources* GetModelResources(bool bIsCooking);
	CUSTOMIZABLEOBJECT_API const UModelResources* GetModelResources(bool bIsCooking) const;

	CUSTOMIZABLEOBJECT_API void SetModelResources(UModelResources* InModelResources, bool bIsCooking);
	CUSTOMIZABLEOBJECT_API void SetModelStreamableBulkData(const TSharedPtr<FModelStreamableBulkData>& StreamableData, bool bIsCooking);
#endif
	
	CUSTOMIZABLEOBJECT_API TSharedPtr<FModelStreamableBulkData> GetModelStreamableBulkData(bool bIsCooking = false) const;

	// See UCustomizableObjectSystem::LockObject()
	CUSTOMIZABLEOBJECT_API bool IsLocked() const;

	/** Modify the provided mutable parameters so that the forced values for the given customizable object state are applied. */
	CUSTOMIZABLEOBJECT_API void ApplyStateForcedValuesToParameters(FCustomizableObjectInstanceDescriptor& Descriptor);

	CUSTOMIZABLEOBJECT_API int32 FindParameter(const FString& Name) const;
	CUSTOMIZABLEOBJECT_API int32 FindParameterTyped(const FString& Name, EMutableParameterType Type) const;

	CUSTOMIZABLEOBJECT_API EMutableParameterType GetParameterType(int32 ParamIndex) const;

	CUSTOMIZABLEOBJECT_API int32 FindIntParameterValue(int32 ParamIndex, const FString& Value) const;

	CUSTOMIZABLEOBJECT_API FString GetStateName(int32 StateIndex) const;

#if WITH_EDITORONLY_DATA
	CUSTOMIZABLEOBJECT_API void PostCompile();
#endif

	/** Returns a pointer to the BulkData subobject, only valid in packaged builds. */
	CUSTOMIZABLEOBJECT_API const UCustomizableObjectBulk* GetStreamableBulkData() const;

	CUSTOMIZABLEOBJECT_API UCustomizableObject* GetPublic() const;

#if WITH_EDITOR

	/** Compose file name. */
	CUSTOMIZABLEOBJECT_API FString GetCompiledDataFileName(const ITargetPlatform* InTargetPlatform = nullptr, bool bIsDiskStreamer = false) const;

	/** DDC helpers. BuildDerivedDataKey is expensive, try to cache it as much as possible. */
	CUSTOMIZABLEOBJECT_API TArray<uint8> BuildDerivedDataKey(FCompilationOptions Options);
	CUSTOMIZABLEOBJECT_API UE::DerivedData::FCacheKey GetDerivedDataCacheKeyForOptions(FCompilationOptions InOptions);

	/** Log data for debugging purposes */
	CUSTOMIZABLEOBJECT_API void LogMemory();
#endif
	
	/** Rebuild ParameterProperties from the current compiled model. */
	CUSTOMIZABLEOBJECT_API void UpdateParameterPropertiesFromModel(const TSharedPtr<mu::FModel>& Model);

	CUSTOMIZABLEOBJECT_API void AddUncompiledCOWarning(const FString& AdditionalLoggingInfo);

#if WITH_EDITOR
	// Create new GUID for this CO
	CUSTOMIZABLEOBJECT_API void UpdateVersionId();
	
	CUSTOMIZABLEOBJECT_API FGuid GetVersionId() const;

	CUSTOMIZABLEOBJECT_API void SaveEmbeddedData(FArchive& Ar);
	
	// Add a profile that stores the values of the parameters used by the CustomInstance.
	CUSTOMIZABLEOBJECT_API FReply AddNewParameterProfile(FString Name, class UCustomizableObjectInstance& CustomInstance);



	/** Generic Load methods to read compiled data */
	CUSTOMIZABLEOBJECT_API bool LoadModelResources(FArchive& Ar, const ITargetPlatform* InTargetPlatform, bool bSkipEditorOnlyData = false);
	CUSTOMIZABLEOBJECT_API void LoadModelStreamableBulk(FArchive& Ar, bool bIsCooking);
	CUSTOMIZABLEOBJECT_API void LoadModel(FArchive& Ar);

	/** Load compiled data for the running platform from disk, this is used to load Editor Compilations. */
	CUSTOMIZABLEOBJECT_API void LoadCompiledDataFromDisk();
	
	/** Loads data previously compiled in BeginCacheForCookedPlatformData onto the UProperties in *this,
	  * in preparation for saving the cooked package for *this or for a CustomizableObjectInstance using *this.
      * Returns whether the data was successfully loaded. */
	CUSTOMIZABLEOBJECT_API bool TryLoadCompiledCookDataForPlatform(const ITargetPlatform* TargetPlatform);
#endif

	// Data that may be stored in the asset itself, only in packaged builds.
	CUSTOMIZABLEOBJECT_API void LoadEmbeddedData(FArchive& Ar);
	
	/** Compute bIsChildObject if currently possible to do so. Return whether it was computed. */
	CUSTOMIZABLEOBJECT_API bool TryUpdateIsChildObject();

	CUSTOMIZABLEOBJECT_API void SetIsChildObject(bool bIsChildObject);

	/** Return the names used by mutable to identify which mu::FImage should be considered of LowPriority. */
	CUSTOMIZABLEOBJECT_API void GetLowPriorityTextureNames(TArray<FString>& OutTextureNames);

	/** Return the MinLOD index to generate based on the active LODSettings (PerPlatformMinLOD or PerQualityLevelMinLOD) */
	CUSTOMIZABLEOBJECT_API uint8 GetMinLODIndex(const FName& ComponentName) const;

#if WITH_EDITOR
	/** See ICustomizableObjectEditorModule::IsCompilationOutOfDate. */
	CUSTOMIZABLEOBJECT_API bool IsCompilationOutOfDate(bool bSkipIndirectReferences, TArray<FName>& OutOfDatePackages, TArray<FName>& AddedPackages, TArray<FName>& RemovedPackages, bool& bReleaseVersionDiff) const;
#endif

	CUSTOMIZABLEOBJECT_API TArray<FString>& GetCustomizableObjectClassTags();
	
	CUSTOMIZABLEOBJECT_API TArray<FString>& GetPopulationClassTags();

    CUSTOMIZABLEOBJECT_API TMap<FString, FParameterTags>& GetCustomizableObjectParametersTags();

#if WITH_EDITORONLY_DATA
	CUSTOMIZABLEOBJECT_API TArray<FProfileParameterDat>& GetInstancePropertiesProfiles();
#endif

#if WITH_EDITORONLY_DATA
	CUSTOMIZABLEOBJECT_API TObjectPtr<UEdGraph>& GetSource() const;

	CUSTOMIZABLEOBJECT_API FCompilationOptions GetCompileOptions() const;
#endif

	CUSTOMIZABLEOBJECT_API void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion);

	CUSTOMIZABLEOBJECT_API FName GetComponentName(FCustomizableObjectComponentIndex ObjectComponentIndex) const;

#if WITH_EDITORONLY_DATA
	CUSTOMIZABLEOBJECT_API EMutableCompileMeshType GetMeshCompileType() const;
	
	CUSTOMIZABLEOBJECT_API const TArray<TSoftObjectPtr<UCustomizableObject>>& GetWorkingSet() const;

	CUSTOMIZABLEOBJECT_API bool IsAssetUserDataMergeEnabled() const;
	
	CUSTOMIZABLEOBJECT_API bool IsTableMaterialsParentCheckDisabled() const;

	CUSTOMIZABLEOBJECT_API bool IsRealTimeMorphTargetsEnabled() const;

	CUSTOMIZABLEOBJECT_API bool IsClothingEnabled() const;
	
	CUSTOMIZABLEOBJECT_API bool Is16BitBoneWeightsEnabled() const;
	
	CUSTOMIZABLEOBJECT_API bool IsAltSkinWeightProfilesEnabled() const;
	
	CUSTOMIZABLEOBJECT_API bool IsPhysicsAssetMergeEnabled() const;
	
	CUSTOMIZABLEOBJECT_API bool IsEnabledAnimBpPhysicsAssetsManipulation() const;
#endif
	
	CUSTOMIZABLEOBJECT_API const FString& GetIntParameterAvailableOption(int32 ParamIndex, int32 K) const;

	CUSTOMIZABLEOBJECT_API int32 GetEnumParameterNumValues(int32 ParamIndex) const;

	CUSTOMIZABLEOBJECT_API FString FindIntParameterValueName(int32 ParamIndex, int32 ParamValue) const;

	CUSTOMIZABLEOBJECT_API int32 FindState(const FString& Name) const;

	CUSTOMIZABLEOBJECT_API int32 GetStateParameterIndex(int32 StateIndex, int32 ParameterIndex) const;

	CUSTOMIZABLEOBJECT_API bool IsParameterMultidimensional(const int32& InParamIndex) const;
	
	/** Cache of generated SkeletalMeshes */
	FMeshCache MeshCache;

	/** Cache of merged Skeletons */
	FSkeletonCache SkeletonCache;

	// See UCustomizableObjectSystem::LockObject. Must only be modified from the game thread
	bool bLocked = false;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	TArray<FMutableMeshComponentData> MutableMeshComponents_DEPRECATED;

	/** Unique Identifier - Deterministic. Used to locate Model and Streamable data on disk. Should not be modified. */
	FGuid Identifier;
	
	ECompilationResultPrivate CompilationResult = ECompilationResultPrivate::Unknown;
	
	FPostCompileDelegate PostCompileDelegate;

	/** Map of PlatformName to CachedPlatformData. Only valid while cooking. */
	TMap<FString, MutablePrivate::FMutableCachedPlatformData> CachedPlatformsData;

#endif

	FCustomizableObjectStatus Status;

	// This is information about the parameters in the model that is generated at model compile time.
	UPROPERTY(Transient)
	TArray<FMutableModelParameterProperties> ParameterProperties;

	/** Reference to all UObject used in game. Only updated during the compilation if the user explicitly wants to save all references. */
	UPROPERTY()
	TObjectPtr<UModelResources> ReferencedObjects;
	
	// Map of name to index of ParameterProperties.
	// use this to lookup fast by Name
	TMap<FString, FMutableParameterIndex> ParameterPropertiesLookupTable;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	ECustomizableObjectTextureCompression TextureCompression = ECustomizableObjectTextureCompression::Fast;

	/** From 0 to UE_MUTABLE_MAX_OPTIMIZATION */
	UPROPERTY()
	int32 OptimizationLevel = UE_MUTABLE_MAX_OPTIMIZATION;

	/** Use the disk to store intermediate compilation data. This slows down the object compilation
	 * but it may be necessary for huge objects. */
	UPROPERTY()
	bool bUseDiskCompilation = false;

	/** High limit of the size in bytes of the packaged data when cooking this object.
	 * This limit is before any pak or filesystem compression. This limit will be broken if a single piece of data is bigger because data is not fragmented for packaging purposes.	*/
	UPROPERTY()
	uint64 PackagedDataBytesLimit = 256 * 1024 * 1024;
	
	/** High (inclusive) limit of the size in bytes of a data block to be included into the compiled object directly instead of stored in a streamable file. */
	UPROPERTY()
	uint64 EmbeddedDataBytesLimit = 1024;

	UPROPERTY()
	int32 ImageTiling = 0;

#endif
	
	static constexpr int32 DerivedDataVersion = 0x44c37a2c;
};

#if WITH_EDITOR

/** Returns the DDC ValueId of the file owning a streamable resource.
	* @ param StreamableDataType - UE level data type
	* @ param ResourceType - mu::EDataType
	*/
CUSTOMIZABLEOBJECT_API UE::DerivedData::FValueId GetDerivedDataValueIdForResource(MutablePrivate::EStreamableDataType StreamableDataType, uint32 FileId, uint16 ResourceType, uint16 Flags);

// Compose folder name where the data is stored
CUSTOMIZABLEOBJECT_API FString GetCompiledDataFolderPath();
CUSTOMIZABLEOBJECT_API FString GetDataTypeExtension(MutablePrivate::EStreamableDataType DataType);

CUSTOMIZABLEOBJECT_API uint32 GetECustomizableObjectVersionEnumHash();

CUSTOMIZABLEOBJECT_API TObjectPtr<UModelResources> LoadModelResources_Internal(FArchive& Ar, const UCustomizableObject* Outer, const ITargetPlatform* InTargetPlatform, bool bSkipEditorOnlyData = false);
CUSTOMIZABLEOBJECT_API const TSharedPtr<FModelStreamableBulkData> LoadModelStreamableBulk_Internal(FArchive& Ar);
CUSTOMIZABLEOBJECT_API const TSharedPtr<mu::FModel, ESPMode::ThreadSafe> LoadModel_Internal(FArchive& Ar);

#endif