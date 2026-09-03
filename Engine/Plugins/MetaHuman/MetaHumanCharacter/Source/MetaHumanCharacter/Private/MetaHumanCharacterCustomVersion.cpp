// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterCustomVersion.h"

#include "Serialization/CustomVersion.h"

// Unique serialization id for MetaHumanCharacter .
const FGuid FMetaHumanCharacterCustomVersion::GUID(0xC5DB848, 0x21757CE, 0xC42843E, 0xCA16973);

FCustomVersionRegistration GRegisterMetaHumanCharacterCustomVersion(FMetaHumanCharacterCustomVersion::GUID, FMetaHumanCharacterCustomVersion::LatestVersion, TEXT("MetaHumanCharacter"));