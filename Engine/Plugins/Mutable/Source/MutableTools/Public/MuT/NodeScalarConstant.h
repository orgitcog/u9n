// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"

namespace mu
{

	/** Node returning a scalar constant value. */
	class MUTABLETOOLS_API NodeScalarConstant : public NodeScalar
	{
	public:

		float Value;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeScalarConstant() {}

	private:

		static FNodeType StaticType;

	};

}
