// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Operations.h"
#include "MuT/AST.h"

namespace mu { struct FProgram; }

namespace mu
{
	class CompilerOptions;

    /** Convert constant data to different formats, based on their usage. */
    extern void DataOptimise( const CompilerOptions*, ASTOpList& roots );

}
