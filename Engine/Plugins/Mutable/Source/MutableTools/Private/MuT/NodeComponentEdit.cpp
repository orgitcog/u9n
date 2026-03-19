// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeComponentEdit.h"

#include "MuT/NodeComponentNew.h"
#include "MuT/NodeSurface.h"

namespace mu
{

	const NodeComponentNew* NodeComponentEdit::GetParentComponentNew() const
	{
		if (Parent)
		{
			return Parent->GetParentComponentNew();
		}

		return nullptr;
	}

}
