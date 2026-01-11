// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeLayout.h"

namespace mu
{

	/** */
	class MUTABLETOOLS_API NodeMeshParameter : public NodeMesh
	{
	public:

		/** Full asset name for the default value of the parameter. */
		FName DefaultValue;

		/** */
		TArray<Ptr<NodeLayout>> Layouts;

		/** */
		uint8 SectionIndex = 0;

		/** Name of the parameter */
		FString Name;

		/** User provided ID to identify the parameter. */
		FString UID;

		/** Ranges for the parameter in case it is multidimensional. */
		TArray<Ptr<NodeRange>> Ranges;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeMeshParameter() {}

	private:

		static FNodeType StaticType;

	};

}
