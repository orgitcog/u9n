// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeRange.h"

namespace mu
{

	//! Node that defines a scalar model parameter.
	class MUTABLETOOLS_API NodeScalarParameter : public NodeScalar
	{
	public:

		float DefaultValue = 0.0f;
		FString Name;
		FString UID;

		TArray<Ptr<NodeRange>> Ranges;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeScalarParameter() {}

	private:

		static FNodeType StaticType;

	};


}
