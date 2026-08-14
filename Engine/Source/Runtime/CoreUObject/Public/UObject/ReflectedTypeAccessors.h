// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UClass;
class UScriptStruct;
class UEnum;

/*-----------------------------------------------------------------------------
C++ templated Static(Class/Struct/Enum) retrieval function prototypes.
-----------------------------------------------------------------------------*/

template<typename ClassType> inline UClass* StaticClass()
{
	return ClassType::StaticClass();
}

template<typename StructType> inline UScriptStruct* StaticStruct()
{
	return StructType::StaticStruct();
}

template<typename EnumType> UEnum* StaticEnum();
