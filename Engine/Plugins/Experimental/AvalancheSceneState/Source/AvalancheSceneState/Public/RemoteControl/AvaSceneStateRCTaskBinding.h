// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/SceneStateTaskBindingExtension.h"
#include "AvaSceneStateRCTaskBinding.generated.h"

struct FAvaSceneStateRCTaskInstance;

USTRUCT()
struct FAvaSceneStateRCTaskBinding : public FSceneStateTaskBindingExtension
{
	GENERATED_BODY()

	using FInstanceDataType = FAvaSceneStateRCTaskInstance;

	/** Data View Indices */
	static constexpr uint16 CONTROLLER_VALUES_DATA_INDEX = 0;

protected:
	//~ Begin FSceneStateTaskBindingExtension
#if WITH_EDITOR
	virtual void VisitBindingDescs(FConstStructView InTaskInstance, TFunctionRef<void(const UE::SceneState::FTaskBindingDesc&)> InFunctor) const override;
	virtual void SetBindingBatch(uint16 InDataIndex, uint16 InBatchIndex) override;
	virtual bool FindDataById(FStructView InTaskInstance, const FGuid& InStructId, FStructView& OutDataView, uint16& OutDataIndex) const override;
#endif
	virtual bool FindDataByIndex(FStructView InTaskInstance, uint16 InDataIndex, FStructView& OutDataView) const override;
	virtual void VisitBindingBatches(FStructView InTaskInstance, TFunctionRef<void(uint16, FStructView)> InFunctor) const override;
	//~ End FSceneStateTaskBindingExtension

private:
	UPROPERTY()
	uint16 ControllerValuesBatchIndex = std::numeric_limits<uint16>::max();
};
