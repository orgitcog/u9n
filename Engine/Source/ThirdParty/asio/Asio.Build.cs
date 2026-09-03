// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Asio : ModuleRules
{
	static readonly string Version = "1.30.2";

	public Asio(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string AsioPath = Path.Combine(Target.UEThirdPartySourceDirectory, "asio", Version);

		PublicSystemIncludePaths.Add(AsioPath);

		PublicDefinitions.AddRange([
			"ASIO_SEPARATE_COMPILATION",
			"ASIO_STANDALONE",
			"ASIO_NO_EXCEPTIONS",
			"ASIO_NO_TYPEID",

			// The following are explicitly set because IncludeTool is unable to
			// parse the __has_include() preprocessor statements in Asio's config.hpp
			"ASIO_HAS_STD_ARRAY",
			"ASIO_HAS_STD_ATOMIC",
			//"ASIO_HAS_STD_CALL_FUTURE",
			//"ASIO_HAS_STD_CALL_ONCE",
			//"ASIO_HAS_STD_CHRONO",
			//"ASIO_HAS_STD_MUTEX_AND_CONDVAR",
			//"ASIO_HAS_STD_STRING_VIEW",
			"ASIO_HAS_STD_SYSTEM_ERROR",
			//"ASIO_HAS_STD_THREAD",
			"ASIO_HAS_STD_TYPE_TRAITS",
		]);
	}
}

