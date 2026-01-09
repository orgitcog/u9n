// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"

namespace mu
{

	/** Node that inverts the colors of an image, channel by channel. */
	class MUTABLETOOLS_API NodeImageInvert : public NodeImage
	{
	public:

		Ptr<NodeImage> Base;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeImageInvert() {}

	private:

		static FNodeType StaticType;

	};

}
