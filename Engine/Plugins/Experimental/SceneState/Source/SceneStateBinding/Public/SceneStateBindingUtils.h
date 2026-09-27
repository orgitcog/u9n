// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingDataView.h"
#include "Templates/Function.h"

class UObject;
struct FGuid;
struct FSceneStateBindingCollection;
struct FSceneStateBindingDataHandle;

namespace UE::SceneState
{

#if WITH_EDITOR
	/**
	 * Update the bindings of a source struct id into a target one.
	 * It goes through the given object's outer chain to get to the binding collection owner to do this operation
	 * @param InObject object to use to find the binding collection owner
	 * @param InOldStructId the struct id to queue copy bindings from
	 * @param InNewStructId the struct id to queue copy bindings to
	 */
	SCENESTATEBINDING_API void HandleStructIdChanged(UObject& InObject, const FGuid& InOldStructId, const FGuid& InNewStructId);
#endif

	struct FApplyBatchParams
	{
		/** Batch within the Binding Collection to run */
		uint16 BatchIndex;
		/** Target Data to copy the source data to */
		FPropertyBindingDataView TargetDataView;
		/** Binding collection holding the Copy Batch to run */
		const FSceneStateBindingCollection& BindingCollection;
		/** Functor to find the Source Data View for a given Data Handle */
		TFunctionRef<FPropertyBindingDataView(const FSceneStateBindingDataHandle&)> FindSourceDataViewFunctor;
	};
	/** Runs the Batch within the Binding Collection for a given Target Data */
	SCENESTATEBINDING_API bool ApplyBatch(const FApplyBatchParams& InParams);

	struct FPatchBindingParams
	{
		/** Binding collection to patch */
		FSceneStateBindingCollection& BindingCollection;
		/** Functor to find the data struct for a given Data Handle */
		TFunctionRef<const UStruct*(const FSceneStateBindingDataHandle&)> FindDataStructFunctor;
	};
	/** Patches invalidated structs (property bags, user defined structs, etc.) in the given binding collection */
	SCENESTATEBINDING_API void PatchBindingCollection(const FPatchBindingParams& InParams);

	/** Patches any invalidated structs (property bags, user defined structs, etc.) in the binding descs of the given binding collection */
	void PatchBindingDescs(const FPatchBindingParams& InParams);

	/** Patches any invalidated structs (property bags, user defined structs, etc.) in the bindings of the given binding collection */
	void PatchBindings(const FPatchBindingParams& InParams);

	/** Patches any invalidated structs (property bags, user defined structs, etc.) in the copy batches of the given binding collection */
	void PatchCopyBatches(const FPatchBindingParams& InParams);

} // UE::SceneState
