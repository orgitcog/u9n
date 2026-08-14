// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LogBenchmarkUtil.h"
#include "Containers/Queue.h"
#include "MuCO/DescriptorHash.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "MuCO/CustomizableInstanceLODManagement.h"
#include "Containers/Ticker.h"

#include "MuCO/CustomizableObjectInstanceDescriptor.h"
#include "MuR/Mesh.h"
#include "MuR/Parameters.h"
#include "MuR/System.h"
#include "MuR/Skeleton.h"
#include "MuR/Image.h"
#include "UObject/GCObject.h"
#include "WorldCollision.h"
#include "Engine/StreamableManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MuCO/FMutableTaskGraph.h"
#include "AssetRegistry/AssetData.h"
#include "ContentStreaming.h"
#include "Animation/MorphTarget.h"
#include "MuCO/MutableStreamableManager.h"
#include "UObject/StrongObjectPtr.h"

#include "Tasks/Task.h"

#include "CustomizableObjectSystemPrivate.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class FMutableStreamRequest;
class UEditorImageProvider;
class UCustomizableObjectSystem;
namespace LowLevelTasks { enum class ETaskPriority : int8; }
struct FTexturePlatformData;

// Split StreamedBulkData into chunks smaller than MUTABLE_STREAMED_DATA_MAXCHUNKSIZE
#define MUTABLE_STREAMED_DATA_MAXCHUNKSIZE		(512 * 1024 * 1024)


extern TAutoConsoleVariable<bool> CVarMutableHighPriorityLoading;


/** Serialized Morph data. */
struct FMorphTargetMeshData
{
	/** Names of the Morph Targets. */
	TArray<FName> NameResolutionMap;

	/** Vertex data. Each vertex contains an index to the NameResolutionMap. */
	TArray<FMorphTargetVertexData> Data;
};


/** Serialized Cloth data. */
struct FClothingMeshData
{
	int32 ClothingAssetIndex = INDEX_NONE;
	int32 ClothingAssetLOD = INDEX_NONE;

	/** Per vertex data. */
	TArray<FCustomizableObjectMeshToMeshVertData> Data;
};


/** Mapping of FMorphTargetVertexData local names to global names. */
struct FMappedMorphTargetMeshData
{
	/** Index is the local name. Value is the index to a global name table. */
	TArray<int32> NameResolutionMap;

	/** Pointer to the original data with indices to local names. */
	const TArray<FMorphTargetVertexData>* DataView = nullptr;
};


/* Reconstruct the final Morph Targets using the global names.
 * @param Mesh Used to know which MappedMorphTargets vertices must be removed.
 * @param GlobalNames
 * @param OutMorphTargets resulting Morph Targets with the indices pointing to GlobalNames. */
void ReconstructMorphTargets(const mu::FMesh& Mesh, const TArray<FName>& GlobalNames, const TMap<uint32, FMappedMorphTargetMeshData>& MappedMorphTargets, TArray<FMorphTargetLODModel>& OutMorphTargets);


/** Request the Mutable Data Streamer to load Morph Target blocks. Does not stream them.
 *  @param MutableDataStreamer Streamer to request the blocks.
 *  @param Mesh Mutable mesh to obtain which blocks to load.
 *  @param StreamingResult Key is the block. Value is the container where the data will be deserialized once streamed. */
void LoadMorphTargetsData(FMutableStreamRequest& MutableDataStreamer, const TSharedRef<const mu::FMesh>& Mesh, TMap<uint32, FMorphTargetMeshData>& StreamingResult);

void LoadMorphTargetsMetadata(const FMutableStreamRequest& MutableDataStreamer, const TSharedRef<const mu::FMesh>& Mesh, TMap<uint32, FMorphTargetMeshData>& StreamingResult);


/** Request the Mutable Data Streamer to load Cloth blocks. Does not stream them.
 *  @param MutableDataStreamer Streamer to request the blocks.
 *  @param Mesh Mutable mesh to obtain which blocks to load.
 *  @param StreamingResult Key is the block. Value is the container where the data will be deserialized once streamed. */
void LoadClothing(FMutableStreamRequest& MutableDataStreamer, const TSharedRef<const mu::FMesh>& Mesh, TMap<uint32, FClothingMeshData>& StreamingResult);


/** End a Customizable Object Instance Update. All code paths of an update have to end here. */
void FinishUpdateGlobal(const TSharedRef<FUpdateContextPrivate>& Context);


#if WITH_EDITOR

/**
 * Class used to hold some MutableSystem settings to be used during the update of a given instance.
 * It will store the settings used by the system during the setup of this object and so later can be reverted back.
 */
class FMutableSystemSettingsOverrides
{
public:
	UE_API FMutableSystemSettingsOverrides(bool bUseProgressiveMipStreaming, bool bOnlyGenerateRequestedLODs, mu::FImageOperator::FImagePixelFormatFunc InImagePixelFormatFunc);
	
	/** Apply the settings set in this object to the mutable system. */
	UE_API void ApplySettingsOverrides() const;

	/** Restore the mutable system settings to a state prior to the setup of this object */
	UE_API void RestoreSettings() const;


private:
	
	// Requested values
	bool bIsProgressiveMipStreamingEnabled = false;
	bool bIsOnlyGenerateRequestedLODsEnabled = false;
	mu::FImageOperator::FImagePixelFormatFunc ImagePixelFormatFunc;
	
	// previous values. Cached at the time of creating this object
	bool bOldIsProgressiveMipStreamingEnabled = false;
	bool bOldIsOnlyGenerateRequestedLODsEnabled = false;
	mu::FImageOperator::FImagePixelFormatFunc OldImagePixelFormatFunc;
};

#endif


/** Strongly typed index for the index of a component in a UCustomizableObjectInstance. */
class FCustomizableObjectInstanceComponentIndex
{
private:

	int32 Index = 0;

public:

	explicit FCustomizableObjectInstanceComponentIndex(int32 InIndex = 0)
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
};


class FMutableUpdateCandidate
{
public:
	// The Instance to possibly update
	UCustomizableObjectInstance* CustomizableObjectInstance;
	EQueuePriorityType Priority = EQueuePriorityType::Med;

	// These are the LODs that would be applied if this candidate is chosen
	TMap<FName, uint8> MinLOD;

	// These are the LODs that would be copied to the descriptor to trigger mesh updates on quality setting changes.
	TMap<FName, uint8> QualitySettingMinLODs;

	TMap<FName, uint8> FirstRequestedLOD;

	FMutableUpdateCandidate(UCustomizableObjectInstance* InCustomizableObjectInstance);
	
	FMutableUpdateCandidate(const UCustomizableObjectInstance* InCustomizableObjectInstance, const TMap<FName, uint8>& InMinLOD,
		const TMap<FName, uint8>& InFirstRequestedLOD) :
		CustomizableObjectInstance(const_cast<UCustomizableObjectInstance*>(InCustomizableObjectInstance)),
		MinLOD(InMinLOD),
		FirstRequestedLOD(InFirstRequestedLOD) {}

	bool HasBeenIssued() const;

	void Issue();

	void ApplyLODUpdateParamsToInstance(FUpdateContextPrivate& Context);

private:
	/** If true it means that EnqueueUpdateSkeletalMesh has decided this update should be performed, if false it should be ignored. Just used for consistency checks */
	bool bHasBeenIssued = false;
};


struct FMutablePendingInstanceUpdate
{
	TSharedRef<FUpdateContextPrivate> Context;

	FMutablePendingInstanceUpdate(const TSharedRef<FUpdateContextPrivate>& InContext);

	bool operator==(const FMutablePendingInstanceUpdate& Other) const;

	bool operator<(const FMutablePendingInstanceUpdate& Other) const;
};


inline uint32 GetTypeHash(const FMutablePendingInstanceUpdate& Update);


struct FPendingInstanceUpdateKeyFuncs : BaseKeyFuncs<FMutablePendingInstanceUpdate, TWeakObjectPtr<const UCustomizableObjectInstance>>
{
	FORCEINLINE static TWeakObjectPtr<const UCustomizableObjectInstance> GetSetKey(const FMutablePendingInstanceUpdate& PendingUpdate);

	FORCEINLINE static bool Matches(const TWeakObjectPtr<const UCustomizableObjectInstance>& A, const TWeakObjectPtr<const UCustomizableObjectInstance>& B);

	FORCEINLINE static uint32 GetKeyHash(const TWeakObjectPtr<const UCustomizableObjectInstance>& Identifier);
};


struct FMutablePendingInstanceDiscard
{
	TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	FMutablePendingInstanceDiscard(UCustomizableObjectInstance* InCustomizableObjectInstance)
	{
		CustomizableObjectInstance = InCustomizableObjectInstance;
	}

	friend bool operator ==(const FMutablePendingInstanceDiscard& A, const FMutablePendingInstanceDiscard& B)
	{
		return A.CustomizableObjectInstance.HasSameIndexAndSerialNumber(B.CustomizableObjectInstance);
	}
};


inline uint32 GetTypeHash(const FMutablePendingInstanceDiscard& Discard)
{
	return GetTypeHash(Discard.CustomizableObjectInstance.GetWeakPtrTypeHash());
}


struct FPendingInstanceDiscardKeyFuncs : BaseKeyFuncs<FMutablePendingInstanceUpdate, TWeakObjectPtr<UCustomizableObjectInstance>>
{
	FORCEINLINE static const TWeakObjectPtr<UCustomizableObjectInstance>& GetSetKey(const FMutablePendingInstanceDiscard& PendingDiscard)
	{
		return PendingDiscard.CustomizableObjectInstance;
	}

	FORCEINLINE static bool Matches(const TWeakObjectPtr<UCustomizableObjectInstance>& A, const TWeakObjectPtr<UCustomizableObjectInstance>& B)
	{
		return A.HasSameIndexAndSerialNumber(B);
	}

	FORCEINLINE static uint32 GetKeyHash(const TWeakObjectPtr<UCustomizableObjectInstance>& Identifier)
	{
		return GetTypeHash(Identifier.GetWeakPtrTypeHash());
	}
};


/** Instance updates queue.
 *
 * The queues will only contain a single operation per UCustomizableObjectInstance.
 * If there is already an operation it will be replaced. */
class FMutablePendingInstanceWork
{
	TSet<FMutablePendingInstanceUpdate, FPendingInstanceUpdateKeyFuncs> PendingInstanceUpdates;

	TSet<FMutablePendingInstanceDiscard, FPendingInstanceDiscardKeyFuncs> PendingInstanceDiscards;

	TSet<mu::FInstance::FID> PendingIDsToRelease;

public:
	// Returns the number of pending instance updates, LOD Updates and discards.
	int32 Num() const;
	
	// Adds a new instance update
	void AddUpdate(const FMutablePendingInstanceUpdate& UpdateToAdd);

	// Removes an instance update
	void RemoveUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance);

#if WITH_EDITOR
	void RemoveUpdatesForObject(const UCustomizableObject* InObject);
#endif

	const FMutablePendingInstanceUpdate* GetUpdate(const TWeakObjectPtr<const UCustomizableObjectInstance>& Instance) const;

	TSet<FMutablePendingInstanceUpdate, FPendingInstanceUpdateKeyFuncs>::TIterator GetUpdateIterator()
	{
		return PendingInstanceUpdates.CreateIterator();
	}

	TSet<FMutablePendingInstanceDiscard, FPendingInstanceDiscardKeyFuncs>::TIterator GetDiscardIterator()
	{
		return PendingInstanceDiscards.CreateIterator();
	}

	TSet<mu::FInstance::FID>::TIterator GetIDsToReleaseIterator()
	{
		return PendingIDsToRelease.CreateIterator();
	}

	void AddDiscard(const FMutablePendingInstanceDiscard& TaskToEnqueue);
	void AddIDRelease(mu::FInstance::FID IDToRelease);

	void RemoveAllUpdatesAndDiscardsAndReleases();
};


struct FMutableImageCacheKey
{
	mu::FResourceID Resource = 0;
	int32 SkippedMips = 0;

	FMutableImageCacheKey() {};

	FMutableImageCacheKey(mu::FResourceID InResource, int32 InSkippedMips)
		: Resource(InResource), SkippedMips(InSkippedMips) {}

	inline bool operator==(const FMutableImageCacheKey& Other) const
	{
		return Resource == Other.Resource && SkippedMips == Other.SkippedMips;
	}
};


inline uint32 GetTypeHash(const FMutableImageCacheKey& Key)
{
	return HashCombine(GetTypeHash(Key.Resource), GetTypeHash(Key.SkippedMips));
}


// Cache of weak references to generated resources for one single model
struct FMutableResourceCache
{
	TWeakObjectPtr<const UCustomizableObject> Object;
	TMap<mu::FResourceID, TWeakObjectPtr<USkeletalMesh> > Meshes;
	TMap<FMutableImageCacheKey, TWeakObjectPtr<UTexture2D> > Images;

	void Clear()
	{
		Meshes.Reset();
		Images.Reset();
	}
};


USTRUCT()
struct FGeneratedTexture
{
	GENERATED_USTRUCT_BODY();

	FMutableImageCacheKey Key;

	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	FString Name;

	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	TObjectPtr<UTexture> Texture = nullptr;

	bool operator==(const FGeneratedTexture& Other) const = default;
};


USTRUCT()
struct FGeneratedMaterial
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	TObjectPtr<UMaterialInterface> MaterialInterface;

	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	TArray< FGeneratedTexture > Textures;

	// Surface or SharedSurface Id
	uint32 SurfaceId = 0;

	// Index of the material to instantiate (UCustomizableObject::ReferencedMaterials)
	uint32 MaterialIndex = 0;

#if WITH_EDITORONLY_DATA
	// Name of the component that contains this material
	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	FName ComponentName;
#endif

	bool operator==(const FGeneratedMaterial& Other) const { return SurfaceId == Other.SurfaceId && MaterialIndex == Other.MaterialIndex; };
};


/** Final data per component. */
struct FSkeletalMeshMorphTargets
{
	/** Name of the Morph Target. */
	TArray<FName> RealTimeMorphTargetNames;

	/** First index is the Morph Target (in sync with RealTimeMorphTargetNames). Second index is the LOD. */
	TArray<TArray<FMorphTargetLODModel>> RealTimeMorphsLODData; 
};


// Mutable data generated during the update steps.
// We keep it from begin to end update, and it is used in several steps.
struct FInstanceUpdateData
{
	struct FImage
	{
		FName Name;
		mu::FResourceID ImageID;
		
		// LOD of the ImageId. If the texture is shared between LOD, first LOD where this image can be found. 
		int32 BaseLOD;
		int32 BaseMip;
		
		uint16 FullImageSizeX, FullImageSizeY;
		TSharedPtr<const mu::FImage> Image;
		TWeakObjectPtr<UTexture2D> Cached;

		TArray<int32> ConstantImagesNeededToGenerate;

		bool bIsPassThrough = false;
		bool bIsNonProgressive = false;
	};

	struct FVector
	{
		FName Name;
		FLinearColor Vector;
	};

	struct FScalar
	{
		FName Name;
		float Scalar;
	};

	struct FSurface
	{
		/** Range in the Images array */
		uint16 FirstImage = 0;
		uint16 ImageCount = 0;

		/** Range in the Vectors array */
		uint16 FirstVector = 0;
		uint16 VectorCount = 0;

		/** Range in the Scalar array */
		uint16 FirstScalar = 0;
		uint16 ScalarCount = 0;

		/** Index of the material in the referenced materials array of the CO.
		* A negative value means that the material of this surface slot of the mesh doesn't need to be changed.
		* This is valid for pass-through meshes.
		*/
		int32 MaterialIndex = -1;

		/** Id of the surface in the mutable core instance. */
		uint32 SurfaceId = 0;

		/** Id of the metadata associated with this surface. */ 
		uint32 SurfaceMetadataId = 0;
	};

	struct FLOD
	{		
		mu::FResourceID MeshID = TNumericLimits<mu::FResourceID>::Max();
		TSharedPtr<const mu::FMesh> Mesh;

		/** Range in the Surfaces array */
		uint16 FirstSurface = 0;
		uint16 SurfaceCount = 0;

		/** Range in the external Bones array */
		uint32 FirstActiveBone = 0;
		uint32 ActiveBoneCount = 0;

		/** Range in the external Bones array */
		uint32 FirstBoneMap = 0;
		uint32 BoneMapCount = 0;
	};

	struct FComponent
	{
		FCustomizableObjectComponentIndex Id = FCustomizableObjectComponentIndex(0);
		
		/** Range in the LODs array */
		uint16 FirstLOD = 0;
		uint16 LODCount = 0;

		/** Overlay material. Only one per mesh, if any. */
		int32 OverlayMaterial = INDEX_NONE;
	};

	TArray<FComponent> Components;
	TArray<FLOD> LODs;
	TArray<FSurface> Surfaces;
	TArray<FImage> Images;
	TArray<FVector> Vectors;
	TArray<FScalar> Scalars;

	TArray<mu::FBoneName> ActiveBones;
	TArray<mu::FBoneName> BoneMaps;
	
	struct FBone
	{
		mu::FBoneName Name;
		FMatrix44f MatrixWithScale;

		bool operator==(const mu::FBoneName& OtherName) const { return Name == OtherName; };
	};
	
	/** Key is the Block Id. Value is the LoadAdditionalAssetsAndData read destination. */
	TMap<uint32, FMorphTargetMeshData> RealTimeMorphTargetMeshData;
	
	/** Key is the Component Name. Value is the final Morph Target data to copy into the Skeletal Mesh. */
	TMap<FName, FSkeletalMeshMorphTargets> RealTimeMorphTargets;

	/** Key is the Block Id. Value is the LoadAdditionalAssetsAndData read destination. */
	TMap<uint32, FClothingMeshData> ClothingMeshData;

	struct FSkeletonData
	{
		TArray<uint16> SkeletonIds;

		TArray<FBone> BonePose;

		TMap<mu::FBoneName, TPair<FName, uint16>> BoneInfoMap;
	};

	// Access by instance component index
	TArray<FSkeletonData> SkeletonsPerInstanceComponent;

	struct FNamedExtensionData
	{
		TSharedPtr<const mu::FExtensionData> Data;
		FName Name;
	};
	TArray<FNamedExtensionData> ExtendedInputPins;

	/** */
	void Clear()
	{
		LODs.Empty();
		Components.Empty();
		Surfaces.Empty();
		Images.Empty();
		Scalars.Empty();
		Vectors.Empty();
		RealTimeMorphTargets.Empty();
		SkeletonsPerInstanceComponent.Empty();
		ExtendedInputPins.Empty();
	}
};


/** Update Context.
 *
 * Alive from the start to the end of the update (both API and LOD update). */
class FUpdateContextPrivate
{
public:
	FUpdateContextPrivate(UCustomizableObjectInstance& InInstance, const FCustomizableObjectInstanceDescriptor& Descriptor);

	FUpdateContextPrivate(UCustomizableObjectInstance& InInstance);

	~FUpdateContextPrivate();

	bool IsContextValid() const;
	
	void SetMinLOD(const TMap<FName, uint8>& MinLOD);

	/** Return an array of LODs per object component. */
	const TMap<FName, uint8>& GetFirstRequestedLOD() const;
	
	void SetFirstRequestedLOD(const TMap<FName, uint8>& RequestedLODs);

	void SetQualitySettingMinLODs(const TMap<FName, uint8>& FirstLODs);

	const FCustomizableObjectInstanceDescriptor& GetCapturedDescriptor() const;

	const FDescriptorHash& GetCapturedDescriptorHash() const;

	const FCustomizableObjectInstanceDescriptor&& MoveCommittedDescriptor();
	
	EQueuePriorityType PriorityType = EQueuePriorityType::Low;
	
	FInstanceUpdateDelegate UpdateCallback;
	FInstanceUpdateNativeDelegate UpdateNativeCallback;

	/** Weak reference to the instance we are operating on.
	 *It is weak because we don't want to lock it in case it becomes irrelevant in the game while operations are pending and it needs to be destroyed. */
	TWeakObjectPtr<UCustomizableObjectInstance> Instance;

	/** Customizable Object we are operating on. It can be destroyed between Game Thread tasks. */
	TWeakObjectPtr<UCustomizableObject> Object;

	/** Return the object component index associated with a component in this instance. */
	FCustomizableObjectComponentIndex GetObjectComponentIndex(FCustomizableObjectInstanceComponentIndex InstanceComponentIndex);
	
private:
	/** Descriptor which the update will be performed on. */
	FCustomizableObjectInstanceDescriptor CapturedDescriptor;

	/** Hash of the descriptor. */
	FDescriptorHash CapturedDescriptorHash;

public:
	/** Instance parameters at the time of the operation request. */
	TSharedPtr<mu::FParameters> Parameters; 
	TSharedPtr<mu::FSystem> MutableSystem;

	bool bOnlyUpdateIfNotGenerated = false;
	bool bIgnoreCloseDist = false;
	bool bForceHighPriority = false;
	
	FInstanceUpdateData InstanceUpdateData;
	TArray<int32> RelevantParametersInProgress;

	const FInstanceUpdateData::FComponent* GetComponentUpdateData(FCustomizableObjectInstanceComponentIndex InstanceComponentIndex);

	TArray<FString> LowPriorityTextures;

	/** This option comes from the operation request */
	bool bNeverStream = false;
	
	/** When this option is enabled it will reuse the Mutable core instance and its temp data between updates.  */
	bool bLiveUpdateMode = false;
	bool bReuseInstanceTextures = false;
	bool bUseMeshCache = false;
	
	/** Whether the mesh to generate should support Mesh LOD streaming or not. */
	bool bStreamMeshLODs = false;

	/** true if the Update has blocked Low Priority Tasks from launching. */
	bool bLowPriorityTasksBlocked = false;

	/** The Context has been successfully created. */
	bool bValid = false; 

	/** This option comes from the operation request. It is used to reduce the number of mipmaps that mutable must generate for images.  */
	int32 MipsToSkip = 0;

	mu::FInstance::FID InstanceID = 0; // Redundant
	TSharedPtr<const mu::FInstance> MutableInstance;

	TSharedPtr<mu::FModel> Model;

	// Number of possible components in the entire CO
	// TODO: Redundant because it is in the CO
	uint8 NumObjectComponents = 0;

	// Number of components in the instance being generated
	// TODO: Redundant to store it here because it is implicit in the size of some arrays or in the MutableInstance while it is valid.
	uint8 NumInstanceComponents = 0;

	/** List of component names. Index is the ObjectComponentIndex. */
	TArray<FName> ComponentNames;

	/** Index of the resource in the StreamedResourceData array of the Model Resources. */
	TArray<int32> StreamedResourceIndex;

	/** Index of the resource in the ExtensionStreamedResourceData array of the Model Resources. */
	TArray<int32> ExtensionStreamedResourceIndex;
	
	TMap<FName, uint8> NumLODsAvailable;

	/** Copy of UModelResources::ComponentFirstLODAvailable. First compiled LOD per component for the running platform. Constant. */
	TMap<FName, uint8> FirstLODAvailable;
	
	TMap<FName, uint8> FirstResidentLOD;

	TMap<mu::FResourceID, FTexturePlatformData*> ImageToPlatformDataMap;

	EUpdateResult UpdateResult = EUpdateResult::Success;

	mu::FImageOperator::FImagePixelFormatFunc PixelFormatOverride;

private:

	/** Mutable Meshes required for each component. Outermost index is the object component index, inner index is the LOD. */
	TArray<TArray<mu::FResourceID>> MeshDescriptors;

public:

	void InitMeshDescriptors(int32 Size);
	const TArray<TArray<mu::FResourceID>>& GetMeshDescriptors() const;
	TArray<mu::FResourceID>* GetMeshDescriptors(FCustomizableObjectComponentIndex Index);

	/** Used to know if the updated instances' meshes are different from the previous ones. 
	  * The index of the array is the instance component's index.
	  * @return true if the mesh is new or new to this instance (e.g. mesh cached by another instance). */
	TArray<bool> MeshChangedPerInstanceComponent;
	
	bool UpdateStarted = false;
	bool bLevelBegunPlay = false;

	/** true if the update has been optimized (skips all Tasks and calls FinishUpdateGlobal directly on the Enqueue). */
	bool bOptimizedUpdate = false;
	
	// Update stats
	double StartQueueTime = 0.0;
	double QueueTime = 0.0;
	
	double StartUpdateTime = 0.0;
	double UpdateTime = 0.0;

	double TaskGetMeshTime = 0.0;
	double TaskLockCacheTime = 0.0;
	double TaskGetImagesTime = 0.0;
	double TaskConvertResourcesTime = 0.0f;
	double TaskCallbacksTime = 0.0;

	// Update Memory stats
	int64 UpdateStartBytes = 0;
	int64 UpdateEndPeakBytes = 0;
	int64 UpdateEndRealPeakBytes = 0;

	/** If a InstanceUsage is in this set it means that its AttachParent has been modified (USkeletalMesh changed, UMaterial changed...). */
	TSet<TWeakObjectPtr<UCustomizableObjectInstanceUsage>> AttachedParentUpdated;
	
	/** Hard references to objects. Avoids GC to collect them. */
	TArray<TStrongObjectPtr<const UObject>> Objects;

#if WITH_EDITORONLY_DATA
	TSharedPtr<class FMutableSystemSettingsOverrides> UpdateSettingsOverride = nullptr;
#endif
};


/** Runtime data used during a mutable instance update */
struct FMutableReleasePlatformOperationData
{
	TMap<mu::FResourceID, FTexturePlatformData*> ImageToPlatformDataMap;
};


USTRUCT()
struct FPendingReleaseSkeletalMeshInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY()
	double TimeStamp = 0.0f;
};


#if WITH_EDITORONLY_DATA

UENUM()
enum class ECustomizableObjectDDCPolicy : uint8
{
	None = 0,
	Local,
	Default
};


// Struct used to keep a copy of the EditorSettings needed to compile Customizable Objects.
struct FEditorCompileSettings
{
	// General case
	bool bIsMutableEnabled = true;

	// Auto Compile 
	bool bEnableAutomaticCompilation = true;
	bool bCompileObjectsSynchronously = true;
	bool bCompileRootObjectsOnStartPIE = false;

	// DDC settings
	ECustomizableObjectDDCPolicy EditorDerivedDataCachePolicy = ECustomizableObjectDDCPolicy::Default;
	ECustomizableObjectDDCPolicy CookDerivedDataCachePolicy = ECustomizableObjectDDCPolicy::Default;
};

#endif

/** Private part, hidden from outside the plugin.
 *
 * UE STREAMING:
 * 
 * [- NumLODsAvailable -----------------------------] = 8 (State and platform dependent)
 * [- Stripped --[- Packaged -----------------------]
 * 0      1      2      3      4      5      6      7    
 * |------|------|------|------|------|------|------|
 *               [- Streaming --------[- Residents -]
 *               ^                    ^
 *               |                    |
 *               FirstLODAvailable    FirstResidentLOD
 *                                    FirstRequestedLOD (LODs generated by Core)
 * 
 * [- NumLODsToStream ----------------] = 5 (Compiled constant, ModelResources)
 *
 *
 * HACKY MUTABLE STREAMING:
 * 
 * [- NumLODsAvailable -----------------------------] = 8
 * [- Stripped --[- Packaged -----------------------]
 * 0      1      2      3      4      5      6      7    
 * |------|------|------|------|------|------|------|
 *               [- Residents ----------------------]
 *               [------] Data copied from FirstRequestedLODs. Hacky Mutable LOD Streaming.
 *               ^      ^         
 *               |      |
 *               |      FirstRequestedLOD (LODs generated by Core)
 *               |      
 *               FirstLODAvailable (Compilation constant)
 *               FirstResidentLOD
 *               MinLOD
 * 
 * NumLODsToStream = 0
 *
 *
 * DEFINITIONS:
 * 
 * QualitySettingMinLODs =	MinLOD based on the active quality settings.													COI Descriptor.
 * MinLOD =					From user. Artificial limit.											                     	Skeletal Mesh Component.
 * FirstLODAvailable =	    First available lod per platform.																Skeletal Mesh Component.
 * FirstResidentLOD =       First LOD generated with geometry.																Skeletal Mesh Component.
 * FirstRequestedLOD =		From user. Usually from USkeletalMeshComponent::GetPredictedLODLevel							Skeletal Mesh Component. 
 */
UCLASS()
class UCustomizableObjectSystemPrivate : public UObject, public IStreamingManager
{
	GENERATED_BODY()
	
public:
	// Singleton for the unreal mutable system.
	static UCustomizableObjectSystem* SSystem;

	// Pointer to the lower level mutable system that actually does the work.
	TSharedPtr<mu::FSystem> MutableSystem;

	/** Store the last streaming memory size in bytes, to change it when it is safe. */
	uint64 LastWorkingMemoryBytes = 0;
	uint32 LastGeneratedResourceCacheSize = 0;

	// This object is responsible for streaming data to the MutableSystem.
	TSharedPtr<class FUnrealMutableModelBulkReader> Streamer;
	
	// This object is responsible for providing custom images and meshes to mutable (for image parameters, etc.)
	// This object is called from the mutable thread, and it should only access data already safely submitted from
	// the game thread and stored in FUnrealMutableImageProvider::GlobalExternalImages.
	TSharedPtr<class FUnrealMutableResourceProvider> ResourceProvider;

	// Cache of weak references to generated resources to see if they can be reused.
	TArray<FMutableResourceCache> ModelResourcesCache;

	// List of textures currently cached and valid for the current object that we are operating on.
	// This array gets generated when the object cached resources are protected in SetResourceCacheProtected
	// from the game thread, and it is read from the Mutable thread only while updating the instance.
	TArray<mu::FResourceID> ProtectedObjectCachedImages;

	// The pending instance updates, discards or releases
	FMutablePendingInstanceWork MutablePendingInstanceWork;
	
	static int32 EnableMutableProgressiveMipStreaming;
	static int32 EnableMutableLiveUpdate;
	static int32 EnableReuseInstanceTextures;
	static int32 EnableMutableAnimInfoDebugging;
	static int32 EnableSkipGenerateResidentMips;
	static int32 EnableOnlyGenerateRequestedLODs;
	static int32 MaxTextureSizeToGenerate;
	static int32 SkeletalMeshMinLodQualityLevel;

#if WITH_EDITOR
	mu::FImageOperator::FImagePixelFormatFunc ImageFormatOverrideFunc;
#endif

	// IStreamingManager interface
	virtual void UpdateResourceStreaming(float DeltaTime, bool bProcessEverything = false) override;
	virtual int32 BlockTillAllRequestsFinished(float TimeLimit = 0.0f, bool bLogResults = false) override;
	virtual void CancelForcedResources() override {}
	virtual void AddLevel(ULevel* Level) override {}
	virtual void RemoveLevel(ULevel* Level) override {}
	virtual void NotifyLevelChange() override {}
	virtual void SetDisregardWorldResourcesForFrames(int32 NumFrames) override {}
	virtual void NotifyLevelOffset(ULevel* Level, const FVector& Offset) override {}

	UCustomizableObjectSystem* GetPublic() const;
	
	// Remove references to cached objects that have been deleted in the unreal
	// side, and cannot be cached anyway.
	// This should only happen in the game thread
	void CleanupCache();

	// This should only happen in the game thread
	FMutableResourceCache& GetObjectCache(const UCustomizableObject* Object);

	void AddTextureReference(const FMutableImageCacheKey& TextureId);

	// Returns true if the texture's references become zero
	bool RemoveTextureReference(const FMutableImageCacheKey& TextureId);

	bool TextureHasReferences(const FMutableImageCacheKey& TextureId) const;

	EUpdateRequired IsUpdateRequired(const UCustomizableObjectInstance& Instance, bool bOnlyUpdateIfNotGenerated, bool bOnlyUpdateIfLOD, bool bIgnoreCloseDist) const;

	EQueuePriorityType GetUpdatePriority(const UCustomizableObjectInstance& Instance, bool bForceHighPriority) const;

	void EnqueueUpdateSkeletalMesh(const TSharedRef<FUpdateContextPrivate>& Context);
		
	// Init an async and safe release of the UE and Mutable resources used by the instance without actually destroying the instance, for example if it's very far away
	void InitDiscardResourcesSkeletalMesh(UCustomizableObjectInstance* InCustomizableObjectInstance);

	// Init the async release of a Mutable Core Instance ID and all the temp resources associated with it
	void InitInstanceIDRelease(mu::FInstance::FID IDToRelease);

	void GetMipStreamingConfig(const UCustomizableObjectInstance& Instance, bool& bOutNeverStream, int32& OutMipsToSkip) const;
	
	bool IsReplaceDiscardedWithReferenceMeshEnabled() const;
	void SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled);

	/** Updated at the beginning of each tick. */
	int32 GetNumSkeletalMeshes() const;

	bool bReplaceDiscardedWithReferenceMesh = false;
	bool bReleaseTexturesImmediately = false;

	bool bSupport16BitBoneIndex = false;

	TMap<FMutableImageCacheKey, uint32> TextureReferenceCount; // Keeps a count of texture usage to decide if they have to be blocked from GC during an update

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectInstance> CurrentInstanceBeingUpdated = nullptr;

	TSharedPtr<FUpdateContextPrivate> CurrentMutableOperation = nullptr;

	// Handle to the registered TickDelegate.
	FTSTicker::FDelegateHandle TickWarningsDelegateHandle;

	/** Change the current status of Mutable. Enabling/Disabling core features.	
	 * Disabling Mutable will turn off compilation, generation, and streaming and will remove the system ticker. */
	static void OnMutableEnabledChanged(IConsoleVariable* CVar = nullptr);

	/** Update the last set amount of internal memory Mutable can use to build objects. */
	void UpdateMemoryLimit();

	bool IsMutableAnimInfoDebuggingEnabled() const;

	FUnrealMutableResourceProvider* GetResourceProviderChecked() const;

	/** Start the actual work of Update Skeletal Mesh process (Update Skeletal Mesh without the queue). */
	void StartUpdateSkeletalMesh(const TSharedRef<FUpdateContextPrivate>& Context);

	/** See UCustomizableObjectInstance::IsUpdating. */
	bool IsUpdating(const UCustomizableObjectInstance& Instance) const;

	/** Update stats at each tick.
	 * Used for stats that are costly to update. */
	void UpdateStats();

	void CacheTextureParameters(const TArray<FCustomizableObjectAssetParameterValue>& TextureParameters) const;

	void UnCacheTextureParameters(const TArray<FCustomizableObjectAssetParameterValue>& TextureParameters) const;

#if WITH_EDITOR
	/** Add a CO to the pending - load list.COs need to wait until all related objects are fully loaded before being able
	 * to do things like check-if-up-to-date or compile.
	 */
	void AddPendingLoad(UCustomizableObject*);

#endif

	// Unprotect the resources in the instances of this object of being garbage collector
	// while an instance is being built or updated, so that they can be reused.
	void ClearResourceCacheProtected();
	
	/** Mutable TaskGraph system (Mutable Thread). */
	FMutableTaskGraph MutableTaskGraph;

	/** Last Mutable task from the previous update. The next update can not start until this task has has completed. */
	UE::Tasks::FTask LastUpdateMutableTask = UE::Tasks::MakeCompletedTask<void>();
	
#if WITH_EDITORONLY_DATA
	/** Mutable default image provider. Used by the COIEditor and Instance/Descriptor APIs. */
	UPROPERTY(Transient)
	TObjectPtr<UEditorImageProvider> EditorImageProvider = nullptr;

	/** List of CustomizableObjects pending to complete loading. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCustomizableObject>> ObjectsPendingLoad;

#endif

	FLogBenchmarkUtil LogBenchmarkUtil;

	int32 NumSkeletalMeshes = 0;

	bool bAutoCompileCommandletEnabled = false;

	UPROPERTY()
	TArray<FPendingReleaseSkeletalMeshInfo> PendingReleaseSkeletalMesh;
	
	UPROPERTY(Transient)
	TObjectPtr<UCustomizableInstanceLODManagementBase> DefaultInstanceLODManagement = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableInstanceLODManagementBase> CurrentInstanceLODManagement = nullptr;

	// Array where textures are added temporarily while the mutable thread may want to
	// reused them for some instance under construction.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> ProtectedCachedTextures;
	
	TSharedRef<FMutableStreamableManager> StreamableManager = MakeShared<FMutableStreamableManager>();

#if WITH_EDITOR
	// Copy of the Mutable Editor Settings tied to CO compilation. They are updated whenever changed
	FEditorCompileSettings EditorSettings;
#endif

#if WITH_EDITORONLY_DATA
	// Array to keep track of cached objects
	TArray<FGuid> UncompiledCustomizableObjectIds;

	/** Weak pointer to the Uncompiled Customizable Objects notification */
	TWeakPtr<SNotificationItem> UncompiledCustomizableObjectsNotificationPtr;

	/** Map used to cache per platform MaxChunkSize. If MaxChunkSize > 0, streamed data will be split in multiple files */
	TMap<FString, int64> PlatformMaxChunkSize;
#endif

	int32 NumLODUpdatesLastTick = 0;

	/** Time when the "Started Update Skeletal Mesh Async" log will be unmuted (in seconds). */
	float LogStartedUpdateUnmute = 0.0;

	/** Time of the last "Started Update Skeletal Mesh Async" log (in seconds). */
	float LogStartedUpdateLast = 0;

public:
	/**
	 * Get to know if the settings used by the mutable syustem are optimzed for benchmarking operations or not
	 * @return true if using benchmarking optimized settings, false otherwise.
	 */
	static bool IsUsingBenchmarkingSettings();

	/**
	 * Enable or disable the usage of benchmarking optimized settings
	 * @param bUseBenchmarkingOptimizedSettings True to enable the usage of benchmarking settings, false to disable it.
	 */
	CUSTOMIZABLEOBJECT_API static void SetUsageOfBenchmarkingSettings(bool bUseBenchmarkingOptimizedSettings);

private:

	/** Flag that controls some of the settings used for the generation of instances. */
	inline static bool bUseBenchmarkingSettings = false;
};


namespace impl
{
	void CreateMutableInstance(const TSharedRef<FUpdateContextPrivate>& Operation);

	void FixLODs(const TSharedRef<FUpdateContextPrivate>& Operation);
	
	void Subtask_Mutable_PrepareSkeletonData(const TSharedRef<FUpdateContextPrivate>& OperationData);

	void Subtask_Mutable_UpdateParameterRelevancy(const TSharedRef<FUpdateContextPrivate>& OperationData);
	
	void Subtask_Mutable_PrepareTextures(const TSharedRef<FUpdateContextPrivate>& OperationData);
}


/** Set OnlyLOD to -1 to generate all mips */
CUSTOMIZABLEOBJECT_API FTexturePlatformData* MutableCreateImagePlatformData(TSharedPtr<const mu::FImage> MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY);


/** Return true if Streaming is enabled for the given Object. */
bool IsStreamingEnabled(const UCustomizableObject& Object);

#undef UE_API
