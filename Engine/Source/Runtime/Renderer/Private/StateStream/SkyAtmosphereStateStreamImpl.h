// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericStateStream.h"
#include "StateStream/SkyAtmosphereStateStream.h"

#define UE_API RENDERER_API

class FSceneInterface;
class FSkyAtmosphereSceneProxy;

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FSkyAtmosphereStateStreamSettings : TStateStreamSettings<ISkyAtmosphereStateStream, FSkyAtmosphereSceneProxy>
{
	static inline constexpr bool SkipCreatingDeletes = true;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FSkyAtmosphereStateStreamImpl : public TStateStream<FSkyAtmosphereStateStreamSettings>
{
public:
	FSkyAtmosphereStateStreamImpl(FSceneInterface& InScene);
private:
	UE_API virtual void Render_OnCreate(const FSkyAtmosphereStaticState& Ss, const FSkyAtmosphereDynamicState& Ds, FSkyAtmosphereSceneProxy*& UserData, bool IsDestroyedInSameFrame) override;
	UE_API virtual void Render_OnUpdate(const FSkyAtmosphereStaticState& Ss, const FSkyAtmosphereDynamicState& Ds, FSkyAtmosphereSceneProxy*& UserData) override;
	UE_API virtual void Render_OnDestroy(const FSkyAtmosphereStaticState& Ss, const FSkyAtmosphereDynamicState& Ds, FSkyAtmosphereSceneProxy*& UserData) override;

	FSceneInterface& Scene;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef UE_API
