// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeScalar.h"
#include "MuR/Image.h"

namespace mu
{

	/** */
	class MUTABLETOOLS_API NodeImageProject : public NodeImage
	{
	public:

		Ptr<NodeProjector> Projector;
		Ptr<NodeMesh> Mesh;
		Ptr<NodeScalar> AngleFadeStart;
		Ptr<NodeScalar> AngleFadeEnd;
		Ptr<NodeImage> Image;
		Ptr<NodeImage> Mask;
		FUintVector2 ImageSize;
		uint8 Layout = 0;
		bool bIsRGBFadingEnabled = true;
		bool bIsAlphaFadingEnabled = true;
		bool bEnableTextureSeamCorrection = true;
		ESamplingMethod SamplingMethod = ESamplingMethod::Point;
		EMinFilterMethod MinFilterMethod = EMinFilterMethod::None;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeImageProject() {}

	private:

		static FNodeType StaticType;

	};

}
