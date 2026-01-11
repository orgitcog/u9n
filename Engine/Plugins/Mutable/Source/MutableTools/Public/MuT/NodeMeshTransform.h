// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"
#include "Math/Matrix.h"

namespace mu
{

    /** This node applies a geometric transform represented by a 4x4 matrix to a mesh. */
    class MUTABLETOOLS_API NodeMeshTransform : public NodeMesh
	{
	public:

		Ptr<NodeMesh> Source;
		FMatrix44f Transform;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeMeshTransform() {}

	private:

		static FNodeType StaticType;

	};

}
