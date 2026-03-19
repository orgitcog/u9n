// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "Containers/Array.h"
#include "Containers/Map.h"

namespace mu
{

	// Forward definitions
	class FMesh;
	class NodeMesh;
	typedef Ptr<NodeMesh> NodeMeshPtr;
	typedef Ptr<const NodeMesh> NodeMeshPtrConst;


    /** Base class of any node that outputs a mesh. */
	class MUTABLETOOLS_API NodeMesh : public Node
	{
	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		// Forbidden. Manage with the Ptr<> template.
		inline ~NodeMesh() {}

	private:

		static FNodeType StaticType;

	};


}

