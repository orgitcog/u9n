// Copyright Epic Games, Inc. All Rights Reserved.

#include <calib/Error.h>
#include "ErrorInternal.h"

#include <iostream>


static thread_local std::string lastMessage = "";
static thread_local CalibStatus lastError = CalibStatus::CALIB_OK;

void calibPrintLastError(const char* message)
{
    if (message)
    {
        std::cout << message << ": ";
    }
    std::cout << "Error code: " << lastError << ", Message: " << lastMessage << std::endl;
}

const char* calibGetLastErrorMessage() { return lastMessage.c_str(); }

CalibStatus calibGetLastErrorCode() { return lastError; }

void calibSetLastError(const std::string& errorMessage, CalibStatus error)
{
    lastMessage = errorMessage;
    lastError = error;
}
