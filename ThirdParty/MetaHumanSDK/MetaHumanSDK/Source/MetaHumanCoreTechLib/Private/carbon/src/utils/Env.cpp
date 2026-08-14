// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/utils/Env.h>
#include <carbon/common/Log.h>

#include <cstdlib>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

bool SetEnv(const char* varname, const char* value)
{
    #ifdef _MSC_VER
    const int err = _putenv_s(varname, value);
    #else
    const int err = setenv(varname, value, /*overwrite=*/1);
    #endif
    if (err)
    {
        LOG_ERROR("error setting environment variable \"{}\"=\"{}\": error {}", varname, value, err);
    }
    return (err == 0);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
