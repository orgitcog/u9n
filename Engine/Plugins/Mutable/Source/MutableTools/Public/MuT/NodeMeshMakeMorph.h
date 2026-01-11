// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"


namespace mu
{

	/** This node removes from a mesh A all the faces that are also part of a mesh B. */
    class MUTABLETOOLS_API NodeMeshMakeMorph : public NodeMesh
	{
	public:

		Ptr<NodeMesh> Base;
		Ptr<NodeMesh> Target;

		bool bOnlyPositionAndNormal = false;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeMeshMakeMorph() {}

	private:

		static FNodeType StaticType;

	};

}
