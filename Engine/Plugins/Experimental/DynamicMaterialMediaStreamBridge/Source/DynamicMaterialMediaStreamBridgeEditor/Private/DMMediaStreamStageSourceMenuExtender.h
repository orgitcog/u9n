// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UDMMenuContext;
struct FToolMenuSection;

class FDMMediaStreamStageSourceMenuExtender
{
public:
	static FDMMediaStreamStageSourceMenuExtender& Get();

	void Integrate();

private:
	bool bIntegrated = false;

	void ExtendMenu_ChangeSource(FToolMenuSection& InSection);

	void ExtendMenu_AddLayer(FToolMenuSection& InSection);

	void ChangeSourceToMediaStreamFromContext(UDMMenuContext* InMenuContext);

	void AddMediaStreamLayerFromContext(UDMMenuContext* InMenuContext);
};
