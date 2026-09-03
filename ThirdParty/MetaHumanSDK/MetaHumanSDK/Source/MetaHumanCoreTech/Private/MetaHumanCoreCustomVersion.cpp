// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCoreCustomVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FMetaHumanCoreCustomVersion::GUID(0x9A529652, 0xB6F785F7, 0x4B868FB5, 0x744F83E3);

// Register the custom version with core
FCustomVersionRegistration GRegisterMetaHumanCustomVersion(FMetaHumanCoreCustomVersion::GUID, FMetaHumanCoreCustomVersion::LatestVersion, TEXT("MetaHumanCoreVer"));
