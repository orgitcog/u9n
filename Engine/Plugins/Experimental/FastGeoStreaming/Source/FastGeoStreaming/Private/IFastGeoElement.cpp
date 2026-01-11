// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFastGeoElement.h"

uint32 FFastGeoElementType::NextUniqueID = 0;
const FFastGeoElementType IFastGeoElement::Type;

const FFastGeoElementType FFastGeoElementType::Invalid = FFastGeoElementType(FFastGeoElementType::EInitMode::Invalid);