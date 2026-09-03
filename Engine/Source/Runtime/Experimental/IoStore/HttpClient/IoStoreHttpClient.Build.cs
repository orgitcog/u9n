// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IoStoreHttpClient : ModuleRules
{
	public IoStoreHttpClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("TraceLog");

		PrivateDependencyModuleNames.Add("OpenSSL");
		PrivateDefinitions.Add("IAS_HTTP_HAS_OPENSSL=1");

		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Error; 
	}
}
