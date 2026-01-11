// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AESHandlerComponent : ModuleRules
{
	public AESHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PacketHandler",
				"PlatformCrypto",
				"PlatformCryptoContext",
			}
		);
	}
}