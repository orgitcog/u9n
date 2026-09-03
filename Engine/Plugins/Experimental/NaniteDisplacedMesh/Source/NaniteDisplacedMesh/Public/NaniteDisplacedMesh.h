// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/Interface_AsyncCompilation.h"
#include "RenderCommandFence.h"
#include "StaticMeshResources.h"
#include "IO/IoHash.h"
#include "Rendering/NaniteResources.h"

#include "NaniteDisplacedMesh.generated.h"

#define UE_API NANITEDISPLACEDMESH_API

class FQueuedThreadPool;
enum class EQueuedWorkPriority : uint8;
struct FPropertyChangedEvent;

class FNaniteBuildAsyncCacheTask;
class UNaniteDisplacedMesh;
class UTexture;

USTRUCT(BlueprintType)
struct FNaniteDisplacedMeshDisplacementMap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Displacement)
	TObjectPtr<class UTexture2D> Texture;

	UPROPERTY(EditAnywhere, Category = Displacement)
	float Magnitude;

	UPROPERTY(EditAnywhere, Category = Displacement)
	float Center;

	FNaniteDisplacedMeshDisplacementMap()
		: Texture(nullptr)
		, Magnitude(0.0f)
		, Center(0.0f)
	{}

	bool operator==(const FNaniteDisplacedMeshDisplacementMap& Other) const
	{
		return Texture		== Other.Texture
			&& Magnitude	== Other.Magnitude
			&& Center		== Other.Center;
	}

	bool IsEquivalent(const FNaniteDisplacedMeshDisplacementMap& Other) const
	{
		if (GenerateDisplacement())
		{
			return *this == Other;
		}

		return !Other.GenerateDisplacement();
	}

	bool GenerateDisplacement() const
	{
		return Texture && Magnitude > 0.f;
	}

	bool operator!=(const FNaniteDisplacedMeshDisplacementMap& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct FNaniteDisplacedMeshParams
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category = Mesh)
	TObjectPtr<class UStaticMesh> BaseMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	float RelativeError;

	UPROPERTY(EditAnywhere, Category = Texture)
	TArray<FNaniteDisplacedMeshDisplacementMap> DisplacementMaps;

	/** Default settings. */
	FNaniteDisplacedMeshParams()
		: BaseMesh(nullptr)
		, RelativeError(0.03f)
	{}

	/** Equality operator. */
	bool operator==(const FNaniteDisplacedMeshParams& Other) const
	{
		if (BaseMesh != Other.BaseMesh ||
			RelativeError != Other.RelativeError ||
			DisplacementMaps.Num() != Other.DisplacementMaps.Num())
		{
			return false;
		}

		for( int32 i = 0; i < DisplacementMaps.Num(); i++ )
		{
			if (DisplacementMaps[i] != Other.DisplacementMaps[i])
			{
				return false;
			}
		}

		return true;
	}

	bool IsEquivalent(const FNaniteDisplacedMeshParams& Other) const
	{
		if (BaseMesh != Other.BaseMesh ||
			RelativeError != Other.RelativeError ||
			DisplacementMaps.Num() != Other.DisplacementMaps.Num())
		{
			return false;
		}

		for (int32 i = 0; i < DisplacementMaps.Num(); i++)
		{
			if (!DisplacementMaps[i].IsEquivalent(Other.DisplacementMaps[i]))
			{
				return false;
			}
		}

		return true;
	}

	/** Inequality operator. */
	bool operator!=(const FNaniteDisplacedMeshParams& Other) const
	{
		return !(*this == Other);
	}

	UE_API void ClearInvalidAssetsForDisplacement();

	/*
	 * Does the settings would result in the creation of some rendering data
	 */
	UE_API bool IsDisplacementRequired() const;

	static UE_API bool CanUseAssetForDisplacement(const UObject* InAsset);

#endif // WITH_EDITORONLY_DATA
};

#if WITH_EDITOR && WITH_EDITORONLY_DATA
struct FValidatedNaniteDisplacedMeshParams
{
	FValidatedNaniteDisplacedMeshParams(const FNaniteDisplacedMeshParams& InParams)
		: ValidatedParams(InParams)
	{
		ValidatedParams.ClearInvalidAssetsForDisplacement();
	}

	FValidatedNaniteDisplacedMeshParams(FNaniteDisplacedMeshParams&& InParams)
		: ValidatedParams(MoveTemp(InParams))
	{
		ValidatedParams.ClearInvalidAssetsForDisplacement();
	}

	/*
	 * Does the settings would result in the creation of some rendering data
	 */
	UE_API bool IsDisplacementRequired() const;

	operator const FNaniteDisplacedMeshParams&() const
	{
		return ValidatedParams;
	}

	// Consume the validated nanite displaced mesh params
	FNaniteDisplacedMeshParams ConvertToNaniteDisplacedMeshParams()
	{
		return MoveTemp(ValidatedParams);
	}

private:
	FNaniteDisplacedMeshParams ValidatedParams;
};

#endif //WITH_EDITOR && WITH_EDITORONLY_DATA

struct FNaniteData
{
	TPimplPtr<Nanite::FResources> ResourcesPtr;

	// Material section information that matches displaced mesh.
	Nanite::FMeshDataSectionArray MeshSections;
};

UCLASS(MinimalAPI)
class UNaniteDisplacedMesh : public UObject, public IInterface_AsyncCompilation
{
	GENERATED_BODY()

	friend class FNaniteBuildAsyncCacheTask;

public:
	UE_API UNaniteDisplacedMesh(const FObjectInitializer& Init);

	UE_API virtual void Serialize(FArchive& Ar) override;
	UE_API virtual void PostLoad() override;
	UE_API virtual void BeginDestroy() override;
	UE_API virtual bool IsReadyForFinishDestroy() override;
	UE_API virtual bool NeedsLoadForTargetPlatform(const ITargetPlatform* TargetPlatform) const override;

	UE_API void InitResources();
	UE_API void ReleaseResources();

	UE_API bool HasValidNaniteData() const;

	inline Nanite::FResources* GetNaniteData()
	{
		return Data.ResourcesPtr.Get();
	}

	inline const Nanite::FResources* GetNaniteData() const
	{
		return Data.ResourcesPtr.Get();
	}

	inline const Nanite::FMeshDataSectionArray& GetMeshSections() const
	{
		return Data.MeshSections;
	}

#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	UE_API virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	UE_API virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	UE_API virtual void ClearAllCachedCookedPlatformData() override;

	/** Returns whether or not the asset is currently being compiled */
	UE_API bool IsCompiling() const override;

	/** Try to cancel any pending async tasks.
	 *  Returns true if there is no more async tasks pending, false otherwise.
	 */
	UE_API bool TryCancelAsyncTasks();

	/** Returns false if there is currently an async task running */
	UE_API bool IsAsyncTaskComplete() const;

	/**
	* Wait until all async tasks are complete, up to a time limit
	* Returns true if all tasks are completed
	**/
	UE_API bool WaitForAsyncTasks(float TimeLimitSeconds);

	/** Make sure all async tasks are completed before returning */
	UE_API void FinishAsyncTasks();

private:
	friend class FNaniteDisplacedMeshCompilingManager;
	UE_API void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority);
#endif

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta=(EditCondition = "bIsEditable"))
	FNaniteDisplacedMeshParams Parameters;

	/**
	 * Was this asset created by a procedural tool?
	 * This flag is generally set by tool that created the asset.
	 * It's used to tell the users that they shouldn't modify the asset by themselves.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Asset, AdvancedDisplay)
	bool bIsEditable = true;
#endif

private:
	bool bIsInitialized = false;

	// Data used to render this displaced mesh with Nanite.
	FNaniteData Data;

	FRenderCommandFence ReleaseResourcesFence;

#if WITH_EDITOR
	UE_API FIoHash CreateDerivedDataKeyHash(const ITargetPlatform* TargetPlatform);
	UE_API FIoHash BeginCacheDerivedData(const ITargetPlatform* TargetPlatform);
	UE_API bool PollCacheDerivedData(const FIoHash& KeyHash) const;
	UE_API void EndCacheDerivedData(const FIoHash& KeyHash);

	/** Synchronously cache and return derived data for the target platform. */
	UE_API FNaniteData& CacheDerivedData(const ITargetPlatform* TargetPlatform);

	FIoHash DataKeyHash;
	TMap<FIoHash, TUniquePtr<FNaniteData>> DataByPlatformKeyHash;
	TMap<FIoHash, TPimplPtr<FNaniteBuildAsyncCacheTask>> CacheTasksByKeyHash;

	DECLARE_MULTICAST_DELEGATE(FOnNaniteDisplacedMeshRenderingDataChanged);
	FOnNaniteDisplacedMeshRenderingDataChanged OnRenderingDataChanged;
#endif

public:
#if WITH_EDITOR
	typedef FOnNaniteDisplacedMeshRenderingDataChanged::FDelegate FOnRebuild;
	UE_API FDelegateHandle RegisterOnRenderingDataChanged(const FOnRebuild& Delegate);
	UE_API void UnregisterOnRenderingDataChanged(FDelegateUserObject Unregister);
	UE_API void UnregisterOnRenderingDataChanged(FDelegateHandle Handle);

	UE_API void NotifyOnRenderingDataChanged();
#endif

private:
	friend class UGeneratedNaniteDisplacedMeshEditorSubsystem;
#if WITH_EDITOR
	DECLARE_EVENT_OneParam(UNaniteDisplacedMesh, FOnNaniteDisplacmentMeshDependenciesChanged, UNaniteDisplacedMesh*);
	static UE_API FOnNaniteDisplacmentMeshDependenciesChanged OnDependenciesChanged;
#endif
};

#undef UE_API
