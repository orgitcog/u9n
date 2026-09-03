// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#pragma pack(push, 8)

#ifndef EMRTC_STATIC

    #ifdef _WIN32

        #ifdef EMRTC_LIBRARY_IMPL
            #define EPICRTC_API __declspec(dllexport)
        #else
            #define EPICRTC_API __declspec(dllimport)
        #endif

    #else  // _WIN32

        #if __has_attribute(visibility) && defined(EMRTC_LIBRARY_IMPL)
            #define EPICRTC_API __attribute__((visibility("default")))
        #endif

    #endif  // _WIN32

#endif  // EMRTC_STATIC

#ifndef EPICRTC_API
    #define EPICRTC_API
#endif

#pragma pack(pop)
