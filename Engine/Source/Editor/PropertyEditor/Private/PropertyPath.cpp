// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyPath.h"

#include "UObject/PropertyPathName.h"
#include "UObject/PropertyTypeName.h"

TArray<UE::FPropertyPathName> FPropertyPath::ToPropertyPathName() const
{
	using namespace UE;

	TArray<FPropertyPathName> PathNames;
	FPropertyPathName PathName;

	for (const FPropertyInfo& Info : Properties)
	{
		FPropertyTypeNameBuilder TypeName;
		Info.Property->SaveTypeName(TypeName);
		PathName.Push({Info.Property->GetFName(), TypeName.Build(), Info.ArrayIndex});

		if (Info.Property->IsA<FObjectPropertyBase>())
		{
			PathNames.Emplace(MoveTemp(PathName));
			PathName.Reset();
		}
	}

	if (!PathName.IsEmpty())
	{
		PathNames.Emplace(MoveTemp(PathName));
	}

	return PathNames;
}
