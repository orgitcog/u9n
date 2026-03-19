// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineAssemblySchemaCustomization.h"

#include "Algo/Contains.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CineAssemblySchema.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "CineAssemblySchemaCustomization"

TSharedRef<IDetailCustomization> FCineAssemblySchemaCustomization::MakeInstance()
{
	return MakeShared<FCineAssemblySchemaCustomization>();
}

void FCineAssemblySchemaCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);

	// Ensure that we are only customizing one object
	if (CustomizedObjects.Num() != 1)
	{
		return;
	}

	CustomizedSchema = Cast<UCineAssemblySchema>(CustomizedObjects[0]);

	IDetailCategoryBuilder& DefaultCategory = DetailBuilder.EditCategory("Default");
	TSharedRef<IPropertyHandle> SchemaNamePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCineAssemblySchema, SchemaName));
	TSharedRef<IPropertyHandle> DescriptionPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCineAssemblySchema, Description));
	TSharedRef<IPropertyHandle> DefaultAssemblyNamePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCineAssemblySchema, DefaultAssemblyName));

	// Add the properties back into the category in the correct order
	IDetailPropertyRow& SchemaNamePropertyRow = DefaultCategory.AddProperty(SchemaNamePropertyHandle);
	DefaultCategory.AddProperty(DescriptionPropertyHandle);
	IDetailPropertyRow& DefaultAssemblyNamePropertyRow = DefaultCategory.AddProperty(DefaultAssemblyNamePropertyHandle);

	// Customize the widget for the SchemaName property to add additional validation on the user input text
	FString ExistingSchemaName;
	SchemaNamePropertyHandle->GetValue(ExistingSchemaName);

	// If the key name is not yet set, assign it a unique default key name
	if (ExistingSchemaName.IsEmpty())
	{
		SchemaNamePropertyHandle->SetValue(MakeUniqueSchemaName());
	}

	SchemaNamePropertyRow.CustomWidget()
		.NameContent()
		[
			SchemaNamePropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text_Lambda([this]() -> FText
					{
						return FText::FromString(CustomizedSchema->SchemaName);
					})
				.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
					{
						CustomizedSchema->RenameAsset(InText.ToString());
					})
				.OnVerifyTextChanged_Raw(this, &FCineAssemblySchemaCustomization::ValidateSchemaName)
		];

	// Customize the widget for the DefaultAssemblyName property to add additional validation on the user input text
	DefaultAssemblyNamePropertyRow.CustomWidget()
		.NameContent()
		[
			DefaultAssemblyNamePropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text_Lambda([this]() -> FText
					{
						return FText::FromString(CustomizedSchema->DefaultAssemblyName);
					})
				.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
					{
						CustomizedSchema->Modify();
						CustomizedSchema->DefaultAssemblyName = InText.ToString();
					})
				.OnVerifyTextChanged_Raw(this, &FCineAssemblySchemaCustomization::ValidateDefaultAssemblyName)
		];
}

FString FCineAssemblySchemaCustomization::MakeUniqueSchemaName() const
{
	const FString BaseName = TEXT("NewCineAssemblySchema");
	FString WorkingName = BaseName;

	int32 IntSuffix = 1;
	while (DoesSchemaExistWithName(WorkingName))
	{
		WorkingName = FString::Printf(TEXT("%s%d"), *BaseName, IntSuffix);
		IntSuffix++;
	}

	return WorkingName;
}

bool FCineAssemblySchemaCustomization::DoesSchemaExistWithName(const FString& SchemaName) const
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> SchemaAssets;
	AssetRegistryModule.Get().GetAssetsByClass(UCineAssemblySchema::StaticClass()->GetClassPathName(), SchemaAssets);

	return Algo::ContainsBy(SchemaAssets, *SchemaName, &FAssetData::AssetName);
}

bool FCineAssemblySchemaCustomization::ValidateSchemaName(const FText& InText, FText& OutErrorMessage) const
{
	if (InText.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("EmptyNameErrorMessage", "Please provide a name for the schema");
		return false;
	}

	// It is valid if the input text matches the schema's current name
	if (CustomizedSchema->SchemaName == InText.ToString())
	{
		return true;
	}

	if (DoesSchemaExistWithName(InText.ToString()))
	{
		OutErrorMessage = LOCTEXT("DuplicateNameErrorMessage", "A schema with that name already exists");
		return false;
	}

	return FName::IsValidXName(InText.ToString(), INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorMessage);
}

bool FCineAssemblySchemaCustomization::ValidateDefaultAssemblyName(const FText& InText, FText& OutErrorMessage) const
{
	// Ensure that the name does not contain any characters that would be invalid for an asset name
	// This matches the validation that would happen if the user was renaming an asset in the content browser
	FString InvalidCharacters = INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS;

	// These characters are actually valid, because we want to support naming tokens
	InvalidCharacters = InvalidCharacters.Replace(TEXT("{}"), TEXT(""));
	InvalidCharacters = InvalidCharacters.Replace(TEXT(":"), TEXT(""));

	const FString PotentialName = InText.ToString();

	if (!FName::IsValidXName(PotentialName, InvalidCharacters, &OutErrorMessage))
	{
		return false;
	}

	if (PotentialName.Contains(TEXT("{assembly}")))
	{
		OutErrorMessage = LOCTEXT("RecursiveAssemblyTokenError", "The default assembly name cannot use the {assembly} token");
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
