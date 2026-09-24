// Copyright Epic Games, Inc. All Rights Reserved.

#include "genesplicer/Splicer.h"

namespace gs4 {

Splicer::~Splicer() = default;

Splicer::Splicer(MemoryResource* memRes_) :
    memRes{memRes_} {
}

}  // namespace gs4
