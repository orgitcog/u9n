// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "genesplicer/Splicer.h"
#include "genesplicer/neutraljointsplicer/JointAttribute.h"

namespace gs4 {

class GeneSplicerDNAReader;
class SpliceData;

template<CalculationType CT>
class NeutralJointSplicer : public Splicer {
    public:
        using Splicer::Splicer;

        void splice(const SpliceDataInterface* spliceData, GeneSplicerDNAReader* output) override;

};

}  // namespace gs4
