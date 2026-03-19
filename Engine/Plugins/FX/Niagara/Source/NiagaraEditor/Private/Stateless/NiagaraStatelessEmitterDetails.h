// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IDetailLayoutBuilder;
class UNiagaraStatelessEmitter;

class FNiagaraStatelessEmitterDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	bool GetFixedBoundsEnabled() const;

protected:
	TWeakObjectPtr<UNiagaraStatelessEmitter> WeakEmitter;
};
