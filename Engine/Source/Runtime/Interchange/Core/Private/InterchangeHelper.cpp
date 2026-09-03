// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeHelper.h"
#include "InterchangeLogPrivate.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"

namespace UE::Interchange
{
	namespace Private
	{
		int32 GInterchangeMaxAssetPathLength = 160; // a base value that should suits most cases
		static FAutoConsoleVariableRef CVarInterchangeMaxAssetPathLength(
			TEXT("Interchange.MaxAssetPathLength"),
			GInterchangeMaxAssetPathLength,
			TEXT("Interchange will try to limit asset path length to this value. Default: 260")
		);

		int32 GetUsableMaxAssetPathLength()
		{
			const int32 MinWorkable = 60;
			static int32 ProjectConstraint; // Deduced from os limit and project path
			static bool RunOnce = [&]()
			{
				const FString ProjectContentDir = FPaths::ProjectContentDir();
				const FString FullPathProjectContentDir = FPaths::ConvertRelativePathToFull(ProjectContentDir);
				ProjectConstraint = FPlatformMisc::GetMaxPathLength() - FullPathProjectContentDir.Len();

				if (ProjectConstraint < MinWorkable)
				{
					UE_LOG(LogInterchangeCore, Error,
							TEXT("Interchange can encounter import issues due to a Content path too long, and an OS limitation on path length.\n")
							TEXT("Content path: '%s'\n")
							TEXT("System max path length: %d.\n")
							, *FullPathProjectContentDir, FPlatformMisc::GetMaxPathLength());
				}

				if (GInterchangeMaxAssetPathLength > ProjectConstraint)
				{
					UE_LOG(LogInterchangeCore, Warning,
							TEXT("The Interchange.MaxAssetPathLength value (%d) is too high for the current setup.\n")
							TEXT("Content path: '%s'\n")
							TEXT("System max path length: %d.\n")
							, GInterchangeMaxAssetPathLength, *FullPathProjectContentDir, FPlatformMisc::GetMaxPathLength());
				}

				return true;
			}();

			return FMath::Max(FMath::Min(GInterchangeMaxAssetPathLength, ProjectConstraint), MinWorkable);
		}
	}

	void SanitizeName(FString& OutName, bool bIsJoint)
	{
		const TCHAR* InvalidChar = bIsJoint ? INVALID_OBJECTNAME_CHARACTERS TEXT("+ ") : INVALID_OBJECTNAME_CHARACTERS;

		while (*InvalidChar)
		{
			if (bIsJoint)
			{
				//For joint we want to replace any space by a dash
				OutName.ReplaceCharInline(TCHAR(' '), TCHAR('-'), ESearchCase::CaseSensitive);
			}
			OutName.ReplaceCharInline(*InvalidChar, TCHAR('_'), ESearchCase::CaseSensitive);
			++InvalidChar;
		}
	}

	FString MakeName(const FString& InName, bool bIsJoint)
	{
		FString TmpName = InName;
		SanitizeName(TmpName, bIsJoint);
		return TmpName;
	}

	int32 GetAssetNameMaxCharCount(const FString& ParentPackage)
	{
		// can be tweaked, the goal is to be more restrictive than the filesystem
		// so that a project can be shared / moved without breaking the constraint
		int32 MaxAssetPathLength = Private::GetUsableMaxAssetPathLength();

		// internal limit of FNames + room for prefix, separators and null char. (Asset names occur twice in paths)
		int32 InternalNameConstraint = (NAME_SIZE - 100);

		int32 PackageLength = 1 + (!ParentPackage.IsEmpty() ? ParentPackage.Len() : 20);
		int32 Budget = FMath::Min((InternalNameConstraint - PackageLength) / 2, MaxAssetPathLength - PackageLength);
		Budget = FMath::Min(Budget, 255 - 10); // a filename cannot be longer than 255, and we keep a small buffer for the extension
		return FMath::Max(0, Budget);
	}

	FString GetAssetNameWBudget(const FString& DesiredAssetName, int32 CharBudget, TCHAR CharReplacement)
	{
		if (DesiredAssetName.Len() <= CharBudget)
		{
			return DesiredAssetName;
		}

		// Arbitrary number to avoid possible name collisions and having a very low char budget
		if (CharBudget <= 5)
		{
			UE_LOG(LogInterchangeCore,
				   Warning,
				   TEXT("Char budget is too small for generating a new asset name, please adapt the name of the asset manually or try modifying the value of Interchange.MaxAssetPathLength"),
				   *DesiredAssetName);
			return DesiredAssetName;
		}

		// Space for beginning, replacement and ending
		int32 Remaining = CharBudget - 2;
		int32 LeftCount = Remaining / 2;
		int32 RightCount = Remaining - LeftCount;

		FString Left = DesiredAssetName.Mid(0, LeftCount + 1);
		FString Right = DesiredAssetName.Mid(DesiredAssetName.Len() - (RightCount + 1));

		return Left + CharReplacement + Right;
	}
};