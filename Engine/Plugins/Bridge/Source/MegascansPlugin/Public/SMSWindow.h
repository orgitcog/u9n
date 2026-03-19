// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once



#include "Templates/SharedPointer.h"

class SWindow;
class UMegascansSettings;

class FTabManager;

class MEGASCANSPLUGIN_API MegascansSettingsWindow
{
public:
	
	
	static void OpenSettingsWindow(/*const TSharedRef<FTabManager>& TabManager*/);
	static void SaveSettings(const TSharedRef<SWindow>& Window, UMegascansSettings* MegascansSettings);	
	
	
};
