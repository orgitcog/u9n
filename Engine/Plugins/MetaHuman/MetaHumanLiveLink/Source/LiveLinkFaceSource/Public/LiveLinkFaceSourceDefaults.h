// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "LiveLinkFaceSourceDefaults.generated.h"



UCLASS(config = Editor, defaultconfig)
class LIVELINKFACESOURCE_API ULiveLinkFaceSourceDefaults : public UObject
{
public:

	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category = "Subject")
	bool bHeadOrientation = true;

	UPROPERTY(config, EditAnywhere, Category = "Subject")
	bool bHeadTranslation = true;
};