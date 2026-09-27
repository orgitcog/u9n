// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Module/AnimNextModuleContextData.h"
#include "AnimNextExecuteContext.generated.h"

struct FAnimNextDataInterfaceInstance;

namespace UE::AnimNext
{
	struct FLatentPropertyHandle;
	struct FModuleEventTickFunction;
	struct FScopedExecuteContextData;
}

USTRUCT(BlueprintType)
struct FAnimNextExecuteContext : public FRigVMExecuteContext
{
	GENERATED_BODY()

	FAnimNextExecuteContext() = default;

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextExecuteContext* OtherContext = static_cast<const FAnimNextExecuteContext*>(InOtherContext);
		ContextData = OtherContext->ContextData;
	}

	// Get the context data as the specified type. This will assert if the type differs from the last call to SetContextData.
	template<typename ContextType>
	const ContextType& GetContextData() const
	{
		return ContextData.Get<const ContextType>();
	}

protected:
	// Setup the context data to the specified type
	void SetContextData(TStructView<FAnimNextModuleContextData> InContextData)
	{
		ContextData = InContextData;
	}

	// Context data for this execution
	TStructView<FAnimNextModuleContextData> ContextData;

	friend struct UE::AnimNext::FScopedExecuteContextData;
};

namespace UE::AnimNext
{

// Helper for applying context data prior to RigVM execution
struct FScopedExecuteContextData
{
	FScopedExecuteContextData() = delete;

	explicit FScopedExecuteContextData(FAnimNextExecuteContext& InContext, TStructView<FAnimNextModuleContextData> InContextData)
		: Context(InContext)
	{
		Context.SetContextData(InContextData);
	}

	~FScopedExecuteContextData()
	{
		Context.SetContextData(TStructView<FAnimNextModuleContextData>());
	}

	FAnimNextExecuteContext& Context;
};

}