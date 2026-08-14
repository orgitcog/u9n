// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

enum EVerseClassFlags
{
	VCLASS_None = 0x00000000u,
	VCLASS_NativeBound = 0x00000001u,
	VCLASS_UniversallyAccessible = 0x00000002u,   // The class is accessible from any Verse path, and is in a package with a public scope.
	VCLASS_Concrete = 0x00000004u,                // The class can be instantiated without explicitly setting any properties
	VCLASS_Module = 0x00000008u,                  // This class represents a Verse module
	VCLASS_UHTNative = 0x00000010u,               // This class was created by UHT
	VCLASS_Tuple = 0x00000020u,                   // This class represents a tuple
	VCLASS_EpicInternal = 0x00000040u,            // This class is epic_internal
	VCLASS_HasInstancedSemantics = 0x00000080u,   // This class is using explicit instanced reference semantics (backcompat)
	VCLASS_FinalSuper = 0x00000100u,              // This class has a <final_super> attribute
	VCLASS_Castable = 0x00000200u,                // This class has a <castable> attribute
	VCLASS_EpicInternalConstructor = 0x00000400u, // This class' has a <castable> attribute's constructor is epic_internal

	// @TODO: this should be a per-function flag; a class flag is not granular enough
	VCLASS_Err_Inoperable = 0x40000000u, // One or more of the class's functions contain mis-linked (malformed) bytecode

	VCLASS_Err_Incomplete = 0x80000000u, // The class layout is malformed (missing super, illformed data-member, etc.)

	VCLASS_Err = (VCLASS_Err_Incomplete | VCLASS_Err_Inoperable)
};
