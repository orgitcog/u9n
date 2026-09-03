// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PawnAction_Move.generated.h"

UENUM()
namespace EPawnActionMoveMode
{
	enum Type : int
	{
		UsePathfinding,
		StraightLine,
	};
}
