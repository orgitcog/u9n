// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "BuildMacros.h"
#include "Utils.h"

#include <setjmp.h>

namespace AutoRTFM
{

// setjmp/longjmp that doesn't do any unwinding (no C++ destructor calls, no messing with OS
// signal states - just saving/restoring CPU state). Although it's the setjmp/longjmp everyone
// knows and loves, it's exposed as a try/catch/throw API to make it less error-prone.
class FLongJump
{
public:
    FLongJump()
    {
        memset(this, 0, sizeof(*this));
    }

    template<typename TTryFunctor, typename TCatchFunctor>
    void TryCatch(const TTryFunctor& TryFunctor, const TCatchFunctor& CatchFunctor);
    
    [[noreturn]] void Throw();
    
private:
    jmp_buf JmpBuf;
    bool bIsSet;
};

template<typename TTryFunctor, typename TCatchFunctor>
void FLongJump::TryCatch(const TTryFunctor& TryFunctor, const TCatchFunctor& CatchFunctor)
{
    AUTORTFM_ASSERT(!bIsSet);
#if AUTORTFM_PLATFORM_WINDOWS
	if (!setjmp(JmpBuf))
#else
	if (!_setjmp(JmpBuf))
#endif // AUTORTFM_PLATFORM_WINDOWS
    {
        bIsSet = true;
        TryFunctor();
        bIsSet = false;
    }
    else
    {
        AUTORTFM_ASSERT(bIsSet);
        bIsSet = false;
        CatchFunctor();
    }
}

inline void FLongJump::Throw()
{
    AUTORTFM_ASSERT(bIsSet);
#if AUTORTFM_PLATFORM_WINDOWS
	longjmp(JmpBuf, 1);
#else
	_longjmp(JmpBuf, 1);
#endif // AUTORTFM_PLATFORM_WINDOWS
}

} // namespace AutoRTFM

#endif // (defined(__AUTORTFM) && __AUTORTFM)

