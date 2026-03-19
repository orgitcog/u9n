// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/AnimNextAssetItemDetails.h"

namespace UE::AnimNext::Editor
{

class FAnimNextAnimationGraphItemDetails : public FAnimNextAssetItemDetails
{
public:
	FAnimNextAnimationGraphItemDetails() = default;
	
	ANIMNEXTANIMGRAPHEDITOR_API virtual const FSlateBrush* GetItemIcon(const FWorkspaceOutlinerItemExport& Export) const override;

};

}
