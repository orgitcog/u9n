// Copyright Epic Games, Inc. All Rights Reserved.

#include <raf/RegionAffiliationStreamWriter.h>

namespace raf {

VertexRegionAffiliationWriter::~VertexRegionAffiliationWriter() = default;
JointRegionAffiliationWriter::~JointRegionAffiliationWriter() = default;
RegionAffiliationWriter::~RegionAffiliationWriter() = default;
RegionAffiliationStreamWriter::~RegionAffiliationStreamWriter() = default;

const sc::StatusCode RegionAffiliationStreamWriter::IOError{1017, "%s"};

}  // namespace raf
