// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"

namespace mu
{

	/** */
    class MUTABLETOOLS_API NodeScalarVariation : public NodeScalar
    {
	public:

		Ptr<NodeScalar> DefaultScalar;

		struct FVariation
		{
			Ptr<NodeScalar> Scalar;
			FString Tag;
		};

		TArray<FVariation> Variations;

    public:
 
		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

    protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeScalarVariation() {}

    private:

		static FNodeType StaticType;

	};

}
