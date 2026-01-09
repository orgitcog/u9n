// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "raf/RegionAffiliationJSONStreamWriter.h"
#include "raf/TypeDefs.h"
#include "raf/WriterImpl.h"

namespace raf {

class RegionAffiliationJSONStreamWriterImpl : public WriterImpl<RegionAffiliationJSONStreamWriter> {
    public:
        RegionAffiliationJSONStreamWriterImpl(BoundedIOStream* stream_, std::uint32_t indentWidth_, MemoryResource* memRes_);

        void write() override;

    private:
        static sc::StatusProvider status;

        BoundedIOStream* stream;
        std::uint32_t indentWidth;

};

}  // namespace raf
