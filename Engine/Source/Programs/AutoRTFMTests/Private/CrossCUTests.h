// Copyright Epic Games, Inc. All Rights Reserved.

namespace CrossCU
{

int SomeFunction(int X);

struct FLargeStruct
{
	int Ints[32];

	static int Sum(FLargeStruct Struct);
};

} // CrossCU
