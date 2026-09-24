// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Attribute.h"
#include "Templates/SharedPointerFwd.h"
#include "Widgets/MVC/TweenControllers.h"

class FControlRigEditMode;
class FUICommandList;
class ISequencer;

namespace UE::ControlRigEditor
{
class FControlRigTweenModels;

/** Creates the tweening model specific to ControlRig and provides an interface for interacting with it. */
class FTweenLogic
{
public:

	explicit FTweenLogic(const TAttribute<TWeakPtr<ISequencer>>& InSequencerAttr, const TSharedRef<FControlRigEditMode>& InOwningEditMode);

	/** Constructs the content for the viewport tweening widget. */
	TSharedRef<SWidget> MakeWidget() const;
	
private:

	/** Owning control rig's command list. */
	const TSharedRef<FUICommandList> CommandList;

	/** Holds the used tweening functions and info about how to display them in UI. */
	const TSharedRef<FControlRigTweenModels> TweenModels;

	/** Common functionality that should be shared consistent with other editor modules, such as Curve Editor. */
	const TweeningUtilsEditor::FTweenControllers Controllers;
};
}
