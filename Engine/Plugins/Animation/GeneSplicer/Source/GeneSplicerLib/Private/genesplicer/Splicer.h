// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "genesplicer/CalculationType.h"
#include "genesplicer/splicedata/SpliceDataImpl.h"
#include "genesplicer/types/Aliases.h"
#include "genesplicer/types/BlockStorage.h"

#include <cstddef>

namespace gs4 {

class GeneSplicerDNAReader;
class SpliceData;

class Splicer {
    public:
        explicit Splicer(MemoryResource* memRes);
        virtual void splice(const SpliceDataInterface* spliceData, GeneSplicerDNAReader* output) = 0;
        virtual ~Splicer();

    protected:
        MemoryResource* memRes;

};

}  // namespace gs4
