// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "RigVMHost.h"
#include "RigVMBlueprint.h"
#include "Styling/SlateTypes.h"
#include "IPropertyUtilities.h"

class IRigVMEditor;
class IPropertyHandle;

class RIGVMEDITOR_API FRigVMCommentNodeDetailCustomization : public IDetailCustomization
{
	FRigVMCommentNodeDetailCustomization()
	: BlueprintBeingCustomized(nullptr)
	{}

	
public:

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRigVMCommentNodeDetailCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	URigVMBlueprint* BlueprintBeingCustomized;
	TArray<TWeakObjectPtr<URigVMCommentNode>> ObjectsBeingCustomized;

	void GetValuesFromNode(TWeakObjectPtr<URigVMCommentNode> Node);
	void SetValues(TWeakObjectPtr<URigVMCommentNode> Node);
	
	FText GetText() const;
	void SetText(const FText& InNewText, ETextCommit::Type InCommitType);

	FLinearColor GetColor() const;
	FReply OnChooseColor(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void OnColorPicked(FLinearColor LinearColor);

	ECheckBoxState IsShowingBubbleEnabled() const;
	void OnShowingBubbleStateChanged(ECheckBoxState InValue);

	ECheckBoxState IsColorBubbleEnabled() const;
	void OnColorBubbleStateChanged(ECheckBoxState InValue);

	TOptional<int> GetFontSize() const;
	void OnFontSizeChanged(int32 InValue, ETextCommit::Type Arg);
	
	FString CommentText;
	bool bShowingBubble;
	bool bBubbleColorEnabled;
	int32 FontSize;
};



