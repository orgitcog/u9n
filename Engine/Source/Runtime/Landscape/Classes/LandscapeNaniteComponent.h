// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Components/StaticMeshComponent.h"
#include "LandscapeDataAccess.h"
#include "StaticMeshAttributes.h"

#include "LandscapeNaniteComponent.generated.h"

class ALandscape;
class ALandscapeProxy;
class ULandscapeComponent;
class FPrimitiveSceneProxy;
class ULandscapeSubsystem;
struct FStaticMeshSourceModel;
struct FLandscapeComponentDataInterfaceBase;

namespace UE::Landscape::Nanite
{
	// Copy of Component level data required to generate Nanite asynchronously
	struct FAsyncComponentData
	{
		TArray<FColor> HeightAndNormalData;
		TArray<uint8> Visibility;
		TSharedPtr<FLandscapeComponentDataInterfaceBase> ComponentDataInterface;
		int32 Stride = 0;
	};

	// Context for an Async Static Mesh (nanite) build
	// Also serving double duty as input parameter bag for ExportToRawMeshDataCopy
	struct FAsyncBuildData
	{
		using ComponentDataMap = TMap<ULandscapeComponent*, FAsyncComponentData>;
		ComponentDataMap ComponentData;

		TWeakObjectPtr<ALandscapeProxy> LandscapeWeakRef;
		TWeakObjectPtr<ULandscapeSubsystem> LandscapeSubSystemWeakRef;

		TStrongObjectPtr<UStaticMesh> NaniteStaticMesh;
		FMeshDescription* NaniteMeshDescription = nullptr;

		TArray<UMaterialInterface*, TInlineAllocator<4>> InputMaterials;
		TArray<FName, TInlineAllocator<4>> InputMaterialSlotNames;
		TInlineComponentArray<ULandscapeComponent*> InputComponents;
		FStaticMeshSourceModel* SourceModel = nullptr;
		TSharedPtr<FStaticMeshAttributes> MeshAttributes;

		// event triggered when this build is complete (only used in the async case)
		FGraphEventRef BuildCompleteEvent;

		int32 LOD = 0;

		std::atomic<bool> bExportResult = false;
		std::atomic<bool> bIsComplete = false;
		std::atomic<bool> bCancelled = false;
		std::atomic<bool> bStaticMeshNeedsToCallPostMeshBuild = false;	// true if we are waiting for the UStaticMesh PostMeshBuild async callback
		FDelegateHandle PostMeshBuildDelegateHandle;					// post mesh build delegate handle (so we can remove it cleanly)

		bool bWarnedStall = false;										// true if we've notified the user of a stall on this task

		double TimeStamp_Requested = -1.0;								// when the Nanite build was requested
		double TimeStamp_ExportMeshStart = -1.0;						// start of the export mesh task
		double TimeStamp_ExportMeshEnd = -1.0;							// end of the export mesh task
		double TimeStamp_StaticMeshBuildStart = -1.0;					// start of the mesh build task
		double TimeStamp_StaticMeshBatchBuildStart = -1.0;				// call to UStaticMesh::BatchBuild
		double TimeStamp_StaticMeshBatchBuildPostMeshBuildCall = -1.0;	// when we received the async PostMeshBuild callback (if async) - may be out of order
		double TimeStamp_StaticMeshBuildEnd = -1.0;						// end of the mesh build task
		double TimeStamp_LandscapeUpdateStart = -1.0;					// start of the LandscapeUpdate / CompleteStaticMesh call
		double TimeStamp_LandscapeUpdateEnd = -1.0;						// end of the LandscapeUpdate / CompleteStaticMesh call
		double TimeStamp_Complete = -1.0;								// time we marked the task bIsComplete
		double TimeStamp_Cancelled = -1.0;								// first time that the pipeline realized the task was cancelled

		bool CheckForStallAndWarn();
	};
}

UCLASS(hidecategories = (Display, Attachment, Physics, Debug, Collision, Movement, Rendering, PrimitiveComponent, Object, Transform, Mobility, VirtualTexture), showcategories = ("Rendering|Material"), MinimalAPI, Within = LandscapeProxy)
class ULandscapeNaniteComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	ULandscapeNaniteComponent(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;

	/** Gets the landscape proxy actor which owns this component */
	LANDSCAPE_API ALandscapeProxy* GetLandscapeProxy() const;
						
	/** Get the landscape actor associated with this component. */
	LANDSCAPE_API ALandscape* GetLandscapeActor() const;

	inline const FGuid& GetProxyContentId() const
	{
		return ProxyContentId;
	}

	void SetProxyContentId(const FGuid& InProxyContentId) 
	{ 
		ProxyContentId = InProxyContentId; 
	}

	void UpdatedSharedPropertiesFromActor();

	void SetEnabled(bool bValue);

	inline bool IsEnabled() const
	{
		return bEnabled;
	}

	virtual bool NeedsLoadForServer() const override { return false; }
	virtual bool NeedsLoadForTargetPlatform(const class ITargetPlatform* TargetPlatform) const override;

	/** Copy the materials from the source ULandscapeComponents to this ULandscapeNaniteComponent's StaticMesh*/
	void UpdateMaterials();

	inline const TArray<TObjectPtr<ULandscapeComponent>>& GetSourceLandscapeComponents() const { return SourceLandscapeComponents; }
	void SetSourceLandscapeComponents(const TArray<ULandscapeComponent*>& InSourceLandscapeComponents);
private:
	
	/** Collect all the PSO precache data used by the static mesh component */
	virtual void CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams) override;

	/* The landscape proxy identity this Nanite representation was generated for */
	UPROPERTY()
	FGuid ProxyContentId;

	UPROPERTY(Transient)
	bool bEnabled;

	/** Landscape Components which were used to generate this ULandscapeNaniteComponent*/
	UPROPERTY()
	TArray<TObjectPtr<ULandscapeComponent>> SourceLandscapeComponents;

public:
#if WITH_EDITOR
	/**
	 * Generates the Nanite static mesh, stores the content Id.
	 * @param Landscape Proxy to generate the mesh for
	 * @param NewProxyContentId Hash of the content that this mesh corresponds to
	 * 
	 * @return true if the Nanite mesh creation was successful
	 */
	LANDSCAPE_API bool InitializeForLandscape(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId, const TArray<ULandscapeComponent*>& InComponentsToExport, int32 InNaniteComponentIndex);

	LANDSCAPE_API FGraphEventRef InitializeForLandscapeAsync(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId, const TArray<ULandscapeComponent*>& InComponentsToExport, int32 InNaniteComponentIndex);

	UE_DEPRECATED(5.6, "Use the new version of InitializeForLandscapeAsync (above)")
	FGraphEventRef InitializeForLandscapeAsync(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId, bool bInIsAsyncUNUSED, const TArray<ULandscapeComponent*>& InComponentsToExport, int32 InNaniteComponentIndex)
	{
		return InitializeForLandscapeAsync(Landscape, NewProxyContentId, InComponentsToExport, InNaniteComponentIndex);
	}

	/**
	 * Ensures the cooked cached platform data of the Nanite static mesh is finished. It is necessary to ensure that StreamablePages are loaded from DDC
	 * @param Landscape Proxy for this Nanite landscape component
	 * @param TargetPlatform info about the platform being cooked
	 * 
	 * @return true if the Nanite mesh creation was successful
	 */
	LANDSCAPE_API bool InitializePlatformForLandscape(ALandscapeProxy* Landscape, const ITargetPlatform* TargetPlatform);
#endif

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual bool IsHLODRelevant() const override;
};
