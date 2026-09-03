// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "DataflowPreview.generated.h"

class UAnimationAsset;
class USkeletalMesh;

USTRUCT()
struct FDataflowPreviewCacheParams
{
	GENERATED_BODY()
public :

	/** Number of sampling frames per second for caching*/
	UPROPERTY(EditAnywhere, Category="Caching")
	int32 FrameRate = 30;

	/** Number of subframes per frame for timeline clamping*/
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bCanEditSubframeRate", ClampMin = "1"), Category = "Caching")
	int32 SubframeRate = 1;
	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle), Category = "Caching")
	bool bCanEditSubframeRate = false;

	/** Time range of the simulation*/
	UPROPERTY(EditAnywhere, Category="Caching")
	FVector2f TimeRange = FVector2f(0.0f, 5.0f);

	/** If enabled, the simulation will restart on Time Range without modifying anything outside of Time Range */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Caching")
	bool bRestartSimulation = false;

	/** Time range of the restarted simulation*/
	UPROPERTY(EditAnywhere, Category = "Caching", meta = (EditCondition = "bRestartSimulation"))
	FVector2f RestartTimeRange = FVector2f(0.0f, 0.0f);

	/** Boolean to check if the caching will be done on an async thread (if yes no GT dependency) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Caching")
	bool bAsyncCaching = true;
};
