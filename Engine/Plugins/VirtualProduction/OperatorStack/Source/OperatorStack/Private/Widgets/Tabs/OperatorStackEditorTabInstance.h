// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FSpawnTabArgs;
class ILevelEditor;
class SDockTab;
class SOperatorStackEditorWidget;
class UObject;
class UTypedElementSelectionSet;

struct FOperatorStackEditorTabInstance : TSharedFromThis<FOperatorStackEditorTabInstance>
{
	explicit FOperatorStackEditorTabInstance(const TSharedRef<ILevelEditor>& InLevelEditor);
	virtual ~FOperatorStackEditorTabInstance();

	TSharedPtr<SDockTab> InvokeTab();
	bool CloseTab();
	bool RefreshTab(UObject* InContext, bool bInForce);
	bool FocusTab(const UObject* InContext, FName InIdentifier);

	TSharedPtr<SOperatorStackEditorWidget> GetOperatorStackEditorWidget() const;
	bool RegisterTab();
	bool UnregisterTab();

	TSharedPtr<ILevelEditor> GetLevelEditor() const
	{
		return LevelEditorWeak.Pin();
	}

private:
	void BindDelegates();
	void UnbindDelegates();

	TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& InArgs);
	void OnSelectionSetChanged(const UTypedElementSelectionSet* InSelection, bool bInForce);
	void OnSelectionChanged(UObject* InSelectionObject);

	TWeakPtr<ILevelEditor> LevelEditorWeak;
	int32 WidgetIdentifier = INDEX_NONE;
};
