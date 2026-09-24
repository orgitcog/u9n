// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorModule.h"

namespace UE::Chaos::ClothAsset
{
	struct FClothAssetComponentBroker;

	class FChaosClothAssetEditorModule final : public FBaseCharacterFXEditorModule
	{
	public:
		/** IModuleInterface implementation */
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

	private:

		FDelegateHandle StartupCallbackDelegateHandle;
		FDelegateHandle OnCVarChangedDelegateHandle;

		void RegisterMenus();
		
		TSharedPtr<FClothAssetComponentBroker> ClothAssetComponentBroker;
	};
} // namespace UE::Chaos::ClothAsset
