// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

namespace UE::Anim::PackedAttributes
{
	class FRuntimeModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
		}

		virtual void ShutdownModule() override
		{
		}
	};
}

IMPLEMENT_MODULE(UE::Anim::PackedAttributes::FRuntimeModule, PackedAttributes)
