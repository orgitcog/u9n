// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ANY_PACKAGE macro for legacy support
=============================================================================*/

#pragma once

#define ANY_PACKAGE_DEPRECATED ((UPackage*)-1)

#define IS_ANY_PACKAGE_DEPRECATED(Package) (reinterpret_cast<PTRINT>(Package) == -1)