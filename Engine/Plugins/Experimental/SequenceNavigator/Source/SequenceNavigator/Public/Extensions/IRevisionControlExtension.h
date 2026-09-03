// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlModule.h"
#include "Misc/PackageName.h"
#include "NavigationToolItemType.h"
#include "UObject/Package.h"

struct FSlateBrush;

namespace UE::SequenceNavigator
{

enum class EItemRevisionControlState
{
	None                      = 0,
	SourceControlled          = 1 << 0,
	PartiallySourceControlled = 1 << 1,
};
ENUM_CLASS_FLAGS(EItemRevisionControlState)

class IRevisionControlExtension : public INavigationToolItemTypeCastable
{
public:
	UE_NAVIGATIONTOOL_INHERITS(IRevisionControlExtension, INavigationToolItemTypeCastable)

	IRevisionControlExtension(const UObject* const InObject)
	{
		QueueRevisionControlStatusUpdate(InObject);
	}

	SEQUENCENAVIGATOR_API virtual EItemRevisionControlState GetRevisionControlState() const = 0;

	SEQUENCENAVIGATOR_API virtual const FSlateBrush* GetRevisionControlStatusIcon() const = 0;

	SEQUENCENAVIGATOR_API virtual FText GetRevisionControlStatusText() const = 0;

	static void QueueRevisionControlStatusUpdate(const UObject* const InObject)
	{
		if (!InObject)
		{
			return;
		}

		UPackage* const InPackage = InObject->GetPackage();
		if (!InPackage)
		{
			return;
		}

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(InPackage->GetName()
			, FPackageName::GetAssetPackageExtension());
		ISourceControlModule::Get().QueueStatusUpdate(PackageFilename);
	}
};

} // namespace UE::SequenceNavigator
