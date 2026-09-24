// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <dna/DataLayer.h>
#include <dna/BinaryStreamReader.h>
#include <dna/BinaryStreamWriter.h>
#include <dna/types/Aliases.h>
#include <dna/types/Vector3.h>
#include <pma/MemoryResource.h>
#include <pma/ScopedPtr.h>
#include <pma/resources/AlignedMemoryResource.h>
#include <pma/resources/DefaultMemoryResource.h>
#include <raf/RegionAffiliationBinaryStreamReader.h>
#include <raf/RegionAffiliationBinaryStreamWriter.h>
#include <raf/RegionAffiliationJSONStreamReader.h>
#include <raf/RegionAffiliationJSONStreamWriter.h>
#include <status/Status.h>
#include <status/StatusCode.h>
#include <trio/Stream.h>
#include <trio/streams/FileStream.h>

namespace gs4 {

using sc::Status;
using sc::StatusCode;
using trio::BoundedIOStream;
using trio::FileStream;
using trio::MemoryMappedFileStream;
using trio::MemoryStream;
using dna::BinaryStreamReader;
using dna::BinaryStreamWriter;
using dna::DataLayer;
using dna::Reader;
using dna::StringView;
using dna::UnknownLayerPolicy;
using dna::Vector3;
using dna::Writer;
using raf::JointRegionAffiliationReader;
using raf::VertexRegionAffiliationReader;
using raf::RegionAffiliationReader;
using raf::RegionAffiliationBinaryStreamReader;
using raf::RegionAffiliationJSONStreamReader;
using raf::RegionAffiliationStreamReader;
using raf::JointRegionAffiliationWriter;
using raf::VertexRegionAffiliationWriter;
using raf::RegionAffiliationWriter;
using raf::RegionAffiliationStreamWriter;
using raf::RegionAffiliationBinaryStreamWriter;
using raf::RegionAffiliationJSONStreamWriter;

template<typename T>
using ArrayView = dna::ArrayView<T>;

template<typename T>
using ConstArrayView = dna::ConstArrayView<T>;

using namespace pma;

}  // namespace gs4
