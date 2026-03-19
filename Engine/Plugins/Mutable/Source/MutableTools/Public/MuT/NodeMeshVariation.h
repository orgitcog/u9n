// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"

namespace mu
{

	/** */
	class MUTABLETOOLS_API NodeMeshVariation : public NodeMesh
    {
	public:

		Ptr<NodeMesh> DefaultMesh;

		struct FVariation
		{
			Ptr<NodeMesh> Mesh;
			FString Tag;
		};

		TArray<FVariation> Variations;

    public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

    protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeMeshVariation() {}

    private:

		static FNodeType StaticType;

    };

}

