// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

typedef enum
{
    CALIB_OK = 0,
    CALIB_ERROR = -1,
    CALIB_INVALID_ARGUMENT = 1,
    CALIB_INVALID_HANDLE = 2,
    CALIB_DETECT_PATTERN_FAILED = 3
} CalibStatus;

void calibPrintLastError(const char* message);

const char* calibGetLastErrorMessage();

CalibStatus calibGetLastErrorCode();
