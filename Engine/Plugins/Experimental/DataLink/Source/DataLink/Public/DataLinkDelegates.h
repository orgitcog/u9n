// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"

class FDataLinkExecutor;
enum class EDataLinkExecutionResult : uint8;
struct FConstStructView;

DECLARE_DELEGATE_ThreeParams(FOnDataLinkExecutionFinished, const FDataLinkExecutor&, EDataLinkExecutionResult, FConstStructView);
