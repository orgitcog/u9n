// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UCineAssembly;

namespace UE::CineAssemblyToolsAnalytics
{
	void RecordEvent_CreateAssembly(const UCineAssembly* const Assembly);
	void RecordEvent_CreateAssemblySchema();
	void RecordEvent_CreateProduction();
	void RecordEvent_ProductionAddAssetNaming();
	void RecordEvent_ProductionCreateTemplateFolders();
	void RecordEvent_RecordAssembly();
};