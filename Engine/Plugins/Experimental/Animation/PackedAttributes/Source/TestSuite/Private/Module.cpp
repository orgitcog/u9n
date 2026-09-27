// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

namespace UE::Anim::PackedAttributes
{
	class FTestSuiteModule : public IModuleInterface
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

IMPLEMENT_MODULE(UE::Anim::PackedAttributes::FTestSuiteModule, PackedAttributesTestSuite)
