// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"

class FPCGHLSLSyntaxHighlighter;
class SPCGNodeSourceTextBox;
class UPCGComputeSource;

/**
 * Implements a details view customization for PCG Compute Sources.
 */
class FPCGComputeSourceDetails : public IDetailCustomization
{
public:
	FPCGComputeSourceDetails();

	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	FText GetSourceText() const;
	void OnSourceTextChanged(const FText& InText);
	void OnSourceTextCommitted(const FText& InText, ETextCommit::Type InCommitInfo);
	void OnSourceTextChangesApplied() const;
	void SetComputeSourceText() const;

	TWeakObjectPtr<UPCGComputeSource> ComputeSourceWeakPtr;
	TSharedRef<FPCGHLSLSyntaxHighlighter> SyntaxHighlighter;
	TSharedPtr<SPCGNodeSourceTextBox> SourceTextBox;

	FText SourceText;
};
