// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

	/** Node that composes a new image by gathering pixel data from channels in other images. */
	class MUTABLETOOLS_API NodeImageFormat : public NodeImage
	{
	public:

		EImageFormat Format = EImageFormat::None;
		EImageFormat FormatIfAlpha = EImageFormat::None;
		Ptr<NodeImage> Source;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeImageFormat() {}

	private:

		static FNodeType StaticType;

	};


}

