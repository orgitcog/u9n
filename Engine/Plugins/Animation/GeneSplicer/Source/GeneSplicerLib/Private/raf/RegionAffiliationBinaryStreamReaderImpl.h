// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "raf/ReaderImpl.h"
#include "raf/RegionAffiliationBinaryStreamReader.h"
#include "raf/TypeDefs.h"

namespace raf {

class RegionAffiliationBinaryStreamReaderImpl : public ReaderImpl<RegionAffiliationBinaryStreamReader> {
    public:
        RegionAffiliationBinaryStreamReaderImpl(BoundedIOStream* stream_, MemoryResource* memRes_);

        void read() override;

    private:
        static sc::StatusProvider status;
        BoundedIOStream* stream;

};

}  // namespace raf
