// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Math/NumericLimits.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuT/AST.h"


namespace mu
{
struct FProgram;


//---------------------------------------------------------------------------------------------
//! Adds an overlay material to an Instance
//---------------------------------------------------------------------------------------------
class ASTOpAddOverlayMaterial final : public ASTOp
{
public:

	ASTChild Instance;
	ASTChild OverlayMaterialId;

public:

	ASTOpAddOverlayMaterial();
	ASTOpAddOverlayMaterial(const ASTOpAddOverlayMaterial&) = delete;
	~ASTOpAddOverlayMaterial();

	EOpType GetOpType() const override { return EOpType::IN_ADDOVERLAYMATERIAL; }
	uint64 Hash() const override;
	void ForEachChild(const TFunctionRef<void(ASTChild&)> F) override;
	bool IsEqual(const ASTOp& OtherUntyped) const override;
	Ptr<ASTOp> Clone(MapChildFuncRef MapChild) const override;
	void Link(FProgram& Program, FLinkerOptions* Options) override;
};


}

