// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSPlatformInput.h"
#include "IOS/IOSInputInterface.h"

uint32 FIOSPlatformInput::GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };
	
	uint32 NumMappings = 0;
	
	// we only handle a few "fake" keys from the IOS keyboard delegate stuff in IOSView.cpp
	if (KeyCodes && KeyNames && (MaxMappings > 0))
	{
		ADDKEYMAP(KEYCODE_ENTER, TEXT("Enter"));
		ADDKEYMAP(KEYCODE_BACKSPACE, TEXT("BackSpace"));
		ADDKEYMAP(KEYCODE_ESCAPE, TEXT("Escape"));
		ADDKEYMAP(KEYCODE_TAB, TEXT("Tab"));
		
		ADDKEYMAP(KEYCODE_LEFT, TEXT("Left"));
		ADDKEYMAP(KEYCODE_RIGHT, TEXT("Right"));
		ADDKEYMAP(KEYCODE_DOWN, TEXT("Down"));
		ADDKEYMAP(KEYCODE_UP, TEXT("Up"));
		
		ADDKEYMAP(KEYCODE_LEFT_CONTROL, TEXT("LeftControl"));
		ADDKEYMAP(KEYCODE_LEFT_SHIFT, TEXT("LeftShift"));
		ADDKEYMAP(KEYCODE_LEFT_ALT, TEXT("LeftAlt"));
		ADDKEYMAP(KEYCODE_LEFT_COMMAND, TEXT("LeftCommand"));
		ADDKEYMAP(KEYCODE_CAPS_LOCK, TEXT("CapsLock"));
		ADDKEYMAP(KEYCODE_RIGHT_CONTROL, TEXT("RightControl"));
		ADDKEYMAP(KEYCODE_RIGHT_SHIFT, TEXT("RightShift"));
		ADDKEYMAP(KEYCODE_RIGHT_ALT, TEXT("RightAlt"));
		ADDKEYMAP(KEYCODE_RIGHT_COMMAND, TEXT("RightCommand"));
		
		ADDKEYMAP(KEYCODE_F1, TEXT("F1"));
		ADDKEYMAP(KEYCODE_F2, TEXT("F2"));
		ADDKEYMAP(KEYCODE_F3, TEXT("F3"));
		ADDKEYMAP(KEYCODE_F4, TEXT("F4"));
		ADDKEYMAP(KEYCODE_F5, TEXT("F5"));
		ADDKEYMAP(KEYCODE_F6, TEXT("F6"));
		ADDKEYMAP(KEYCODE_F7, TEXT("F7"));
		ADDKEYMAP(KEYCODE_F8, TEXT("F8"));
		ADDKEYMAP(KEYCODE_F9, TEXT("F9"));
		ADDKEYMAP(KEYCODE_F10, TEXT("F10"));
		ADDKEYMAP(KEYCODE_F11, TEXT("F11"));
		ADDKEYMAP(KEYCODE_F12, TEXT("F12"));
	}
	return NumMappings;

#undef ADDKEYMAP
}

uint32 FIOSPlatformInput::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformInput::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, true, true);
}
