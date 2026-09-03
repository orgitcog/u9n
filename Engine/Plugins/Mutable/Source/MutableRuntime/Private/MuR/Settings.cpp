// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/Settings.h"

namespace mu
{

    void FSettings::SetProfile( bool bEnabled )
    {
        bProfile = bEnabled;
    }


    void FSettings::SetWorkingMemoryBytes( uint64 Bytes )
    {
        WorkingMemoryBytes = Bytes;
    }


    void FSettings::SetImageCompressionQuality( int32 Quality )
    {
        ImageCompressionQuality = Quality;
    }

}
