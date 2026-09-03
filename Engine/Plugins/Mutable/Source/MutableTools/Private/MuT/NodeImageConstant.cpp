// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Serialisation.h"


namespace mu
{

	void NodeImageConstant::SetValue(TSharedPtr<const FImage> Value)
	{
		Proxy = new TResourceProxyMemory<FImage>(Value);
	}


	void NodeImageConstant::SetValue(Ptr<TResourceProxy<FImage>> InProxy)
	{
		Proxy = InProxy;
	}

}
