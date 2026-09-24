// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/IKRetargetSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRetargetSettings)

#if WITH_EDITOR
FLinearColor FIKRetargetDebugDrawState::Muted = FLinearColor::Gray;
FLinearColor FIKRetargetDebugDrawState::SourceColor = (FLinearColor::Gray * FLinearColor::Blue) * Muted;
FLinearColor FIKRetargetDebugDrawState::GoalColor = FLinearColor::Yellow;
FLinearColor FIKRetargetDebugDrawState::MainColor = FLinearColor::Green;
FLinearColor FIKRetargetDebugDrawState::NonSelected = FLinearColor::Gray * 0.3f;
#endif
