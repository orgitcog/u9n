// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UToolTarget;
class UMetaHumanCharacter;

/**
 * Utility and helper functions to interact with tool targets
 * Largely based on ModelingToolTargetUtil.h
 */
namespace UE::ToolTarget
{

/**
 * Returns the MetaHuman Character asset backing a tool target, or nullptr if there is no such asset
 */
UMetaHumanCharacter* GetTargetMetaHumanCharacter(UToolTarget* InTarget);

} // namespace UE::MetaHuman