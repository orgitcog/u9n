// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Widgets/SWindow.h"

class ULoadAnimToControlRigSettings;

DECLARE_DELEGATE_OneParam(FLoadAnimToControlRigDelegate, ULoadAnimToControlRigSettings*);

//dialog to show ULoadAnimToControlRigSettings properties for loading animation into a control rig section
struct  FLoadAnimToControlRigDialog
{
	static void GetLoadAnimParams(FLoadAnimToControlRigDelegate& Delegate, const FOnWindowClosed& OnClosedDelegate);
};