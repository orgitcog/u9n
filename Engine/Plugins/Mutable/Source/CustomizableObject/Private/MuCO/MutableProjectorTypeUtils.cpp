// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/MutableProjectorTypeUtils.h"

#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuR/Parameters.h"

ECustomizableObjectProjectorType ProjectorUtils::GetEquivalentProjectorType (mu::EProjectorType ProjectorType)
{
	// Translate projector type from Mutable Core enum type to CO enum type
	switch (ProjectorType)
	{
	case mu::EProjectorType::Planar:
		return ECustomizableObjectProjectorType::Planar;
		
	case mu::EProjectorType::Cylindrical:
		return ECustomizableObjectProjectorType::Cylindrical;
		
	case mu::EProjectorType::Wrapping:
		return ECustomizableObjectProjectorType::Wrapping;
		
	case mu::EProjectorType::Count:
	default:
		checkNoEntry();
		return ECustomizableObjectProjectorType::Planar;
	}
}


mu::EProjectorType ProjectorUtils::GetEquivalentProjectorType (ECustomizableObjectProjectorType ProjectorType)
{
	if (GetEquivalentProjectorType(mu::EProjectorType::Planar) == ProjectorType)
	{
		return mu::EProjectorType::Planar;
	}
	if (GetEquivalentProjectorType(mu::EProjectorType::Wrapping) == ProjectorType)
	{
		return mu::EProjectorType::Wrapping;
	}
	if (GetEquivalentProjectorType(mu::EProjectorType::Cylindrical) == ProjectorType)
	{
		return mu::EProjectorType::Cylindrical;
	}

	checkNoEntry();
	return mu::EProjectorType::Count;		// Invalid 
}
