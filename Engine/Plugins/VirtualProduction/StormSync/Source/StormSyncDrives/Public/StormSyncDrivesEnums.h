// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

enum class EStormSyncDriveErrorCode : uint8
{
	/** No error */
	None,

	/** Root path is empty */
	EmptyRootPath,

	/** Path is "/". No mounting allowed at the top level root */
	TopLevelRootDisallowed,

	/** Path does not have a standard package name format */
	InvalidPackageNameFormat,

	/** Path is expected to be within /Game or one level path e.g. /Example when not within /Game */
	InvalidPathLevel,

	/** Directory path is empty */
	EmptyDirectoryPath,

	/** Directory path does not exist */
	DirectoryNotFound,
};
