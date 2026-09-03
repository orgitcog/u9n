// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StormSyncCommonTypes.h"
#include "StormSyncPackageDescriptor.h"
#include "Tasks/IStormSyncImportTask.h"

/** Import files from Buffer implementation for tasks that need delayed execution */
class FStormSyncImportBufferTask final : public IStormSyncImportSubsystemTask
{
public:
	explicit FStormSyncImportBufferTask(const FStormSyncPackageDescriptor& InPackageDescriptor, const FStormSyncArchivePtr& InArchive)
		: PackageDescriptor(InPackageDescriptor)
		, Archive(InArchive)
	{
	}

	virtual void Run() override;

private:
	/** Metadata info about buffer being extracted */
	FStormSyncPackageDescriptor PackageDescriptor;
	
	/** Archive to import */
	FStormSyncArchivePtr Archive;
};