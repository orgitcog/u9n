// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/ChaosLogUtil.h"
#include "CoreTypes.h"

namespace Chaos
{
	FString ToString(const EObjectStateType ObjectState)
	{
		static FString StateStrings[] =
		{
			TEXT("Uninitialized(0)"),
			TEXT("Sleeping(1)"),
			TEXT("Kinematic(2)"),
			TEXT("Static(3)"),
			TEXT("Dynamic(4)"),
		};

		const bool bIsValid = ((int32)ObjectState >= 0) && ((int32)ObjectState < (int32)EObjectStateType::Count);
		if (ensure(bIsValid))
		{
			return StateStrings[(int32)ObjectState];
		}

		return FString::Printf(TEXT("Invalid(%d)"), (int32)ObjectState);
	}

	FString ToString(const FRotation3& V)
	{
		return FString::Format(TEXT("{0}, {1}, {2}, {3}"), { V.X, V.Y, V.Z, V.W });
	}

	FString ToString(const FRotation3f& V)
	{
		return FString::Format(TEXT("{0}, {1}, {2}, {3}"), { V.X, V.Y, V.Z, V.W });
	}

	FString ToString(const FVec3& V)
	{
		return FString::Format(TEXT("{0}, {1}, {2}"), { V.X, V.Y, V.Z });
	}

	FString ToString(const FVec3f& V)
	{
		return FString::Format(TEXT("{0}, {1}, {2}"), { V.X, V.Y, V.Z });
	}

	FString ToString(const TBitArray<>& BitArray)
	{
		FString S;
		S.Reserve(BitArray.Num());

		for (int32 BitIndex = 0; BitIndex < BitArray.Num(); ++BitIndex)
		{
			S.Append(BitArray[BitIndex] ? TEXT("1") : TEXT("0"));
		}

		return S;
	}

}