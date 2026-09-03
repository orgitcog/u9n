// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Interface.h"
#include "MovieSceneSection.h"
#include "IMovieSceneChannelDecoration.generated.h"

struct FMovieSceneChannelProxyData;

UINTERFACE(MinimalAPI)
class UMovieSceneChannelDecoration : public UInterface
{
public:
	GENERATED_BODY()
};


/** 
 * Optional decoration that can be added to sections to add channels
 */
class IMovieSceneChannelDecoration
{
public:
	GENERATED_BODY()


	/**
	 * Called to add channels to the channel proxy
	 */
	virtual EMovieSceneChannelProxyType PopulateChannelProxy(FMovieSceneChannelProxyData& OutProxyData)
	{
		return EMovieSceneChannelProxyType::Static;
	}
};

