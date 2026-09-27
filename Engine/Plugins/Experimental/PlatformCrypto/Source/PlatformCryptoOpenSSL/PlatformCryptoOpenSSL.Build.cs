// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

[Obsolete("Deprecated in UE5.6 - Use PlatformCryptoContext instead.")]
public class PlatformCryptoOpenSSL : ModuleRules
{
	public PlatformCryptoOpenSSL(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
	}
}
