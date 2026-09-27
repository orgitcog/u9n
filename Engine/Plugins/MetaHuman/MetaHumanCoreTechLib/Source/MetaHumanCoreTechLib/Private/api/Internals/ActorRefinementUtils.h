// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

#include <vector>

namespace dna
{

class Reader;
class Writer;

} // namespace dna

namespace TITAN_API_NAMESPACE
{

enum class Operation
{
    Add = 0,
    Substract
};

void ApplyDNAInternal(dna::Reader* dna1, dna::Reader* dna2, dna::Writer* resultDna, Operation operation,
                      const std::vector<float>& mask);

} // namespace TITAN_API_NAMESPACE
