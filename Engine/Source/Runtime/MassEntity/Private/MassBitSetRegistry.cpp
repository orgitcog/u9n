// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassBitSetRegistry.h"

namespace UE::Mass
{
	template<>
	FFragmentBitRegistry::TBitTypeRegistry()
		: StructTracker(FMassFragment::StaticStruct()
			, [](const UStruct* Struct)
				{
					return UE::Mass::IsA<FMassFragment>(Struct);
				})
	{	
	}

	template<>
	FTagBitRegistry::TBitTypeRegistry()
		: StructTracker(FMassTag::StaticStruct()
			, [](const UStruct* Struct)
				{
					return UE::Mass::IsA<FMassTag>(Struct);
				})
	{	
	}
}
