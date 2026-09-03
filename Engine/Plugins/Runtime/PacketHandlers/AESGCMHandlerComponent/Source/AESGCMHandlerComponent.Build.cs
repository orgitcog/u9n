// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AESGCMHandlerComponent : ModuleRules
{
	public AESGCMHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"PacketHandler",
				"PlatformCrypto",
				"PlatformCryptoContext",
				"NetCore"
			}
		);
	}
}