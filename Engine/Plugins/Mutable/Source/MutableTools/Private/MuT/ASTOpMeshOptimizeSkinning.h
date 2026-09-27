// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;

	//---------------------------------------------------------------------------------------------
	class ASTOpMeshOptimizeSkinning final : public ASTOp
	{
	public:

		//! Mesh that will have the skinning optimized.
		ASTChild Source;

	public:

		ASTOpMeshOptimizeSkinning();
		ASTOpMeshOptimizeSkinning(const ASTOpMeshOptimizeSkinning&) = delete;
		~ASTOpMeshOptimizeSkinning() override;

		// ASTOp interface
		virtual EOpType GetOpType() const override { return EOpType::ME_OPTIMIZESKINNING; }
		virtual uint64 Hash() const override;
		virtual void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		virtual bool IsEqual(const ASTOp&) const override;
		virtual Ptr<ASTOp> Clone(MapChildFuncRef) const override;
		virtual void Link(FProgram&, FLinkerOptions*) override;
		virtual Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const override;
		virtual FSourceDataDescriptor GetSourceDataDescriptor(FGetSourceDataDescriptorContext*) const override;
	};


}

