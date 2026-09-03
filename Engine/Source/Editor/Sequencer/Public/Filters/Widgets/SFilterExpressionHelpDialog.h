// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"

class ISequencerTextFilterExpressionContext;
enum class ESequencerTextFilterValueType : uint8;

struct SEQUENCER_API FFilterExpressionHelpDialogConfig
{
	static constexpr float DefaultMaxDesiredWidth = 460.f;
	static constexpr float DefaultMaxDesiredHeight = 560.f;

	FFilterExpressionHelpDialogConfig();

	FName IdentifierName;

	FText DialogTitle;
	FString DocumentationLink;

	TArray<TSharedRef<ISequencerTextFilterExpressionContext>> TextFilterExpressionContexts;

	float MaxDesiredWidth = DefaultMaxDesiredWidth;
	float MaxDesiredHeight = DefaultMaxDesiredHeight;
};

class SFilterExpressionHelpDialog : public SWindow
{
public:
	SEQUENCER_API static void Open(FFilterExpressionHelpDialogConfig&& InConfig);

	static bool IsOpen(const FName InName);

	static void CloseWindow(const FName InName);

	void Construct(const FArguments& InArgs, FFilterExpressionHelpDialogConfig&& InConfig);

protected:
	static const FSlateColor KeyColor;
	static const FSlateColor ValueColor;

	static TMap<FName, TSharedPtr<SFilterExpressionHelpDialog>> DialogInstance;

	TSharedRef<SWidget> ConstructDialogHeader();
	TSharedRef<SWidget> ConstructExpressionWidgetList();
	TSharedRef<SWidget> ConstructExpressionWidget(const TSharedPtr<ISequencerTextFilterExpressionContext>& InExpressionContext);
	TSharedRef<SWidget> ConstructKeysWidget(const TSet<FName>& InKeys);
	TSharedRef<SWidget> ConstructValueWidget(const ESequencerTextFilterValueType InValueType);

	void OpenDocumentationLink() const;

	FFilterExpressionHelpDialogConfig Config;
};
