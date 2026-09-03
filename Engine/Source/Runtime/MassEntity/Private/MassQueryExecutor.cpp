// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassQueryExecutor.h"
#include "MassExecutionContext.h"

namespace UE::Mass
{

FQueryExecutor::FQueryExecutor(FMassEntityQuery& InQuery, UObject* InLogOwner)
	: BoundQuery(&InQuery)
	, LogOwner(InLogOwner)
{
}

FMassEntityQuery FQueryExecutor::DummyQuery;

FQueryExecutor::FQueryExecutor()
	: BoundQuery(&DummyQuery)
{
}

void FQueryExecutor::CallExecute(FMassExecutionContext& Context)
{
#if WITH_MASSENTITY_DEBUG
	ValidateAccessors();
#endif

	AccessorsPtr->SetupForExecute(Context);

	Execute(Context);
}

void FQueryExecutor::ConfigureQuery(FMassSubsystemRequirements& ProcessorRequirements)
{
	AccessorsPtr->ConfigureQuery(*BoundQuery, ProcessorRequirements);
}

#if WITH_MASSENTITY_DEBUG
void FQueryExecutor::ValidateAccessors()
{
	const uintptr_t ExecutorStart = (uintptr_t)this;
	const uintptr_t ExecutorEnd = ExecutorStart + DebugSize;

	if (AccessorsPtr)
	{
		const uintptr_t AccessorsStart = (uintptr_t)this;
		checkf(ExecutorStart <= AccessorsStart && AccessorsStart <= ExecutorEnd, TEXT("Accessors assigned to a FQueryExecutor must be member variables of that struct."));
	}
};
#endif //WITH_MASSENTITY_DEBUG

} // namespace UE::Mass