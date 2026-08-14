// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectChooser_Asset.h"

UObject* FAssetChooser::ChooseObject(FChooserEvaluationContext& Context) const
{
	return Asset;
}

FObjectChooserBase::EIteratorStatus FAssetChooser::IterateObjects(FObjectChooserIteratorCallback Callback) const
{
	return Callback.Execute(Asset);
}

void FSoftAssetChooser::ChooseObject(FChooserEvaluationContext& Context, TSoftObjectPtr<UObject>& Result) const
{
	Result = Asset.ToSoftObjectPath();
}

UObject* FSoftAssetChooser::ChooseObject(FChooserEvaluationContext& Context) const
{
	return Asset.Get();
}

FObjectChooserBase::EIteratorStatus FSoftAssetChooser::IterateObjects(FObjectChooserIteratorCallback Callback) const
{
	return Callback.Execute(Asset.LoadSynchronous());
}
