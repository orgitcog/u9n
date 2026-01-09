// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanCalibrationLib : ModuleRules
{
	protected bool BuildForDevelopment
	{
		get
		{
			// Check if source is available
			string SourceFilesPath = Path.Combine(ModuleDirectory, "Private");
			return Directory.Exists(SourceFilesPath) &&
				   Directory.GetFiles(SourceFilesPath).Length > 0;
		}
	}

	public MetaHumanCalibrationLib(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled = !BuildForDevelopment;
		IWYUSupport = IWYUSupport.None;
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
		CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Warning;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] {
		});

		PrivateDependencyModuleNames.AddRange(new string[] 
		{
			"Core",
			"OpenCVHelper",
			"OpenCV",
			"ImageCore",
			"Eigen",
			"CaptureDataCore",
			"RigLogicLib"
		});

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PrivateIncludePaths.AddRange(new string[]
		{
			Path.Combine(ModuleDirectory, "Private/api"),
			Path.Combine(ModuleDirectory, "Private/carbon/include"),
			Path.Combine(ModuleDirectory, "Private/nls/include"),
			Path.Combine(ModuleDirectory, "Private/calib/include"),
			Path.Combine(ModuleDirectory, "Private/rig/include")
		});

		PrivateDefinitions.Add("TITAN_DYNAMIC_API");
		PrivateDefinitions.Add("CALIB_DYNAMIC_API");
		PrivateDefinitions.Add("EIGEN_MPL2_ONLY");
		PrivateDefinitions.Add("TITAN_NAMESPACE=calibrationlib::epic::nls");
		PrivateDefinitions.Add("TITAN_API_NAMESPACE=calibrationlib::titan::api");

		if (Target.Architecture == UnrealArch.X64)
		{
			PrivateDefinitions.Add("CARBON_ENABLE_SSE=0");
			PrivateDefinitions.Add("CARBON_ENABLE_AVX=0"); // CARBON_ENABLE_AVX does not work with Windows clang build
		}

		// This module uses exceptions in the core tech libs, so they must be enabled here
		bEnableExceptions = true;

		// AutoRTFM cannot be used with exceptions.
		bDisableAutoRTFMInstrumentation = true;

		bUseRTTI = true;
	}
}
