// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeScalar.h"
#include "MuR/Skeleton.h"

namespace mu
{

	/** Node that morphs a base mesh with one weighted target. */
	class MUTABLETOOLS_API NodeMeshMorph : public NodeMesh
	{
	public:

		Ptr<NodeScalar> Factor;
		Ptr<NodeMesh> Base;
		Ptr<NodeMesh> Morph;

		bool bReshapeSkeleton = false;
		bool bReshapePhysicsVolumes = false;

		TArray<FBoneName> BonesToDeform;
		TArray<FBoneName> PhysicsToDeform;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeMeshMorph() {}

	private:

		static FNodeType StaticType;

	};

}
