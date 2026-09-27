// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Defs.h>
#include <calib/Error.h>

#include <calib/BeforeOpenCvHeaders.h>
CARBON_DISABLE_EIGEN_WARNINGS
#include <opencv2/opencv.hpp>
CARBON_RENABLE_WARNINGS
#include <calib/AfterOpenCvHeaders.h>

#include <stdio.h>
#include <string>



#define CV_CALL_CATCH(call, ExceptionType) \
        try { \
            call; \
        } \
        catch (cv::Exception&) { \
            throw ExceptionType; \
        }

#define CALIB_ASSERT(cond, message, errorCode) \
        if (!(cond)) { \
            calibSetLastError(message, errorCode); \
            return errorCode; \
        }

#define REPORT_ERROR(message, errorCode) \
        calibSetLastError(message, errorCode); \
        return errorCode;

#define CALIB_CHECK_INVALID_HANDLE(handle) CALIB_ASSERT(handle != nullptr, \
                                                        "Invalid (null) handle given for a parameter: #handle", \
                                                        CALIB_INVALID_HANDLE)
#define CALIB_CHECK_INVALID_ARGUMENT(cond) CALIB_ASSERT(cond, \
                                                        "Invalid argument: #cond", \
                                                        CALIB_INVALID_ARGUMENT)
#define CALIB_EXPECT_VALID(expected, error) \
        CALIB_ASSERT(expected.valid(), expected.getExceptionMessage<std::string>(), error)

void calibSetLastError(const std::string& errorMessage, CalibStatus error);
