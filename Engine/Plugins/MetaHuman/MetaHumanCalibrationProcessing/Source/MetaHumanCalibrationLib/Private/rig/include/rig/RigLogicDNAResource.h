// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>

#include <memory>
#include <string>

namespace dna
{

class Reader;

} // namespace dna

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Wrapper to load RigLogic DNA resources.
 */
class RigLogicDNAResource
{
public:
    RigLogicDNAResource();
    ~RigLogicDNAResource();
    RigLogicDNAResource(RigLogicDNAResource&&);
    RigLogicDNAResource(const RigLogicDNAResource&) = delete;
    RigLogicDNAResource& operator=(RigLogicDNAResource&&);
    RigLogicDNAResource& operator=(const RigLogicDNAResource&) = delete;

    /**
     * Read the DNA file from \p dnaFile.
     * @param [in] retain  Cache the resource in memory for subsequent accesses. Only necessary if the same DNA file is being loaded multiple times.
     */
    static std::shared_ptr<const RigLogicDNAResource> LoadDNA(const std::string& dnaFile, bool retain);

    /**
     * Returns the pointer to the underlying RL4 BinaryStreamReader. Only valid as long as the instance is alive.
     */
    dna::Reader* Stream() const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
