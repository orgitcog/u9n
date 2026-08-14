// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"

#if WITH_EDITOR

FMaterialIRModule::FMaterialIRModule()
{
	for (int32 i = 0; i < MIR::NumStages; ++i)
	{
		RootBlock[i] = new MIR::FBlock;
	}
}

FMaterialIRModule::~FMaterialIRModule()
{
	Empty();

	for (int32 i = 0; i < MIR::NumStages; ++i)
	{
		delete RootBlock[i];
	}
}

void FMaterialIRModule::Empty()
{
	for (int32 i = 0; i < MIR::NumStages; ++i)
	{
		RootBlock[i]->Instructions = nullptr;
		Outputs[i].Empty();
	}

	for (MIR::FValue* Value : Values)
	{
		FMemory::Free(Value);
	}

	Values.Empty();

	// Reset module statistics.
	for (int i = 0; i < MIR::NumStages; ++i)
	{
		Statistics.ExternalInputUsedMask[i].Init(false, (int)MIR::EExternalInput::Count);
	}

	Statistics.NumVertexTexCoords = 0;
	Statistics.NumPixelTexCoords = 0;

	Allocator.Flush();
}

const TCHAR* FMaterialIRModule::PushUserString(FString InString)
{
	UserStrings.Add(MoveTemp(InString));
	return GetData(UserStrings.Last());
}

void FMaterialIRModule::AddError(UMaterialExpression* Expression, FString Message)
{
	Errors.Push({ Expression, MoveTemp(Message) });
}

#endif // #if WITH_EDITOR
