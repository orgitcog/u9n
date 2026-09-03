// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef WITH_AITESTSUITE
#include "TestableEnsures.h"
#else
#define testableEnsureMsgf ensureMsgf
#define testableCheckf checkf
#define testableCheckfReturn(InExpression, ReturnValue, InFormat, ... ) checkf(InExpression, InFormat, ##__VA_ARGS__)
#endif 
