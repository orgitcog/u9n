// Copyright Epic Games, Inc. All Rights Reserved.


#include "GenePoolAssetFactory.h"
#include "GenePoolAsset.h"

UGenePoolAssetFactory::UGenePoolAssetFactory()
{
	SupportedClass = UGenePoolAsset::StaticClass();
	bCreateNew = true;

}

UObject* UGenePoolAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UGenePoolAsset>(InParent, Class, Name, Flags, Context);
}
