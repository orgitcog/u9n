// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Bridge functions for communicating from C++ to SwiftMain.swift.
// See LaunchCPPToSwift.h for an example of going the other way, from SwiftMain.swift to C++.

#if PLATFORM_VISIONOS

namespace UE::SwiftMainBridgeNS
{
	void ConfigureImmersiveSpace(int32 InImmersiveStyle, int32 InUpperLimbVisibility);
}

#endif // PLATFORM_VISIONOS
