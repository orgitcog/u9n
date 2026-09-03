// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeComponent.h"

namespace mu
{

	/** This node modifies a node of the parent object of the object that this node belongs to.
	* It allows to extend, cut and morph the parent component's meshes.
	* It also allows to patch the parent component's textures.
	*/
	class MUTABLETOOLS_API NodeComponentEdit : public NodeComponent
	{
	public:

		Ptr<NodeComponent> Parent;

	public:

		// Node interface
		virtual const FNodeType* GetType() const override { return GetStaticType(); }
		static const FNodeType* GetStaticType() { return &StaticType; }

        // NodeComponent interface
		virtual const class NodeComponentNew* GetParentComponentNew() const override;

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeComponentEdit() {}

	private:

		static FNodeType StaticType;

	};

}
