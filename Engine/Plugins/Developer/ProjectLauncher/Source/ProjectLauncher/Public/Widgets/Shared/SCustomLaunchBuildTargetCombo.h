// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Fonts/SlateFontInfo.h"

class PROJECTLAUNCHER_API SCustomLaunchBuildTargetCombo
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, FString );

	SLATE_BEGIN_ARGS(SCustomLaunchBuildTargetCombo)
		: _TextStyle( &FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "NormalText" ) )
		{}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged);
		SLATE_ATTRIBUTE(FString, SelectedProject);
		SLATE_ATTRIBUTE(FString, SelectedBuildTarget);
		SLATE_ATTRIBUTE(TArray<EBuildTargetType>, SupportedTargetTypes);
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )
	SLATE_END_ARGS()

public:
	void Construct(	const FArguments& InArgs);

protected:
	FString GetDefaultBuildTargetName() const;

	TAttribute<TArray<EBuildTargetType>> SupportedTargetTypes;
	TAttribute<FString> SelectedProject;
	TAttribute<FString> SelectedBuildTarget;
	FOnSelectionChanged OnSelectionChanged;
	bool bShowAnyProjectOption;

	TSharedRef<SWidget> MakeBuildTargetSelectionWidget();

	FText GetBuildTargetName() const;
	void SetBuildTargetName(FString BuildTargetName);


};
