// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Fonts/SlateFontInfo.h"

template<typename ItemType> class SComboBox;


class PROJECTLAUNCHER_API SCustomLaunchPlatformCombo
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, TArray<FString> );

	SLATE_BEGIN_ARGS(SCustomLaunchPlatformCombo)
		: _TextStyle( &FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "NormalText" ) )
		{}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged);
		SLATE_ATTRIBUTE(TArray<FString>, SelectedPlatforms);
		SLATE_ARGUMENT(bool, BasicPlatformsOnly)
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )
	SLATE_END_ARGS()

public:
	void Construct(	const FArguments& InArgs);

protected:
	TAttribute<TArray<FString>> SelectedPlatforms;
	FOnSelectionChanged OnSelectionChanged;
	bool bBasicPlatformsOnly;

	TSharedRef<SWidget> OnGeneratePlatformListWidget( TSharedPtr<FString> Platform ) const;
	void OnPlatformSelectionChanged( TSharedPtr<FString> Platform, ESelectInfo::Type InSelectInfo );
	const FSlateBrush* GetSelectedPlatformBrush() const;
	FText GetSelectedPlatformName() const;
	TArray<TSharedPtr<FString>> PlatformsList;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PlatformsComboBox;
};
