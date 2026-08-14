// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/RigLogicDNAResource.h>

#include <riglogic/RigLogic.h>

#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
#include <pma/PolyAllocator.h>
#include <carbon/common/External.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

using pma::MemoryResource;
using pma::PolyAllocator;

struct RigLogicDNAResource::Private
{
    pma::ScopedPtr<dna::BinaryStreamReader> stream;
};


RigLogicDNAResource::RigLogicDNAResource() : m(new Private)
{}

RigLogicDNAResource::~RigLogicDNAResource() = default;
RigLogicDNAResource::RigLogicDNAResource(RigLogicDNAResource&&) = default;
RigLogicDNAResource& RigLogicDNAResource::operator=(RigLogicDNAResource&&) = default;

std::shared_ptr<const RigLogicDNAResource> RigLogicDNAResource::LoadDNA(const std::string& dnaFile, bool retain)
{
    if (!std::filesystem::exists(dnaFile))
    {
        CARBON_CRITICAL("dna file \"{}\" does not exist", dnaFile);
    }

    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    static std::map<std::string, std::shared_ptr<const RigLogicDNAResource>> allStreams;

    auto it = allStreams.find(dnaFile);
    if (it != allStreams.end())
    {
        return it->second;
    }
    else
    {
        pma::ScopedPtr<dna::FileStream> stream = pma::makeScoped<dna::FileStream>(dnaFile.c_str(), dna::FileStream::AccessMode::Read,
                                                                                  dna::FileStream::OpenMode::Binary);
        pma::ScopedPtr<dna::BinaryStreamReader> reader = pma::makeScoped<dna::BinaryStreamReader>(stream.get(), dna::DataLayer::All);
        reader->read();

        PolyAllocator<RigLogicDNAResource> polyAllocatorRigLogicDNA{ MEM_RESOURCE };
        std::shared_ptr<RigLogicDNAResource> newStream = std::allocate_shared<RigLogicDNAResource>(polyAllocatorRigLogicDNA);
        newStream->m->stream = std::move(reader);

        if (retain)
        {
            allStreams[dnaFile] = newStream;
        }

        return newStream;
    }
}

dna::Reader* RigLogicDNAResource::Stream() const
{
    return m->stream.get();
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
