// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"

namespace mu
{

    /** Node that applies a pose to a mesh, baking it into the vertex data. */
    class MUTABLETOOLS_API NodeMeshApplyPose : public NodeMesh
	{
	public:

		Ptr<NodeMesh> Base;
		Ptr<NodeMesh> Pose;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden.Manage with the Ptr<> template. */
		~NodeMeshApplyPose() {}

	private:

		static FNodeType StaticType;

	};

}
