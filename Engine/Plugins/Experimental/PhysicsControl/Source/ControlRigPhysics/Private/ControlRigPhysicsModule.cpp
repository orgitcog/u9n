// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigPhysicsModule.h"
#if WITH_EDITOR
#include "ControlRigPhysicsEditorStyle.h"
#endif

#define LOCTEXT_NAMESPACE "FControlRigPhysicsModule"

DEFINE_LOG_CATEGORY(LogControlRigPhysics);

//======================================================================================================================
void FControlRigPhysicsModule::StartupModule()
{
#if WITH_EDITOR
	// register the editor style
	FControlRigPhysicsEditorStyle::Get();
#endif
}

//======================================================================================================================
void FControlRigPhysicsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FControlRigPhysicsModule, ControlRigPhysics)

