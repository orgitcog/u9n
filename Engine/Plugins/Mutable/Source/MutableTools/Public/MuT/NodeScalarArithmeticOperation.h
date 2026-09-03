// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"

namespace mu
{

    /** Perform an arithmetic operation between two scalars. */
    class MUTABLETOOLS_API NodeScalarArithmeticOperation : public NodeScalar
	{
	public:

		/** Possible arithmetic operations. */
		typedef enum
		{
			AO_ADD = 0,
			AO_SUBTRACT,
			AO_MULTIPLY,
			AO_DIVIDE,
			_AO_COUNT
		} EOperation;

		EOperation Operation;
		Ptr<NodeScalar> A;
		Ptr<NodeScalar> B;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeScalarArithmeticOperation() {}

	private:

		static FNodeType StaticType;

	};

}
