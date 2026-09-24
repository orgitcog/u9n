// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TweenModel.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "ScopedTransaction.h"
#include "Templates/UniquePtr.h"

#include <type_traits>

class FCurveEditor;

namespace UE::TweeningUtilsEditor
{
/** Starts a transaction in StartBlendOperation, stops it in StopBlendOperation.  */
template<typename TBase> requires std::is_base_of_v<FTweenModel, TBase>
class TTransactionalTweenModelProxy : public TBase
{
public:

	template<typename... TArg>
	explicit TTransactionalTweenModelProxy(TArg&&... Arg) : TBase(Forward<TArg>(Arg)...) {}
	
	//~ Begin FTweenModel Interface
	virtual void StartBlendOperation() override
	{
		InProgressTransaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("FTransactionalTweenModelProxy", "Transaction", "Blend values"));
		TBase::StartBlendOperation();
	}
	virtual void StopBlendOperation() override
	{
		TBase::StopBlendOperation();
		InProgressTransaction.Reset();
	}
	//~ Begin FTweenModel Interface

private:

	/** Active during a blend operation. */
	TUniquePtr<FScopedTransaction> InProgressTransaction;
};
}

