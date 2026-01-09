// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ExtensionData.h"

#include "MuR/MutableTrace.h"
#include "MuR/SerialisationPrivate.h"
#include "Templates/TypeHash.h"

namespace mu
{

//---------------------------------------------------------------------------------------------
void FExtensionData::Serialise(const FExtensionData* Data, FOutputArchive& Archive)
{
	Archive << *Data;
}


//---------------------------------------------------------------------------------------------
TSharedPtr<FExtensionData> FExtensionData::StaticUnserialise(FInputArchive& Archive)
{
	MUTABLE_CPUPROFILER_SCOPE(ExtensionDataUnserialise);
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

	TSharedPtr<FExtensionData> Result = MakeShared<FExtensionData>();
	Archive >> *Result;

	return Result;
}


//---------------------------------------------------------------------------------------------
uint32 FExtensionData::Hash() const
{
	uint32 Result = ::GetTypeHash(Index);
	Result = HashCombine(Result, ::GetTypeHash((uint8)Origin));

	return Result;
}


//---------------------------------------------------------------------------------------------
void FExtensionData::Serialise(FOutputArchive& Archive) const
{
	Archive << Index;

	uint8 OriginByte = (uint8)Origin;
	Archive << OriginByte;
}


//---------------------------------------------------------------------------------------------
void FExtensionData::Unserialise(FInputArchive& Archive)
{
	Archive >> Index;

	uint8 OriginByte;
	Archive >> OriginByte;
	Origin = (EOrigin)OriginByte;
}


//---------------------------------------------------------------------------------------------
int32 FExtensionData::GetDataSize() const
{
	return sizeof(FExtensionData);
}


}
