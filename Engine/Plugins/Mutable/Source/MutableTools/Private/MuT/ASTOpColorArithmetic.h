// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;

	/** */
	class ASTOpColorArithmetic final : public ASTOp
	{
	public:

		ASTChild A;
		ASTChild B;
		uint16 Operation=0;

	public:

		ASTOpColorArithmetic();
		ASTOpColorArithmetic(const ASTOpColorArithmetic&) = delete;
		~ASTOpColorArithmetic();

		// ASTOp interface
		virtual EOpType GetOpType() const override { return EOpType::CO_ARITHMETIC; }
		virtual uint64 Hash() const override;
		virtual bool IsEqual(const ASTOp&) const override;
		virtual Ptr<ASTOp> Clone(MapChildFuncRef) const override;
		virtual void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		virtual void Link(FProgram&, FLinkerOptions*) override;
	};

}
