// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/IPCGEditorModule.h"

static IPCGEditorModule* PCGEditorModulePtr = nullptr;

IPCGEditorModule* IPCGEditorModule::Get()
{
	return PCGEditorModulePtr;
}

void IPCGEditorModule::SetEditorModule(IPCGEditorModule* InModule)
{
	check(PCGEditorModulePtr == nullptr || InModule == nullptr);
	PCGEditorModulePtr = InModule;
}