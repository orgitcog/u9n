// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"

namespace mu
{

    /** */
    class MUTABLETOOLS_API NodeImageVariation : public NodeImage
    {
	public:

		Ptr<NodeImage> DefaultImage;

		struct FVariation
		{
			Ptr<NodeImage> Image;
			FString Tag;
		};

		TArray<FVariation> Variations;

    public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

    protected:

        /** Forbidden. Manage with the Ptr<> template. */
		~NodeImageVariation() {}

    private:

		static FNodeType StaticType;
	
	};

}

