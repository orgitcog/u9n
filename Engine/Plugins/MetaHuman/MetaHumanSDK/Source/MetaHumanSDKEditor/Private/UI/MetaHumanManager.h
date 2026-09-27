// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/UniquePtr.h"

namespace UE::MetaHuman
{
class FMetaHumanManagerImpl;

/**
 * Class that handles the display of the MetaHuman Manager UI for packaging MetaHuman Assets
 */
class FMetaHumanManager
{
public:
	/**
	 * Initializes the manager and registers the UI with the editor
	 */
	static void Initialize();
	static void Shutdown();

private:
	FMetaHumanManager() = default;
	static TUniquePtr<FMetaHumanManagerImpl> Instance;
};
} // namespace UE::MetaHuman
