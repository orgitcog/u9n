// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseNodes/BinaryOpLambdaNode.h"
#include "GeometryFlowTypes.h"
#include "GeometryFlowNode.h"


namespace UE
{
namespace GeometryFlow
{

	template<typename T, int StorageTypeIdentifier>
	class TBinaryOpAddNode : public TBinaryOpLambdaNode<T, StorageTypeIdentifier>
	{
	public:
		TBinaryOpAddNode() : TBinaryOpLambdaNode<T, StorageTypeIdentifier>([](const T& A, const T& B) { return A + B; })
		{}
	};

	class FAddFloatNode : public TBinaryOpAddNode<float, (int)EDataTypes::Float>
	{
		static constexpr int NodeVersion = 1;
		GEOMETRYFLOW_NODE_INTERNAL(FAddFloatNode, NodeVersion, FNode)
	};


}	// end namespace GeometryFlow
}	// end namespace UE
