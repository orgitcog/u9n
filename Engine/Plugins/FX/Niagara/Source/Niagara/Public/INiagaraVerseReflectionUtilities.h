// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "NiagaraCore.h"

struct FNiagaraTypeDefinition;
struct FNiagaraVariableBase;

class INiagaraVerseReflectionUtilities
{
public:
	virtual ~INiagaraVerseReflectionUtilities() = default;

	virtual ENiagaraParameterAccessLevel GetDefaultAccessLevelForVariable(const FNiagaraVariableBase& Variable) const = 0;

	// Is the Name valid for Verse, returns error Text if not
	virtual TOptional<FText> IsValidVariableName(const FString& VariableName) const = 0;

	// Is the Type valid for Verse, returns error Text if not
	virtual TOptional<FText> IsValidVariableType(const FNiagaraTypeDefinition& TypeDef) const = 0;

	// Is the Variable valid for Verse, returns error Text if not
	virtual TOptional<FText> IsValidVariable(const FNiagaraVariableBase& Variable) const = 0;
};
