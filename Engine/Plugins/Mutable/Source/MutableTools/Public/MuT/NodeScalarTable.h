// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeScalar.h"
#include "MuT/Table.h"

namespace mu
{

	/** This node provides the meshes stored in the column of a table. */
	class MUTABLETOOLS_API NodeScalarTable : public NodeScalar
	{
	public:

		FString ParameterName;
		Ptr<FTable> Table;
		FString ColumnName;
		bool bNoneOption = false;
		FString DefaultRowName;

	public:

		// Node interface
		virtual const FNodeType* GetType() const override { return GetStaticType(); }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeScalarTable() {}

	private:

		static FNodeType StaticType;

	};

}
