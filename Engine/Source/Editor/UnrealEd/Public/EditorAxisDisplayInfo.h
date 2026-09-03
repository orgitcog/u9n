// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AxisDisplayInfo.h"

class UNREALED_API FEditorAxisDisplayInfo : public IAxisDisplayInfo
{
public:

	FEditorAxisDisplayInfo();
	virtual ~FEditorAxisDisplayInfo() = default;

	virtual EAxisList::Type GetAxisDisplayCoordinateSystem() const override;
	
	virtual FText GetAxisToolTip(EAxisList::Type Axis) const override;
	virtual FText GetAxisDisplayName(EAxisList::Type Axis) override;
	virtual FText GetAxisDisplayNameShort(EAxisList::Type Axis) override;
	

	virtual FLinearColor GetAxisColor(EAxisList::Type Axis) override;

	virtual bool UseForwardRightUpDisplayNames() override;

	virtual FText GetRotationAxisToolTip(EAxisList::Type Axis) const override;
	virtual FText GetRotationAxisName(EAxisList::Type Axis) override;
	virtual FText GetRotationAxisNameShort(EAxisList::Type Axis) override;
	
	virtual FIntVector4 DefaultAxisComponentDisplaySwizzle() const override;

private:
	// Maps the given axis from FLU -> XYZ if the AxisDisplayCoordinateSystem is XYZ
	EAxisList::Type MapAxis(EAxisList::Type Axis) const;

	// Inits info stored in settings, like the axis colors
	void InitSettingsInfo(double EditorStartupTime);

	TOptional<bool> bUseForwardRightUpDisplayNames;
	mutable TOptional<EAxisList::Type> AxisDisplayCoordinateSystem;

};
