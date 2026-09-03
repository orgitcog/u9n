// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MVVM/ViewModelTypeID.h"
#include "UObject/WeakObjectPtrFwd.h"

namespace UE::Sequencer
{

/**
 * An extension that is used for view models that represent UObject data
 */
class SEQUENCERCORE_API IObjectModelExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IObjectModelExtension)

	virtual ~IObjectModelExtension(){}

	virtual void InitializeObject(TWeakObjectPtr<> InWeakObject) = 0;
	virtual UObject* GetObject() const = 0;
};

} // namespace UE::Sequencer