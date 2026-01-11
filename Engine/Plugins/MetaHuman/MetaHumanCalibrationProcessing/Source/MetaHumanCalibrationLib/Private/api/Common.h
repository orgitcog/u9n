// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Log.h>
#include <status/Provider.h>
#include <carbon/common/External.h>

#define TITAN_RESET_ERROR if (!sc::StatusProvider::isOk()) sc::StatusProvider::set({ 0, "" })
#define TITAN_SET_ERROR(errorCode, message) sc::StatusProvider::set({ errorCode, message })

#define TITAN_CHECK_OR_RETURN(condition, returnValue, ...) if (!(condition)) { \
            TITAN_SET_ERROR(-1, TITAN_NAMESPACE::fmt::format(__VA_ARGS__).c_str()); \
            LOG_ERROR(__VA_ARGS__); return returnValue; \
}

#define TITAN_HANDLE_EXCEPTION(...) { \
            TITAN_SET_ERROR(-1, TITAN_NAMESPACE::fmt::format(__VA_ARGS__).c_str()); \
            LOG_ERROR(__VA_ARGS__); return false; \
}
