// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;

// customization for UVertexPaintBasicProperties
// creates custom widgets for paint and erase brushes with pressure sensitivity
class FVertexPaintBasicPropertiesDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
private:
	// helper to build strength widget, with pressure sensitivity toggle
	static void BuildPaintPressureWidget(
		IDetailLayoutBuilder& DetailBuilder,
		const TSharedPtr<IPropertyHandle>& ColorPropHandle,
		TSharedPtr<IPropertyHandle> EnablePressureSensitivityHandle
	);
};