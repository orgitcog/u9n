// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanWardrobeItemFactory.h"

#include "MetaHumanWardrobeItem.h"

UMetaHumanWardrobeItemFactory::UMetaHumanWardrobeItemFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMetaHumanWardrobeItem::StaticClass();
}

UObject* UMetaHumanWardrobeItemFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* Context, FFeedbackContext* Warn)
{
	UMetaHumanWardrobeItem* NewWardrobeItem = NewObject<UMetaHumanWardrobeItem>(InParent, InClass, InName, InFlags | RF_Transactional);

	return NewWardrobeItem;
}
