// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"

namespace mu
{

	/** This node applies a layer blending effect on a base image using a mask and a blended image. */
	class MUTABLETOOLS_API NodeImageLayer : public NodeImage
	{
	public:

		Ptr<NodeImage> Base;
		Ptr<NodeImage> Mask;
		Ptr<NodeImage> Blended;
		EBlendType Type;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeImageLayer() {}

	private:

		static FNodeType StaticType;

	};


}
