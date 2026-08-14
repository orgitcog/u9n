// Copyright Epic Games, Inc. All Rights Reserved.

#include "TweenLogic.h"

#include "ControlRigTweenModel.h"
#include "ControlRigTweenModels.h"
#include "EditMode/ControlRigEditMode.h"
#include "Framework/Commands/UICommandList.h"
#include "Math/Abstraction/TweenModelArray.h"
#include "Math/Models/EditorTweenModel.h"
#include "Misc/SequencerUtils.h"
#include "Templates/SharedPointer.h"

namespace UE::ControlRigEditor
{
FTweenLogic::FTweenLogic(const TAttribute<TWeakPtr<ISequencer>>& InSequencerAttr, const TSharedRef<FControlRigEditMode>& InOwningEditMode)
	: CommandList(InOwningEditMode->GetCommandBindings().ToSharedRef())
	, TweenModels(MakeShared<FControlRigTweenModels>(InSequencerAttr, InOwningEditMode))
	, Controllers(CommandList, TweenModels, TEXT("ControlRigTween"))
{}

TSharedRef<SWidget> FTweenLogic::MakeWidget() const
{
	// Visually move the slider widget when the user uses the U+LMB button command to indirectly move the mouse
	TAttribute<TOptional<float>> OverrideSliderPosition =
		TAttribute<TOptional<float>>::CreateLambda([this]()
		{
			return Controllers.MouseSlidingController.GetCurrentSliderPosition();
		});
	
	FSlimHorizontalToolBarBuilder ToolBarBuilder(CommandList, FMultiBoxCustomization::None);
	ToolBarBuilder.SetStyle(&FAppStyle::Get(), TEXT("ControlRigTweenToolbar"));
	Controllers.ToolbarController.AddToToolbar(ToolBarBuilder, { MoveTemp(OverrideSliderPosition) });
	return ToolBarBuilder.MakeWidget();
}
}
