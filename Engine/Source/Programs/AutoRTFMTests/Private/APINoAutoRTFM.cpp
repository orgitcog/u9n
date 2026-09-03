// Copyright Epic Games, Inc. All Rights Reserved.

#include "API.h"
#include "AutoRTFM.h"

UE_AUTORTFM_NOAUTORTFM int NoAutoRTFM::DoSomethingC(int I)
{
    return I + 13;
}

UE_AUTORTFM_NOAUTORTFM int NoAutoRTFM::DoSomethingInTransactionC(int I)
{
    return I + 42;
}

UE_AUTORTFM_NOAUTORTFM int NoAutoRTFM::DoSomethingCpp(int I)
{
    return I + 13;
}

UE_AUTORTFM_NOAUTORTFM int NoAutoRTFM::DoSomethingInTransactionCpp(int I)
{
    return I + 42;
}
