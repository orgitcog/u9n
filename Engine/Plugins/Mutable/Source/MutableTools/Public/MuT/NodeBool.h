// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeRange.h"


namespace mu
{

	/** Base class of any node that outputs a Bool value. */
	class MUTABLETOOLS_API NodeBool : public Node
	{
	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		inline ~NodeBool() {}

	private:

		static FNodeType StaticType;

	};


	/** Node returning a Bool constant value. */
	class MUTABLETOOLS_API NodeBoolConstant : public NodeBool
	{
	public:

		bool Value = false;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeBoolConstant() {}

	private:

		static FNodeType StaticType;

	};


	/** Node that defines a Bool model parameter. */
	class MUTABLETOOLS_API NodeBoolParameter : public NodeBool
	{
	public:

		bool DefaultValue;
		FString Name;
		FString UID;

		TArray<Ptr<NodeRange>> Ranges;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeBoolParameter() {}

	private:

		static FNodeType StaticType;

	};


	/** Node that returns the oposite of the input value. */
	class MUTABLETOOLS_API NodeBoolNot : public NodeBool
	{
	public:

		Ptr<NodeBool> Source;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeBoolNot() {}

	private:

		static FNodeType StaticType;

	};


	/** */
	class MUTABLETOOLS_API NodeBoolAnd : public NodeBool
	{
	public:

		Ptr<NodeBool> A;
		Ptr<NodeBool> B;

	public:

		// Node Interface
		virtual const FNodeType* GetType() const override { return &StaticType; }
		static const FNodeType* GetStaticType() { return &StaticType; }

	protected:

		/** Forbidden. Manage with the Ptr<> template. */
		~NodeBoolAnd() {}

	private:

		static FNodeType StaticType;

	};


}
