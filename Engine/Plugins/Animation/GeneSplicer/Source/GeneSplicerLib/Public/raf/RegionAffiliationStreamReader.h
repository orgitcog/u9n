// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "raf/Defs.h"
#include "raf/RegionAffiliationReader.h"
#include "raf/types/Aliases.h"

namespace raf {

class RAFAPI RegionAffiliationStreamReader : public RegionAffiliationReader {
    public:
        static const StatusCode IOError;
        static const StatusCode SignatureMismatchError;
        static const StatusCode VersionMismatchError;

    protected:
        virtual ~RegionAffiliationStreamReader();

    public:
        virtual void read() = 0;

};

}  // namespace raf
