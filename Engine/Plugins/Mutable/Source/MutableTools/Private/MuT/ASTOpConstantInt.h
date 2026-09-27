// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
	struct FProgram;

	class ASTOpConstantInt final : public ASTOp
	{
	public:

		int32 Value = 0;

	public:

		ASTOpConstantInt(int Value = 0);

		virtual EOpType GetOpType() const override { return EOpType::NU_CONSTANT; }
		virtual uint64 Hash() const override;
		virtual void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		virtual bool IsEqual(const ASTOp&) const override;
		virtual Ptr<ASTOp> Clone(MapChildFuncRef) const override;
		virtual void Link(FProgram&, FLinkerOptions*) override;
		virtual int32 EvaluateInt(ASTOpList& Facts, bool& bOutUnknown) const override;
	};

}

