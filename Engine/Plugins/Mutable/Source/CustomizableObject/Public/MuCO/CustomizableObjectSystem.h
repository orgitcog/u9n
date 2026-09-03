// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define UE_MUTABLE_MAX_OPTIMIZATION			2

#include "HAL/IConsoleManager.h"
#include "Engine/StreamableManager.h"
#include "AssetRegistry/AssetData.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuR/Parameters.h"
#include "MuR/Image.h"
#include "MuR/Types.h"

#include "CustomizableObjectSystem.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class IConsoleVariable;
class UCustomizableInstanceLODManagementBase;
class ITargetPlatform;
class UCustomizableObject;
class UDefaultImageProvider;
class USkeletalMesh;
class UMaterialInterface;
class UTexture2D;
class UCustomizableObjectSystemPrivate; // This is used to hide Mutable SDK members in the public headers.
class FUpdateContextPrivate;
struct FFrame;
struct FGuid;
struct FEditorCompileSettings;


extern TAutoConsoleVariable<bool> CVarClearWorkingMemoryOnUpdateEnd;

extern TAutoConsoleVariable<bool> CVarReuseImagesBetweenInstances;

extern TAutoConsoleVariable<bool> CVarPreserveUserLODsOnFirstGeneration;

extern TAutoConsoleVariable<bool> CVarEnableMeshCache;

extern TAutoConsoleVariable<bool> CVarEnableRealTimeMorphTargets;

extern TAutoConsoleVariable<bool> CVarRollbackFixModelDiskStreamerDataRace;

extern TAutoConsoleVariable<bool> CVarEnableReleaseMeshResources;

extern TAutoConsoleVariable<bool> CVarFixLowPriorityTasksOverlap;


UENUM(BlueprintType)
enum class ECustomizableObjectOptimizationLevel : uint8
{
	None = 0, 
	Minimal UE_DEPRECATED(5.6, "Converted internally to None. Use None instead.") = 1,
	Maximum = 2,
	FromCustomizableObject  = 3					// Grab the optimization settings from the CO
};


UENUM(BlueprintType)
enum class ECustomizableObjectTextureCompression : uint8
{
	// Don't use texture compression
	None = 0,
	// Use Mutable's fast low-quality compression
	Fast,
	// Use Unreal's highest quality compression (100x slower to compress)
	HighQuality
};


UENUM()
enum class ECustomizableObjectNumBoneInfluences : uint8
{
	// The enum values can be used as the real numeric value of number of bone influences
	Four = 4,
	// 
	Eight = 8,
	//
	Twelve = 12 // This is essentially the same as "Unlimited", but UE ultimately limits to 12
};


namespace EMutableProfileMetric
{
	typedef uint8 Type;

	constexpr Type BuiltInstances = 1;
	constexpr Type UpdateOperations = 2;
	constexpr Type Count = 4;

};


USTRUCT()
struct FPendingReleaseMaterialsInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY()
	int32 TicksUntilRelease = 0;
};



/** Key to identify an image inside a generated mutable runtime instance. */
struct FMutableImageReference
{
	/** Original image ID. Once generated it will be unique. However, future updates of the image may return a different ID for the
	* same image, if many other resources have been built in the middle. For this reason the rest of the data in the struct is what
	* must be used to request the additional mips.	*/
	uint32 ImageID = 0;

	uint32 SurfaceId = 0;

	uint8 LOD = 0;
	
	/** Object component index. */
	uint8 Component = 0;

	uint8 Image = 0;

	uint8 BaseMip = 0;

	TArray<int32> ConstantImagesNeededToGenerate;
};



// Opaque representation of a possible registered value for texture parameters.
struct FCustomizableObjectExternalTexture
{
	FCustomizableObjectExternalTexture() = default;

	FString Name;
	FName Value;
};


/** Base class for Image provider.
 * 
 *  An image provider only needs to be alive when calling UpdateSkeletalMeshAsync. Once called it can be safety destructed.
 *  Mutable will cache those images in case they are needed due to MIP updates. */
UCLASS(MinimalAPI, abstract)
class UCustomizableSystemImageProvider : public UObject
{
public:
	GENERATED_BODY()

	enum class ValueType : uint8
	{
		// This texture is not provided by this provider.
		None,

		// Data will be provided with size and pointer
		Raw,

		// Data will be provided from an unreal texture, loaded in the game thread and kept in memory
		Unreal,

		// Data will be provided from an unreal texture, and will only be loaded when actually needed in the Mutable thread
		Unreal_Deferred,

		// Number of elements of this enum.
		Count
	};

	// Query that Mutable will run to find out if a texture will be provided as an Unreal UTexture2D,
	// or as a raw data blob.
	virtual ValueType HasTextureParameterValue(const FName& ID) { return ValueType::None; }

	// In case IsTextureParameterValueUnreal returns false, this will be used to query the texture size data.
	virtual FIntVector GetTextureParameterValueSize(const FName& ID) { return FIntVector(0, 0, 0); }

	// In case IsTextureParameterValueUnreal returns false, this will be used to query the texture data that must
	// be copied in the preallocated buffer. The pixel format is assumed to be 4-channel RGBA, uint8_t per channel.
	virtual void GetTextureParameterValueData(const FName& ID, uint8* OutData) {}

	// In case IsTextureParameterValueUnreal returns true, this will be used to query the texture.
	virtual UTexture2D* GetTextureParameterValue(const FName& ID) { return nullptr; }

	// Used in the editor to show the list of available options.
	// Only necessary if the images are required in editor previews.
	virtual void GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues) {}
};


UCLASS(MinimalAPI, BlueprintType)
class UCustomizableObjectSystem : public UObject
{
	// Friends
	friend class UCustomizableObjectSystemPrivate;

public:
	GENERATED_BODY()

	UCustomizableObjectSystem() = default;
	UE_API void InitSystem();

	/** Get the singleton object. It will be created if it doesn't exist yet. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Status)
	static UE_API UCustomizableObjectSystem* GetInstance();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Status)
	static UE_API UCustomizableObjectSystem* GetInstanceChecked();

	/** Determines if the result of the instance update is valid or not.
	 * @return true if the result is successful or has warnings, false if the result is from the Error category */
	UFUNCTION(BlueprintCallable, Category = Status)
	static UE_API bool IsUpdateResultValid(const EUpdateResult UpdateResult);
	
	// Return true if the singleton has been created. It is different than GetInstance in that GetInstance will create it if it doesn't exist.
	static UE_API bool IsCreated();

	/** Returns the current status of Mutable. Only when active is it possible to compile COs, generate instances, and stream textures.
	  * @return True if Mutable is enabled. */
	static UE_API bool IsActive();

	// Begin UObject interface.
	UE_API virtual void BeginDestroy() override;
	UE_API virtual FString GetDesc() override;
	// End UObject interface.

	UE_API bool IsReplaceDiscardedWithReferenceMeshEnabled() const;
	UE_API void SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled);

#if WITH_EDITOR
	// Lock a CustomizableObjects, preventing the generation or update of any of its instances
	// Will return true if successful, false if it fails to lock because an update is already underway
	// This is usually only used in the editor
	UE_API bool LockObject(UCustomizableObject*);
	UE_API void UnlockObject(UCustomizableObject*);

	/** Checks if there are any outstanding disk or mip update operations in flight for the parameter Customizable Object that may
	* make it unsafe to compile at the moment.
	* @return true if there are operations in flight and it's not safe to compile */
	UE_API bool CheckIfDiskOrMipUpdateOperationsPending(const UCustomizableObject& Object) const;
	
	// Called whenever the Mutable Editor Settings change, copying the new value of the current needed settings to the Customizable Object System
	UE_API void EditorSettingsChanged(const FEditorCompileSettings& InEditorSettings);

	// If true, uncompiled Customizable Objects will be compiled whenever an instance update is required
	UE_API bool IsAutoCompileEnabled() const;

	/** Return true if inside commandlets uncompiled Customizable Objects will be compiled whenever an instance update is required. */
	UE_API bool IsAutoCompileCommandletEnabled() const;

	/** Set if inside commandlets uncompiled Customizable Objects will be compiled whenever an instance update is required. */
	UE_API void SetAutoCompileCommandletEnabled(bool bValue);
	
	// If true, uncompiled Customizable Objects will be compiled synchronously
	UE_API bool IsAutoCompilationSync() const;
#endif
	
	// Return the current MinLodQualityLevel for skeletal meshes.
	UE_API int32 GetSkeletalMeshMinLODQualityLevel() const;

	UE_API bool IsSupport16BitBoneIndexEnabled() const;

	UE_API bool IsProgressiveMipStreamingEnabled() const;
	UE_API void SetProgressiveMipStreamingEnabled(bool bIsEnabled);

	UE_API bool IsOnlyGenerateRequestedLODsEnabled() const;
	UE_API void SetOnlyGenerateRequestedLODsEnabled(bool bIsEnabled);

#if WITH_EDITOR
	UE_API void SetImagePixelFormatOverride(const mu::FImageOperator::FImagePixelFormatFunc&);
#endif

	/** [Texture FParameters] Get a list of all the possible values for external texture parameters according to the various providers registered with RegisterImageProvider. */
	UE_API TArray<FCustomizableObjectExternalTexture> GetTextureParameterValues();

	/** [Texture FParameters] Add a new image provider to the CustomizableObject System. This will be queried for when an external image ID is provided to mutable in an Texture Parameter node. */
	UE_API void RegisterImageProvider(UCustomizableSystemImageProvider* Provider);

	/** [Texture FParameters] Remove a previously registered provider. */
	UE_API void UnregisterImageProvider(UCustomizableSystemImageProvider* Provider);

	/** [Texture FParameters] Interface to actually cache Images in the Mutable system and make them available at run-time.
		Any cached image has to be registered by an Image provider before caching it.
		Have in mind that once an image has been cached, it will spend memory according to its size, except in the case
		of images of type UCustomizableSystemImageProvider::ValueType::Unreal_Deferred, where only a very small amount of
		memory is used and the real texel data is loaded when needed during an update and then immediately discarded */

	/** [Texture FParameters] Cache an image which has to have been previously registered by an Image provider with the parameter id. */
	UE_API void CacheImage(FName ImageId);
	/** [Texture FParameters] Remove an image from the cache. */
	UE_API void UnCacheImage(FName ImageId);
	/** [Texture FParameters] Remove all images from the cache. */
	UE_API void ClearImageCache();

	// Show a warning on-screen and via a notification (if in Editor) and log an error when a CustomizableObject is
	// being used and it's not compiled.  Callers can add additional information to the error log.
	UE_API void AddUncompiledCOWarning(const UCustomizableObject& InObject, FString const* OptionalLogInfo = nullptr);

	// Give access to the internal object data.
	UE_API UCustomizableObjectSystemPrivate* GetPrivate();
	UE_API const UCustomizableObjectSystemPrivate* GetPrivate() const;

	UE_API UCustomizableInstanceLODManagementBase* GetInstanceLODManagement() const;

	// Pass a null ptr to reset to the default InstanceLODManagement
	UE_API void SetInstanceLODManagement(UCustomizableInstanceLODManagementBase* NewInstanceLODManagement);

	// Find out the version of the plugin
	UFUNCTION(BlueprintCallable, Category = Status)
	UE_API FString GetPluginVersion() const;

	// Get the number of instances built and alive.
	UFUNCTION(BlueprintCallable, Category = Status)
	UE_API int32 GetNumInstances() const;

	// Get the number of instances waiting to be updated.
	UFUNCTION(BlueprintCallable, Category = Status)
	UE_API int32 GetNumPendingInstances() const;

	// Get the total number of instances including built and not built.
	UFUNCTION(BlueprintCallable, Category = Status)
	UE_API int32 GetTotalInstances() const;

	// Get the amount of GPU memory in use in bytes for textures generated by mutable.
	UFUNCTION(BlueprintCallable, Category = Status)
	UE_API int64 GetTextureMemoryUsed() const;

	// Return the average build/update time of an instance in ms.
	UFUNCTION(BlueprintCallable, Category = Status)
	UE_API int32 GetAverageBuildTime() const;

	// If set to true, Mutable will release Mutable-generated textures immediately when they are no longer used without waiting for GC
	// IMPORTANT!!! Do NOT keep references to any Mutable generated textures or skeletal meshes if this is enabled,
	// they are owned by Mutable and will be destroyed without notice
	UFUNCTION(BlueprintCallable, Category = Status)
	UE_API void SetReleaseMutableTexturesImmediately(bool bReleaseTextures);

	UE_API bool IsMutableAnimInfoDebuggingEnabled() const;

#if WITH_EDITOR
	// Get the maximum size a chunk can have on a specific platform. If unspecified return MUTABLE_STREAMED_DATA_MAXCHUNKSIZE
	UE_API uint64 GetMaxChunkSizeForPlatform(const ITargetPlatform* InTargetPlatform);
#endif

private:
	// Most of the work in this plugin happens here.
	UE_API bool Tick(float DeltaTime);

	/** Returns the number of remaining operations. */
	UE_API int32 TickInternal(bool bBlocking);
	
	UE_API void DiscardInstances();
	UE_API void ReleaseInstanceIDs();

public:
	/** Return true if the instance is being updated. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CustomizableObjectSystem)
	UE_API bool IsUpdating(const UCustomizableObjectInstance* Instance) const;

	/** Set Mutable's working memory limit (kilobytes). Mutable will flush internal caches to try to keep its memory consumption below the WorkingMemory (i.e., it is not a hard limit).
	 * The working memory limit will especially reduce the memory required to perform Instance Updates and Texture Streaming.
 	 * Notice that Mutable does not track all its memory (e.g., UObjects memory is no tracked).
	 * This value can also be set using "mutable.WorkingMemory" CVar. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectSystem)
	UE_API void SetWorkingMemory(int32 KiloBytes);

	/** Get Mutable's working memory limit (kilobytes). See SetWorkingMemory(int32). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CustomizableObjectSystem)
	UE_API int32 GetWorkingMemory() const;

	/**
	 * Get if the mutable mesh cache for the instance meshes is enabled or not.
	 * @param bCheckCVarOnGameThread Force the checking of the relevant CVar to use the GameThread as target thread
	 * @return True if it is enabled and false otherwise
	 */
	static UE_API bool IsMeshCacheEnabled(bool bCheckCVarOnGameThread = false);

	/**
	 * Get if mutable should clear its working memory between instance updates.
	 * @return True if the clearing of memory will be performed, false otherwise.
	 */
	static UE_API bool ShouldClearWorkingMemoryOnUpdateEnd();

	/**
	 * Get if mutable will try to reuse textures in between instances.
	 * @return True if mutable will try to reuse them, false otherwise.
	 */
	static UE_API bool ShouldReuseTexturesBetweenInstances();
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectSystemPrivate> Private = nullptr;


public:
	
	/**
	 * Enables the collection of internal Mutable performance data. It has a performance cost.
	 */
	UE_API void EnableBenchmark();

	/**
	 * Disables the reporting of mutable instance benchmarking data.
	 */
	UE_API void EndBenchmark();
	
};

#undef UE_API
