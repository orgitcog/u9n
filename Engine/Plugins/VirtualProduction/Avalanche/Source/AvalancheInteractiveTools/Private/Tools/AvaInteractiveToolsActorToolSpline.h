// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/AvaInteractiveToolsActorPointToolBase.h"
#include "AvaInteractiveToolsActorToolSpline.generated.h"

UCLASS()
class UAvaInteractiveToolsActorToolSpline : public UAvaInteractiveToolsActorPointToolBase
{
	GENERATED_BODY()

public:
	UAvaInteractiveToolsActorToolSpline();

	//~ Begin UAvaInteractiveToolsToolBase
	virtual bool UseIdentityRotation() const override { return false; }
	virtual void OnRegisterTool(IAvalancheInteractiveToolsModule* InAITModule) override;
	//~ End UAvaInteractiveToolsToolBase
};
