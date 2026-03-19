// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointerFwd.h"

class IPropertyHandle;
class IPropertyUtilities;

namespace UE::Text3DEditor::Customization
{
	/** Used to customize Text3D component properties in details panel */
	class FText3DEditorTextComponentDetailCustomization : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FText3DEditorTextComponentDetailCustomization>();
		}

		//~ Begin IDetailCustomization
		virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
		//~ End IDetailCustomization
	};
}