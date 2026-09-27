// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

namespace UE::Dataflow
{
	class FContext;
}

/**
* FDataflowSEditorInterface
* 
*/
class FDataflowSEditorInterface
{
public:
	/** Dataflow editor content accessors */
	virtual TSharedPtr<UE::Dataflow::FContext> GetDataflowContext() const = 0;

	virtual bool NodesHaveToggleWidget() const
	{
		return true;
	}

	virtual bool NodesHaveFreezeWidget() const
	{
		return true;
	}

	virtual void OnRenderToggleChanged() const
	{
	}

protected:
	FDataflowSEditorInterface() = default;
};
