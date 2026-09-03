// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class UObject;
namespace Verse { struct VPackage; }

// used by the EDL at boot time to coordinate loading with what is going on with the deferred registration stuff
enum class ENotifyRegistrationType
{
	NRT_Class,
	NRT_ClassCDO,
	NRT_Struct,
	NRT_Enum,
	NRT_Package,
	NRT_NoExportObject,
};

enum class ENotifyRegistrationPhase
{
	NRP_Added,
	NRP_Started,
	NRP_Finished,
};

COREUOBJECT_API void NotifyRegistrationEvent(const TCHAR* PackageName, const TCHAR* Name, ENotifyRegistrationType NotifyRegistrationType, ENotifyRegistrationPhase NotifyRegistrationPhase, UObject *(*InRegister)() = nullptr, bool InbDynamic = false, UObject* FinishedObject = nullptr);
void NotifyScriptVersePackage(Verse::VPackage* Package);
COREUOBJECT_API void NotifyRegistrationComplete();
