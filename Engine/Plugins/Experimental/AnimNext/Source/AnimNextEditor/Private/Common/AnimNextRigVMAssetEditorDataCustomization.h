// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

namespace UE::AnimNext::Editor
{

class FAnimNextRigVMAssetEditorDataCustomization : public IDetailCustomization
{
private:
	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

}
