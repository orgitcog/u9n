// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"
#include "TG_Texture.h"
#include "EditorUndoClient.h"
#include "EdGraph/EdGraphPin.h"

class IPropertyHandle;
class SHorizontalBox;

/**
 * Widget for editing TextureDescriptor.
 */
class STG_GraphPinTextureDescriptorWidget : public SCompoundWidget, public FEditorUndoClient
{
	SLATE_DECLARE_WIDGET(STG_GraphPinTextureDescriptorWidget, SCompoundWidget)

private:
	DECLARE_DELEGATE_RetVal(FText, FGetTextDelegate);
	DECLARE_DELEGATE_TwoParams(FTextCommitted, const FText&, ETextCommit::Type);
	DECLARE_DELEGATE_RetVal(TSharedRef<SWidget>, FGenerateEnumMenu);

public:

	DECLARE_DELEGATE_OneParam(FOnTextureDescriptorChanged, const FTG_TextureDescriptor& /*TextureDescriptor*/)

	SLATE_BEGIN_ARGS(STG_GraphPinTextureDescriptorWidget)
		:
		_DescriptionMaxWidth(250.0f)
		, _PropertyHandle(nullptr)
		{}

		/** Maximum with of the query description field. */
		SLATE_ARGUMENT(float, DescriptionMaxWidth)

		/** TextureDescriptor to edit */
		SLATE_ATTRIBUTE(FTG_TextureDescriptor, TextureDescriptor)

		/** If set, the TextureDescriptor is read from the property, and the property is update when TextureDescriptor is edited. */ 
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, PropertyHandle)

		SLATE_EVENT(FOnTextureDescriptorChanged, OnTextureDescriptorChanged)
	SLATE_END_ARGS();

	STG_GraphPinTextureDescriptorWidget();
	virtual ~STG_GraphPinTextureDescriptorWidget() override;

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
	TSharedRef<SWidget> AddEnumComobox(FText Label, FGetTextDelegate GetText, FGenerateEnumMenu OnGenerateEnumMenu);
	TSharedRef<SWidget> AddSRGBWidget();
	FTG_TextureDescriptor GetSettings() const;
	void GenerateStringsFromEnum(TArray<FString>& OutEnumNames, const FString& EnumPathName);

	template<typename T>
	void GenerateValuesFromEnum(TArray<T>& OutEnumValues, const FString& EnumPathName) const;

	template<typename T>
	int GetValueFromIndex(const FString& EnumPathName, int Index) const;
	FString GetEnumValueDisplayName(const FString& EnumPathName, int EnumValue) const;
	EVisibility ShowParameters() const;
	EVisibility ShowPinLabel() const;

protected:
	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FEditorUndoClient Interface

private:
	UEdGraphPin* GraphPinObj;

	TSlateAttribute<FTG_TextureDescriptor> TextureDescriptorAttribute;
	FOnTextureDescriptorChanged OnTextureDescriptorChanged;
	FTG_TextureDescriptor CachedTextureDescriptor;
	FString SelectedWidthName;

	int SelectedWidthIndex;
	int SelectedHeightIndex;
	int SelectedFormatIndex;
	bool bSRGB = false;

	FGenerateEnumMenu OnGenerateWidthMenu;
	FGetTextDelegate GetWidthDelegate;
	FGenerateEnumMenu OnGenerateHeightMenu;
	FGetTextDelegate GetHeightDelegate;
	FGenerateEnumMenu OnGenerateFormatMenu;
	FGetTextDelegate GetFormatDelegate;

	TSharedRef<SWidget> OnGenerateWidthEnumMenu();
	void HandleWidthChanged(FString OuputName, int Index);
	FText HandleWidthText() const;

	TSharedRef<SWidget> OnGenerateHeightEnumMenu();
	void HandleHeightChanged(FString OuputName, int Index);
	FText HandleHeightText() const;

	TSharedRef<SWidget> OnGenerateFormatEnumMenu();
	void HandleFormatChanged(FString OuputName, int Index);
	FText HandleFormatText() const;

	ECheckBoxState HandleSRGBIsChecked() const;
	void HandleSRGBExecute(ECheckBoxState InNewState);

	const int LabelSize = 75;
};
