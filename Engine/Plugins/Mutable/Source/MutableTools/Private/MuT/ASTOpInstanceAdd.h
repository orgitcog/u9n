// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct FProgram;


	//---------------------------------------------------------------------------------------------
	//! Operations to add elements to an instance
	//---------------------------------------------------------------------------------------------
	class ASTOpInstanceAdd final : public ASTOp
	{
	public:

		//! Type of switch
		EOpType type = EOpType::NONE;

		ASTChild instance;
		ASTChild value;
		uint32_t id = 0;
		uint32_t ExternalId = 0;
		int32 SharedSurfaceId = INDEX_NONE;
		FString name;

	public:

		ASTOpInstanceAdd();
		ASTOpInstanceAdd(const ASTOpInstanceAdd&) = delete;
		~ASTOpInstanceAdd();

		virtual EOpType GetOpType() const override { return type; }

		virtual void ForEachChild(const TFunctionRef<void(ASTChild&)>) override;
		virtual bool IsEqual(const ASTOp& otherUntyped) const override;
		virtual Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const override;
		virtual uint64 Hash() const override;
		virtual void Assert() override;
		virtual void Link(FProgram& program, FLinkerOptions* Options) override;
		virtual Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const override;
	};


}

