// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/UICommandList.h"

#include "MetaHumanViewportModes.h"

/**
 * A helper class to manage the command lists in the AB view environments.
 * It is basically a container for two distinct FUICommandList objects
 * with helper functions to bind commands using a single function call.
 * This can be passed around by value as the internals are just pointers
 */
class FMetaHumanABCommandList final
{
public:
	FMetaHumanABCommandList()
		: CommandListA{ MakeShared<FUICommandList>() }
		, CommandListB{ MakeShared<FUICommandList>() }
	{}

	/**
	 * @brief Maps the given member functions to each command list so they are called with the appropriate EABImageViewMode enum
	 * @param InCommand The FUICommandInfo to map in the command lists
	 * @param InObject The object that will get called when the command is activated
	 * @param InExecuteActionFunc The member function of the given object that will get called with the command is triggered
	 * @param InIsActionCheckedFunc The member function of the give object called to determine if the action is checked or not
	 */
	template<typename UserClass, ESPMode Mode, typename ExecuteActionFunc, typename CanExecuteActionFunc, typename IsActionCheckedFunc, typename... VarTypes>
	void MapAction(TSharedPtr<FUICommandInfo> InCommand,
				   TSharedRef<UserClass, Mode> InObject,
				   ExecuteActionFunc InExecuteActionFunc,
				   CanExecuteActionFunc InCanExecuteActionFunc,
				   IsActionCheckedFunc InIsActionCheckedFunc,
				   VarTypes... InVars)
	{
		check(CommandListA.IsValid() && CommandListB.IsValid());

		CommandListA->MapAction(InCommand,
								FExecuteAction::CreateSP(InObject, InExecuteActionFunc, EABImageViewMode::A, InVars...),
								FCanExecuteAction::CreateSP(InObject, InCanExecuteActionFunc, EABImageViewMode::A),
								FIsActionChecked::CreateSP(InObject, InIsActionCheckedFunc, EABImageViewMode::A, InVars...));

		CommandListB->MapAction(InCommand,
								FExecuteAction::CreateSP(InObject, InExecuteActionFunc, EABImageViewMode::B, InVars...),
								FCanExecuteAction::CreateSP(InObject, InCanExecuteActionFunc, EABImageViewMode::B),
								FIsActionChecked::CreateSP(InObject, InIsActionCheckedFunc, EABImageViewMode::B, InVars...));
	}

	/** Gets the command list for a view mode */
	TSharedPtr<FUICommandList> GetCommandList(EABImageViewMode InViewMode)
	{
		check(InViewMode == EABImageViewMode::A || InViewMode == EABImageViewMode::B);
		return InViewMode == EABImageViewMode::A ? CommandListA : CommandListB;
	}

private:
	/** Command list associated with view A */
	TSharedPtr<FUICommandList> CommandListA;

	/** Command list associated with view B */
	TSharedPtr<FUICommandList> CommandListB;
};