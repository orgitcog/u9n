// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "genesplicer/Splicer.h"

namespace gs4 {

class GeneSplicerDNAReader;
class SpliceData;

template<CalculationType CT>
class JointBehaviorSplicer : public Splicer {
    public:
        using Splicer::Splicer;

        void splice(const SpliceDataInterface* spliceData, GeneSplicerDNAReader* output) override;

};

}  // namespace gs4
