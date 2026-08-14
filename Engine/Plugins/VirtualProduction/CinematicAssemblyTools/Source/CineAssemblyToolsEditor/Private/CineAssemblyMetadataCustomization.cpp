// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineAssemblyMetadataCustomization.h"

#include "Algo/Contains.h"
#include "Algo/IndexOf.h"
#include "CineAssembly.h"
#include "CineAssemblyNamingTokens.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "NamingTokensEngineSubsystem.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CineAssemblyMetadataCustomization"

TSharedRef<IPropertyTypeCustomization> FCineAssemblyMetadataCustomization::MakeInstance()
{
	return MakeShareable(new FCineAssemblyMetadataCustomization);
}

void FCineAssemblyMetadataCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			InPropertyHandle->CreatePropertyValueWidget()
		];
}

void FCineAssemblyMetadataCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get the schema object that owns the metadata struct being customized
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects = CustomizationUtils.GetPropertyUtilities()->GetSelectedObjects();
	if (CustomizedObjects.Num() != 1)
	{
		return;
	}

	Schema = Cast<UCineAssemblySchema>(CustomizedObjects[0].Get());

	if (!PropertyHandle->IsExpanded())
	{
		PropertyHandle->SetExpanded(true);
	}

	ArrayIndex = PropertyHandle->GetArrayIndex();
	if (Schema->AssemblyMetadata.IsValidIndex(ArrayIndex))
	{
		FAssemblyMetadataDesc& MetadataDesc = Schema->AssemblyMetadata[ArrayIndex];

		// Add all of the existing reflected properties of the metadata struct
		uint32 NumChildren;
		PropertyHandle->GetNumChildren(NumChildren);

		for (uint32 Index = 0; Index < NumChildren; ++Index)
		{
			TSharedRef<IPropertyHandle> ChildPropertyHandle = PropertyHandle->GetChildHandle(Index).ToSharedRef();

			if (ChildPropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FAssemblyMetadataDesc, Key))
			{
				CustomizeKeyProperty(ChildPropertyHandle, ChildBuilder, MetadataDesc);
			}
			else
			{
				ChildBuilder.AddProperty(ChildPropertyHandle).ShowPropertyButtons(false);
			}
		}

		// Create a widget switcher that can display the appropriate widget based on the metadata type
		TSharedPtr<IPropertyHandle> MetadataTypeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAssemblyMetadataDesc, Type));

		MetadataTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([&MetadataDesc]()
			{
				if ((MetadataDesc.Type == ECineAssemblyMetadataType::String) ||
					(MetadataDesc.Type == ECineAssemblyMetadataType::AssetPath) ||
					(MetadataDesc.Type == ECineAssemblyMetadataType::CineAssembly))
				{
					MetadataDesc.DefaultValue.Set<FString>(TEXT(""));
				}
				else if (MetadataDesc.Type == ECineAssemblyMetadataType::Bool)
				{
					MetadataDesc.DefaultValue.Set<bool>(false);
				}
				else if (MetadataDesc.Type == ECineAssemblyMetadataType::Integer)
				{
					MetadataDesc.DefaultValue.Set<int32>(0);
				}
				else if (MetadataDesc.Type == ECineAssemblyMetadataType::Float)
				{
					MetadataDesc.DefaultValue.Set<float>(0);
				}
				else
				{
					checkNoEntry();
				}
			}));

		// Add a "Default Value" property, based on the metadata type
		TSharedPtr<SWidgetSwitcher> DefaultValueWidget = SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([MetadataTypeHandle]()
				{
					uint8 Value;
					MetadataTypeHandle->GetValue(Value);
					return Value;
				});

		DefaultValueWidget->AddSlot((uint8)ECineAssemblyMetadataType::String)
			[
				SNew(SBox)
					.MaxDesiredHeight(120.0f)
					[
						SNew(SMultiLineEditableTextBox)
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.AutoWrapText(true)
							.Text_Lambda([&MetadataDesc]() -> FText
								{
									if (const FString* Value = MetadataDesc.DefaultValue.TryGet<FString>())
									{
										return FText::FromString(*Value);
									}
									return FText::GetEmpty();
								})
							.OnTextCommitted_Lambda([&MetadataDesc](const FText& InText, ETextCommit::Type InCommitType)
								{
									MetadataDesc.DefaultValue.Set<FString>(InText.ToString());
								})
					]
			];

		DefaultValueWidget->AddSlot((uint8)ECineAssemblyMetadataType::Bool)
			[
				SNew(SCheckBox)
					.IsChecked_Lambda([&MetadataDesc]() -> ECheckBoxState
						{
							if (const bool* Value = MetadataDesc.DefaultValue.TryGet<bool>())
							{
								return *Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
							}
							return ECheckBoxState::Unchecked;
						})
					.OnCheckStateChanged_Lambda([&MetadataDesc](ECheckBoxState CheckBoxState)
						{
							const bool Value = (CheckBoxState == ECheckBoxState::Checked);
							MetadataDesc.DefaultValue.Set<bool>(Value);
						})
			];

		DefaultValueWidget->AddSlot((uint8)ECineAssemblyMetadataType::Integer)
			[
				SNew(SNumericEntryBox<int32>)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Value_Lambda([&MetadataDesc]() -> int32
						{
							if (const int32* Value = MetadataDesc.DefaultValue.TryGet<int32>())
							{
								return *Value;
							}
							return 0;
						})
					.OnValueChanged_Lambda([&MetadataDesc](int32 InValue)
						{
							MetadataDesc.DefaultValue.Set<int32>(InValue);
						})
			];

		DefaultValueWidget->AddSlot((uint8)ECineAssemblyMetadataType::Float)
			[
				SNew(SNumericEntryBox<float>)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Value_Lambda([&MetadataDesc]() -> float
						{
							if (const float* Value = MetadataDesc.DefaultValue.TryGet<float>())
							{
								return *Value;
							}
							return 0;
						})
					.OnValueChanged_Lambda([&MetadataDesc](float InValue)
						{
							MetadataDesc.DefaultValue.Set<float>(InValue);
						})
			];

		DefaultValueWidget->AddSlot((uint8)ECineAssemblyMetadataType::AssetPath)
			[
				SNew(SObjectPropertyEntryBox)
					.ThumbnailPool(CustomizationUtils.GetThumbnailPool())
					.AllowCreate(true)
					.OnShouldFilterAsset_Lambda([&MetadataDesc](const FAssetData& InAssetData) -> bool
						{
							// Filter out all assets that do not match the selected Asset Class for this metadata struct
							if (MetadataDesc.AssetClass.IsNull())
							{
								return true;
							}
							return (InAssetData.AssetClassPath != MetadataDesc.AssetClass.GetAssetPath());
						})
					.ObjectPath_Lambda([&MetadataDesc]() -> FString
						{
							if (const FString* Value = MetadataDesc.DefaultValue.TryGet<FString>())
							{
								return *Value;
							}
							return FString();
						})
					.OnObjectChanged_Lambda([&MetadataDesc](const FAssetData& InAssetData)
						{
							MetadataDesc.DefaultValue.Set<FString>(InAssetData.GetObjectPathString());
						})
			];

		DefaultValueWidget->AddSlot((uint8)ECineAssemblyMetadataType::CineAssembly)
			[
				SNew(SObjectPropertyEntryBox)
					.AllowedClass(UCineAssembly::StaticClass())
					.ThumbnailPool(CustomizationUtils.GetThumbnailPool())
					.AllowCreate(true)
					.OnShouldFilterAsset_Lambda([&MetadataDesc](const FAssetData& InAssetData) -> bool
						{
							// Filter out all Cine Assembly assets that do not match the selected Schema Type for this metadata struct
							const FAssetDataTagMapSharedView::FFindTagResult AssemblyType = InAssetData.TagsAndValues.FindTag(UCineAssembly::AssetRegistryTag_AssemblyType);
							if (AssemblyType.IsSet())
							{
								return !AssemblyType.GetValue().Equals(MetadataDesc.SchemaType.GetAssetName());
							}
							return true;
						})
					.ObjectPath_Lambda([&MetadataDesc]() -> FString
						{
							if (const FString* Value = MetadataDesc.DefaultValue.TryGet<FString>())
							{
								return *Value;
							}
							return FString();
						})
					.OnObjectChanged_Lambda([&MetadataDesc](const FAssetData& InAssetData)
						{
							MetadataDesc.DefaultValue.Set<FString>(InAssetData.GetObjectPathString());
						})
			];

		ChildBuilder.AddCustomRow(FText::GetEmpty())
			.NameContent()
			[
				SNew(STextBlock)
					.Text(NSLOCTEXT("CineAssemblyMetadataCustomization", "DefaultValueText", "Default Value"))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			[
				DefaultValueWidget.ToSharedRef()
			];
	}
}

void FCineAssemblyMetadataCustomization::CustomizeKeyProperty(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, FAssemblyMetadataDesc& MetadataDesc)
{
	FString ExistingKeyName;
	PropertyHandle->GetValue(ExistingKeyName);

	// If the key name is not yet set, assign it a unique default key name
	if (ExistingKeyName.IsEmpty())
	{
		PropertyHandle->SetValue(MakeUniqueKeyName());
	}

	IDetailPropertyRow& KeyRow = ChildBuilder.AddProperty(PropertyHandle);
	KeyRow.CustomWidget()
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text_Lambda([this, PropertyHandle]() -> FText
					{
						FString KeyName;
						PropertyHandle->GetValue(KeyName);
						return FText::FromString(KeyName);
					})
				.OnTextCommitted_Lambda([this, PropertyHandle](const FText& InText, ETextCommit::Type InCommitType)
					{
						const FString NewKeyName = InText.ToString();
						PropertyHandle->SetValue(NewKeyName);
					})
				.OnVerifyTextChanged_Raw(this, &FCineAssemblyMetadataCustomization::ValidateKeyName)
		];
}

bool FCineAssemblyMetadataCustomization::ValidateKeyName(const FText& InText, FText& OutErrorMessage) const
{
	// An empty name is invalid
	if (InText.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("EmptyKeyNameError", "Please provide a key name");
		return false;
	}

	// Check for duplicate keys in this schema
	const int32 MetadataIndex = Algo::IndexOfBy(Schema->AssemblyMetadata, InText.ToString(), &FAssemblyMetadataDesc::Key);
	if ((MetadataIndex != INDEX_NONE) && (MetadataIndex != ArrayIndex))
	{
		OutErrorMessage = LOCTEXT("DuplicateKeyNameError", "A metadata key with this name already exists in this schema");
		return false;
	}

	// Check that the proposed key name does not match one of the default CAT tokens
	UNamingTokensEngineSubsystem* NamingTokensSubsystem = GEngine->GetEngineSubsystem<UNamingTokensEngineSubsystem>();
	UCineAssemblyNamingTokens* CineAssemblyNamingTokens = Cast<UCineAssemblyNamingTokens>(NamingTokensSubsystem->GetNamingTokens(UCineAssemblyNamingTokens::TokenNamespace));

	TArray<FNamingTokenData> Tokens = CineAssemblyNamingTokens->GetDefaultTokens();
	if (Algo::ContainsBy(Tokens, InText.ToString(), &FNamingTokenData::TokenKey))
	{
		OutErrorMessage = LOCTEXT("ExistingTokenKeyError", "A CAT token key with this name already exists");
		return false;
	}

	return true;
}

FString FCineAssemblyMetadataCustomization::MakeUniqueKeyName()
{
	const FString BaseName = TEXT("NewKey");
	FString WorkingName = BaseName;

	int32 IntSuffix = 1;
	while (Algo::ContainsBy(Schema->AssemblyMetadata, WorkingName, &FAssemblyMetadataDesc::Key))
	{
		WorkingName = FString::Printf(TEXT("%s%d"), *BaseName, IntSuffix);
		IntSuffix++;
	}

	return WorkingName;
}

#undef LOCTEXT_NAMESPACE
