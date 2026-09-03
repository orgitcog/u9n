// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDataLinkSinkProvider.h"
#include "UObject/Object.h"
#include "DataLinkSinkObject.generated.h"

struct FDataLinkSink;

UCLASS(MinimalAPI, BlueprintType)
class UDataLinkSinkObject : public UObject, public IDataLinkSinkProvider
{
	GENERATED_BODY()

public:
	UDataLinkSinkObject();

	UFUNCTION(BlueprintCallable, Category="Data Link")
	void ResetSink();

	//~ Begin IDataLinkSinkProvider
	virtual TSharedPtr<FDataLinkSink> GetSink() const override;
	virtual const UDataLinkSinkObject* GetSinkObject_Implementation() const override;
	//~ End IDataLinkSinkProvider

private:
	TSharedPtr<FDataLinkSink> Sink;
};
