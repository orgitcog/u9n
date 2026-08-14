// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeString.h"
#include "MuT/NodeRange.h"

namespace mu
{

	/** Node that defines a string model parameter. */
	class MUTABLETOOLS_API NodeStringParameter : public NodeString
	{
	public:

		FString DefaultValue;
		FString Name;
		FString UID;

		TArray<Ptr<NodeRange>> Ranges;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeStringParameter() {}

	private:

		static FNodeType StaticType;

	};


}
