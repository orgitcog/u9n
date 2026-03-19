// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class IAvaSceneInterface;
struct FSceneStateExecutionContext;

namespace UE::AvaSceneState
{
	IAvaSceneInterface* FindSceneInterface(const FSceneStateExecutionContext& InContext);
}
