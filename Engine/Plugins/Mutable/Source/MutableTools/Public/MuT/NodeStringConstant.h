// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeString.h"


namespace mu
{

	/** Node returning a string constant value. */
	class MUTABLETOOLS_API NodeStringConstant : public NodeString
	{
	public:

		FString Value;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeStringConstant() {}

	private:

		static FNodeType StaticType;

	};



}
