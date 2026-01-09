// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITweenModelContainer.h"
#include "Math/Models/TweenModel.h"
#include "TweenModelDisplayInfo.h"
#include "Templates/UnrealTemplate.h"

namespace UE::TweeningUtilsEditor
{
/** Backed by an array of tween models and their UI info. */
class TWEENINGUTILSEDITOR_API FTweenModelArray : public ITweenModelContainer, public FNoncopyable
{
public:
	
	explicit FTweenModelArray(TArray<FTweenModelUIEntry> InTweenModels) : TweenModels(MoveTemp(InTweenModels)) {}

	//~ Begin FTweenModelContainer Interface
	virtual void ForEachModel(const TFunctionRef<void(FTweenModel&)> InConsumer) const override;
	virtual FTweenModel* GetModel(int32 InIndex) const override;
	virtual int32 NumModels() const override { return TweenModels.Num(); }
	virtual TSharedPtr<FUICommandInfo> GetCommandForModel(const FTweenModel& InTweenModel) const override;
	virtual const FSlateBrush* GetIconForModel(const FTweenModel& InTweenModel) const override;
	virtual FLinearColor GetColorForModel(const FTweenModel& InTweenModel) const override;
	virtual FText GetLabelForModel(const FTweenModel& InTweenModel) const override;
	virtual FText GetToolTipForModel(const FTweenModel& InTweenModel) const override;
	virtual FString GetModelIdentifier(const FTweenModel& InTweenModel) const override;
	//~ Begin FTweenModelContainer Interface

private:

	/** Info about the contained tween models. */
	const TArray<FTweenModelUIEntry> TweenModels;
};
}
