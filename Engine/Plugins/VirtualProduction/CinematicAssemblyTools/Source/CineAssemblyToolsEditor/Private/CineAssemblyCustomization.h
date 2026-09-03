// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

#include "Styling/SlateTypes.h"
#include "UObject/TemplateString.h"

class UCineAssembly;

/**
 * Detail customization for UCineAssembly
 */
class FCineAssemblyCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization interface */

private:
	/** Utility functions to customize each of the categories */
	void CustomizeDefaultCategory(IDetailLayoutBuilder& DetailBuilder);
	void CustomizeMetadataCategory(IDetailLayoutBuilder& DetailBuilder);
	void CustomizeSubsequenceCategory(IDetailLayoutBuilder& DetailBuilder);

	/**
	 * Evaluates all of the customization's template strings with the naming tokens subsystem.
	 * This function is throttled to only run at a set frequency, to avoid the potential to constantly query the naming tokens subsystem.
	 */
	void EvaluateTokenStrings();

	/**
	 * Evaluates the input template string with the naming tokens subsystem.
	 * This function is not throttled to allow for immediate updates.
	 */
	void EvaluateTokenString(FTemplateString& TokenString);

	/** Checks if the customized CineAssembly contains a SubAssembly name in its list of assets to create */
	ECheckBoxState IsSubAssemblyChecked(int32 Index) const;

	/** Adds/Removes a SubAssembly name from the custmoized CineAssembly's list of SubAssemblies */
	void SubAssemblyCheckStateChanged(ECheckBoxState CheckBoxState, int32 Index);

	/** Returns the template text for a SubAssembly template name */
	FText GetTemplateText(int32 Index) const;

	/** Evaluates the token strings, then returns the resolved text for a SubAssembly template name */
	FText GetResolvedText(int32 Index);

	/** Modifies the template text of the SubAssembly template name and re-evaluates the token string to update the resolved text */
	void OnTemplateTextCommitted(const FText& InText, ETextCommit::Type InCommitType, int32 Index);

	/** Builds the drop-down menu list of productions */
	TSharedRef<SWidget> BuildProductionNameMenu();

	/** Determines whether the input asset should be filtered out of an object picker widget, based on whether it is of the input schema type */
	bool ShouldFilterAssetBySchema(const FAssetData& InAssetData, FSoftObjectPath Schema);

private:
	/** The assembly being customized */
	UCineAssembly* CustomizedCineAssembly;

	/** Array of template names from the customized CineAssembly's schema */
	TArray<FTemplateString> SubAssemblyNames;

	/** The last time the naming tokens were updated */
	FDateTime LastTokenUpdateTime;
};
