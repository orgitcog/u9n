// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassRepresentationTypes.h"
#include "Components/ActorComponent.h"
#include "Misc/MTAccessDetector.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassCommonTypes.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassVisualizationComponent.generated.h"


class UInstancedStaticMeshComponent;

/** 
 * This component handles all the static mesh instances for a MassRepresentationProcessor and is an actor component off a MassVisualizer actor.
 * Meant to be created at runtime and owned by an MassVisualizer actor. Will ensure if placed on a different type of actor. 
 */
UCLASS(MinimalAPI)
class UMassVisualizationComponent : public UActorComponent
{
	GENERATED_BODY()
public:

	/** 
	 * Get the index of the visual type, will add a new one if does not exist
	 * @param Desc is the information for the visual that will be instantiated later via AddVisualInstance()
	 * @return The index of the visual type 
	 */
	MASSREPRESENTATION_API FStaticMeshInstanceVisualizationDescHandle FindOrAddVisualDesc(const FStaticMeshInstanceVisualizationDesc& Desc);

	/** 
	 * Creates a dedicated visual type described by host Desc and ties ISMComponent to it.
	 * @note this is a helper function for a common "single ISMComponent" case. Calls AddVisualDescWithISMComponents under the hood.
	 * @return The index of the visual type 
	 */
	MASSREPRESENTATION_API FStaticMeshInstanceVisualizationDescHandle AddVisualDescWithISMComponent(const FStaticMeshInstanceVisualizationDesc& Desc, UInstancedStaticMeshComponent& ISMComponent);

	/**
	 * Creates a dedicated visual type described by host Desc and ties given ISMComponents to it.
	 * @return The index of the visual type
	 */
	MASSREPRESENTATION_API FStaticMeshInstanceVisualizationDescHandle AddVisualDescWithISMComponents(const FStaticMeshInstanceVisualizationDesc& Desc, TArrayView<TObjectPtr<UInstancedStaticMeshComponent>> ISMComponents);

	/**
	 * Fetches FMassISMCSharedData indicated by DescriptionIndex, or nullptr if it's not a valid index
	 */
	MASSREPRESENTATION_API const FMassISMCSharedData* GetISMCSharedDataForDescriptionIndex(const int32 DescriptionIndex) const;

	/**
	 * Fetches FMassISMCSharedData indicated by an ISMC, or nullptr if the ISMC is not represented by any shared data.
	 */
	MASSREPRESENTATION_API const FMassISMCSharedData* GetISMCSharedDataForInstancedStaticMesh(const UInstancedStaticMeshComponent* ISMC) const;

	/** 
	 * Removes all the visualization data associated with the given ISM component. The function resolves the VisualizationIndex
	 * associated with the given ISMComponent and calls RemoveVisualDescByIndex which will remove data on all ISMComponents
	 * associated with the index. Note that this is safe to do only when there are no entities relying on this data. 
	 * No entity data patching will take place. 
	 */
	UE_DEPRECATED(5.4, "RemoveISMComponent has been deprecated in favor of RemoveVisualDesc. Please use that instead.")
	MASSREPRESENTATION_API void RemoveISMComponent(UInstancedStaticMeshComponent& ISMComponent);

	/**
	 * Removes all data associated with a given VisualizationIndex. Note that this is safe to do only if there are no
	 * entities relying on this index. No entity data patching will take place.
	 */
	UE_DEPRECATED(5.4, "RemoveVisualDescByIndex has been deprecated in favor of RemoveVisualDesc. Please use that instead.")
	MASSREPRESENTATION_API void RemoveVisualDescByIndex(const int32 VisualizationIndex);

	/**
	 * Removes all data associated with a given VisualizationIndex. Note that this is safe to do only if there are no
	 * entities relying on this index. No entity data patching will take place.
	 */
	MASSREPRESENTATION_API void RemoveVisualDesc(const FStaticMeshInstanceVisualizationDescHandle VisualizationHandle);

	/** Get the array of all visual instance informations */
	FMassInstancedStaticMeshInfoArrayView GetMutableVisualInfos()
	{
		FMassInstancedStaticMeshInfoArrayView View = MAKE_MASS_INSTANCED_STATIC_MESH_INFO_ARRAY_VIEW(MakeArrayView(InstancedStaticMeshInfos), InstancedStaticMeshInfosDetector);
		return MoveTemp(View);
	}

	/** Destroy all visual instances */
 	MASSREPRESENTATION_API void ClearAllVisualInstances();

	/** Dirty render state on all static mesh components */
 	MASSREPRESENTATION_API void DirtyVisuals();

	/** Signal the beginning of the static mesh instance changes, used to prepare the batching update of the static mesh instance transforms*/
	MASSREPRESENTATION_API void BeginVisualChanges();

	/** Signal the end of the static mesh instance changes, used to batch apply the transforms on the static mesh instances*/
	MASSREPRESENTATION_API void EndVisualChanges();

protected:
	/**
	 * Process all removed IDs in FMassISMCSharedData and apply to the ISM component.
	 */
	MASSREPRESENTATION_API void ProcessRemoves(UInstancedStaticMeshComponent& ISMComponent, FMassISMCSharedData& SharedData, bool bUpdateNavigation = true);
	
	/** 
	 * Applies changes accumulated in SharedData while manually updating the Instance ID mapping. This approach is done in preparation 
	 * to upcoming ISM changes to keep the mapping management more secure (by making mapping private and fully component-owned).
	 */
	MASSREPRESENTATION_API void HandleChangesWithExternalIDTracking(UInstancedStaticMeshComponent& ISMComponent, FMassISMCSharedData& SharedData);

	/** Recreate all the static mesh components from the InstancedStaticMeshInfos */
	MASSREPRESENTATION_API void ConstructStaticMeshComponents();

	/** Overridden to make sure this component is only added to a MassVisualizer actor */
	MASSREPRESENTATION_API virtual void PostInitProperties() override;

	/**
	 * Creates LODSignificance ranges for all the meshes indicated by Info
	 * @param ForcedStaticMeshRefKeys if not empty will be used when adding individual FMassStaticMeshInstanceVisualizationMeshDesc
	 *	instances to LOD significance ranges.
	 */	
	MASSREPRESENTATION_API void BuildLODSignificanceForInfo(FMassInstancedStaticMeshInfo& Info, TConstArrayView<UInstancedStaticMeshComponent*> StaticMeshRefKeys);

	/** Either adds an element to InstancedStaticMeshInfos or reuses an existing entry based on InstancedStaticMeshInfosFreeIndices*/
	MASSREPRESENTATION_API FStaticMeshInstanceVisualizationDescHandle AddInstancedStaticMeshInfo(const FStaticMeshInstanceVisualizationDesc& Desc);

	/** The information of all the instanced static meshes. Make sure to use AddInstancedStaticMeshInfo to add elements to it */
	UPROPERTY(Transient)
	TArray<FMassInstancedStaticMeshInfo> InstancedStaticMeshInfos;
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(InstancedStaticMeshInfosDetector);

	/** Indices to InstancedStaticMeshInfos that have been released and can be reused */
	TArray<FStaticMeshInstanceVisualizationDescHandle> InstancedStaticMeshInfosFreeIndices;

	/** Mapping from ISMComponent (indicated by FISMCSharedDataKey) to corresponding VisualDescHandle */
	TMap<FISMCSharedDataKey, FStaticMeshInstanceVisualizationDescHandle> ISMComponentMap;

	FMassISMCSharedDataMap ISMCSharedData;

	/** 
	 * Mapping FMassStaticMeshInstanceVisualizationMeshDesc hash to FMassISMCSharedData entries for all FMassStaticMeshInstanceVisualizationMeshDesc
	 * that didn't come with ISMC explicitly provided. Used only for initialization.
	 * Note that FMassStaticMeshInstanceVisualizationMeshDesc that were added with ISMComponents provided directly
	 * (via AddVisualDescWithISMComponents call) will never make it to this map.
	 */
	TMap<uint32, FISMCSharedDataKey> MeshDescToISMCMap;

	/** Indicies to InstancedStaticMeshInfos that need their SMComponent constructed */
	TArray<FStaticMeshInstanceVisualizationDescHandle> InstancedSMComponentsRequiringConstructing;

	UE_DEPRECATED(5.4, "This flavor of BuildLODSignificanceForInfo is no longer supported and is defunct.")
	void BuildLODSignificanceForInfo(FMassInstancedStaticMeshInfo& Info, const uint32 ForcedStaticMeshRefKey){}

	UE_DEPRECATED(5.5, "This flavor of BuildLODSignificanceForInfo is no longer supported and is defunct.")
	void BuildLODSignificanceForInfo(FMassInstancedStaticMeshInfo& Info) {}
};
