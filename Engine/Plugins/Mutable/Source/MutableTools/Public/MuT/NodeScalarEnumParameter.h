// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalar.h"

namespace mu
{

	/** Node that defines a scalar model parameter to be selected from a set of named values. */
	class MUTABLETOOLS_API NodeScalarEnumParameter : public NodeScalar
	{
	public:

		FString Name;
		FString UID;
		int32 DefaultValue = 0;

		struct FOption
		{
			FString Name;
			float Value = 0.0f;
		};

		TArray<FOption> Options;

		TArray<Ptr<NodeRange>> Ranges;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeScalarEnumParameter() {}

	private:

		static FNodeType StaticType;

	};

}
