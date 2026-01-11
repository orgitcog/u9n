// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"

namespace mu
{

	/** Calculate the luminance of an image into a new single - channel image. */
	class MUTABLETOOLS_API NodeImageLuminance : public NodeImage
	{
	public:

		Ptr<NodeImage> Source;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeImageLuminance() {}

	private:

		static FNodeType StaticType;

	};

}
