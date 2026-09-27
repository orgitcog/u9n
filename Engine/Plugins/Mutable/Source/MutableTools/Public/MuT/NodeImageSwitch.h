// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"

namespace mu
{

	//! This node selects an output image from a set of input images based on a parameter.
	class MUTABLETOOLS_API NodeImageSwitch : public NodeImage
	{
	public:

		Ptr<NodeScalar> Parameter;

		TArray<Ptr<NodeImage>> Options;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeImageSwitch() {}

	private:

		static FNodeType StaticType;

	};

}
