// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeScalar.h"

namespace mu
{

	/** Node that multiplies the colors of an image, channel by channel. */
	class MUTABLETOOLS_API NodeImageBinarise : public NodeImage
	{
	public:

		Ptr<NodeImage> Base;
		Ptr<NodeScalar> Threshold;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeImageBinarise() {}

	private:

		static FNodeType StaticType;

	};

}
