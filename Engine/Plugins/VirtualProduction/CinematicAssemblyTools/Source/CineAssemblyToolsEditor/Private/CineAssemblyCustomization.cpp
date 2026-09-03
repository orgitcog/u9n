// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineAssemblyCustomization.h"

#include "Algo/Contains.h"
#include "Algo/IndexOf.h"
#include "Algo/Find.h"
#include "CineAssembly.h"
#include "CineAssemblyNamingTokens.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ProductionSettings.h"
#include "PropertyCustomizationHelpers.h"
#include "STemplateStringEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CineAssemblyCustomization"

TSharedRef<IDetailCustomization> FCineAssemblyCustomization::MakeInstance()
{
	return MakeShared<FCineAssemblyCustomization>();
}

void FCineAssemblyCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);

	// Ensure that we are only customizing one object
	if (CustomizedObjects.Num() != 1)
	{
		return;
	}

	CustomizedCineAssembly = Cast<UCineAssembly>(CustomizedObjects[0]);

	CustomizeDefaultCategory(DetailBuilder);
	CustomizeMetadataCategory(DetailBuilder);
	CustomizeSubsequenceCategory(DetailBuilder);
}

void FCineAssemblyCustomization::CustomizeDefaultCategory(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& DefaultCategory = DetailBuilder.EditCategory("Default", LOCTEXT("DefaultCategoryName", "Default"));

	// Customize the ParenteAssembly and Production properties of the UCineAssembly
	TSharedRef<IPropertyHandle> LevelPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCineAssembly, Level));
	DefaultCategory.AddProperty(LevelPropertyHandle);

	TSharedRef<IPropertyHandle> ParentPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCineAssembly, ParentAssembly));
	IDetailPropertyRow& ParentPropertyRow = DefaultCategory.AddProperty(ParentPropertyHandle);

	TSharedRef<IPropertyHandle> ProdctionPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCineAssembly, Production));
	IDetailPropertyRow& ProductionPropertyRow = DefaultCategory.AddProperty(ProdctionPropertyHandle);

	const UCineAssemblySchema* BaseSchema = CustomizedCineAssembly->GetSchema();
	const FSoftObjectPath ParentSchema = BaseSchema ? BaseSchema->ParentSchema : FSoftObjectPath();

	ParentPropertyRow.CustomWidget()
		.NameContent()
		[
			ParentPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SObjectPropertyEntryBox)
				.AllowedClass(UCineAssembly::StaticClass())
				.ThumbnailPool(DetailBuilder.GetThumbnailPool())
				.AllowCreate(true)
				.OnShouldFilterAsset_Raw(this, &FCineAssemblyCustomization::ShouldFilterAssetBySchema, ParentSchema)
				.ObjectPath_Lambda([this]()
					{
						return CustomizedCineAssembly->ParentAssembly.ToString();
					})
				.OnObjectChanged_Lambda([this](const FAssetData& InAssetData)
					{
						CustomizedCineAssembly->Modify();
						CustomizedCineAssembly->ParentAssembly = InAssetData.GetObjectPathString();
					})
		];

	ProductionPropertyRow.CustomWidget()
		.NameContent()
		[
			ProdctionPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SComboButton)
				.VAlign(VAlign_Center)
				.OnGetMenuContent(this, &FCineAssemblyCustomization::BuildProductionNameMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
						.Text_Lambda([this]()
							{
								const UProductionSettings* const ProductionSettings = GetDefault<UProductionSettings>();
								TOptional<const FCinematicProduction> Production = ProductionSettings->GetProduction(CustomizedCineAssembly->Production);
								return Production.IsSet() ? FText::FromString(Production.GetValue().ProductionName) : FText::FromName(NAME_None);
							})
						.Font(IDetailLayoutBuilder::GetDetailFont())
				]
		];
}

void FCineAssemblyCustomization::CustomizeMetadataCategory(IDetailLayoutBuilder& DetailBuilder)
{
	const UCineAssemblySchema* BaseSchema = CustomizedCineAssembly->GetSchema();
	if (!BaseSchema)
	{
		return;
	}

	// Add a new category for Schema Metadata properties
	const FText MetadataCategoryName = FText::Format(LOCTEXT("SchemaMetadataCategoryName", "{0} Metadata"), FText::FromString(BaseSchema->SchemaName));
	IDetailCategoryBuilder& MetadataCategory = DetailBuilder.EditCategory("SchemaMetadata", MetadataCategoryName);

	// Add a property row for each metadata struct in the customized Assembly's base schema
	for (const FAssemblyMetadataDesc& MetadataDesc : BaseSchema->AssemblyMetadata)
	{
		if (MetadataDesc.Key.IsEmpty())
		{
			continue;
		}

		CustomizedCineAssembly->AddMetadataNamingToken(MetadataDesc.Key);

		TSharedPtr<SWidget> ValueWidget;
		if (MetadataDesc.Type == ECineAssemblyMetadataType::String)
		{
			ValueWidget = SNew(SBox)
				.MaxDesiredHeight(120.0f)
				[
					SNew(SMultiLineEditableTextBox)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.AutoWrapText(true)
						.Text_Lambda([this, MetadataDesc]()
							{
								FString Value;
								if (!CustomizedCineAssembly->GetMetadataAsString(MetadataDesc.Key, Value))
								{
									Value = MetadataDesc.DefaultValue.Get<FString>();
								}
								return FText::FromString(Value);
							})
						.OnTextCommitted_Lambda([this, MetadataDesc](const FText& InText, ETextCommit::Type InCommitType)
							{
								CustomizedCineAssembly->SetMetadataAsString(MetadataDesc.Key, InText.ToString());
							})
				];
		}
		else if (MetadataDesc.Type == ECineAssemblyMetadataType::Bool)
		{
			ValueWidget = SNew(SCheckBox)
				.IsChecked_Lambda([this, MetadataDesc]()
					{
						bool Value;
						if (!CustomizedCineAssembly->GetMetadataAsBool(MetadataDesc.Key, Value))
						{
							Value = MetadataDesc.DefaultValue.Get<bool>();
						}
						return Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
				.OnCheckStateChanged_Lambda([this, MetadataDesc](ECheckBoxState CheckBoxState)
					{
						const bool Value = (CheckBoxState == ECheckBoxState::Checked);
						CustomizedCineAssembly->SetMetadataAsBool(MetadataDesc.Key, Value);
					});
		}
		else if (MetadataDesc.Type == ECineAssemblyMetadataType::Integer)
		{
			ValueWidget = SNew(SNumericEntryBox<int32>)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Value_Lambda([this, MetadataDesc]()
					{
						int32 Value;
						if (!CustomizedCineAssembly->GetMetadataAsInteger(MetadataDesc.Key, Value))
						{
							Value = MetadataDesc.DefaultValue.Get<int32>();
						}
						return Value;
					})
				.OnValueChanged_Lambda([this, MetadataDesc](int32 InValue)
					{
						CustomizedCineAssembly->SetMetadataAsInteger(MetadataDesc.Key, InValue);
					});
		}
		else if (MetadataDesc.Type == ECineAssemblyMetadataType::Float)
		{
			ValueWidget = SNew(SNumericEntryBox<float>)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Value_Lambda([this, MetadataDesc]()
					{
						float Value;
						if (!CustomizedCineAssembly->GetMetadataAsFloat(MetadataDesc.Key, Value))
						{
							Value = MetadataDesc.DefaultValue.Get<float>();
						}
						return Value;
					})
				.OnValueChanged_Lambda([this, MetadataDesc](float InValue)
					{
						CustomizedCineAssembly->SetMetadataAsFloat(MetadataDesc.Key, InValue);
					});
		}
		else if (MetadataDesc.Type == ECineAssemblyMetadataType::AssetPath)
		{
			ValueWidget = SNew(SObjectPropertyEntryBox)
				.AllowedClass(MetadataDesc.AssetClass.ResolveClass())
				.ThumbnailPool(DetailBuilder.GetThumbnailPool())
				.AllowCreate(true)
				.ObjectPath_Lambda([this, MetadataDesc]()
					{
						FString Value;
						if (!CustomizedCineAssembly->GetMetadataAsString(MetadataDesc.Key, Value))
						{
							Value = MetadataDesc.DefaultValue.Get<FString>();
						}
						return Value;
					})
				.OnObjectChanged_Lambda([this, MetadataDesc](const FAssetData& InAssetData)
					{
						// Store the path of the selected object as a string
						CustomizedCineAssembly->SetMetadataAsString(MetadataDesc.Key, InAssetData.GetObjectPathString());
					});
		}
		else if (MetadataDesc.Type == ECineAssemblyMetadataType::CineAssembly)
		{
			ValueWidget = SNew(SObjectPropertyEntryBox)
				.AllowedClass(UCineAssembly::StaticClass())
				.ThumbnailPool(DetailBuilder.GetThumbnailPool())
				.AllowCreate(true)
				.OnShouldFilterAsset_Raw(this, &FCineAssemblyCustomization::ShouldFilterAssetBySchema, MetadataDesc.SchemaType)
				.ObjectPath_Lambda([this, MetadataDesc]()
					{
						FString Value;
						if (!CustomizedCineAssembly->GetMetadataAsString(MetadataDesc.Key, Value))
						{
							Value = MetadataDesc.DefaultValue.Get<FString>();
						}
						return Value;
					})
				.OnObjectChanged_Lambda([this, MetadataDesc](const FAssetData& InAssetData)
					{
						// Store the path of the selected object as a string
						CustomizedCineAssembly->SetMetadataAsString(MetadataDesc.Key, InAssetData.GetObjectPathString());
					});
		}
		else
		{
			checkNoEntry();
		}

		MetadataCategory.AddCustomRow(FText::FromString(MetadataDesc.Key))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(FText::FromString(MetadataDesc.Key))
					.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.HAlign(HAlign_Fill)
			[
				ValueWidget.ToSharedRef()
			];
	}
}

void FCineAssemblyCustomization::CustomizeSubsequenceCategory(IDetailLayoutBuilder& DetailBuilder)
{
	const UCineAssemblySchema* BaseSchema = CustomizedCineAssembly->GetSchema();
	if (!BaseSchema)
	{
		return;
	}

	// Add a new category for Schema Subsequences
	const FText SubsequencesCategoryName = LOCTEXT("SchemaSubseuqnecesCategoryName", "Subsequences");
	IDetailCategoryBuilder& SubsequenceCategory = DetailBuilder.EditCategory("SchemaSubsequences", SubsequencesCategoryName);

	SubAssemblyNames.Reset();
	for (const FString& SubsequenceName : BaseSchema->SubsequencesToCreate)
	{
		FTemplateString SubsequenceTemplateString;
		SubsequenceTemplateString.Template = SubsequenceName;
		SubAssemblyNames.Add(SubsequenceTemplateString);
	}

	for (int32 Index = 0; Index < SubAssemblyNames.Num(); ++Index)
	{
		SubsequenceCategory.AddCustomRow(FText::GetEmpty())
			.RowTag(GET_MEMBER_NAME_CHECKED(UCineAssembly, SubAssemblyNames))
			.NameContent()
			.HAlign(HAlign_Right)
			[
				SNew(SCheckBox)
					.IsChecked_Raw(this, &FCineAssemblyCustomization::IsSubAssemblyChecked, Index)
					.OnCheckStateChanged_Raw(this, &FCineAssemblyCustomization::SubAssemblyCheckStateChanged, Index)
			]
			.ValueContent()
			.HAlign(HAlign_Fill)
			[
				SNew(STemplateStringEditableTextBox)
					.Text_Raw(this, &FCineAssemblyCustomization::GetTemplateText, Index)
					.ResolvedText_Lambda([this, Index]() { return GetResolvedText(Index); })
					.OnTextCommitted_Raw(this, &FCineAssemblyCustomization::OnTemplateTextCommitted, Index)
			];
	}
}

bool FCineAssemblyCustomization::ShouldFilterAssetBySchema(const FAssetData& InAssetData, FSoftObjectPath Schema)
{
	if (Schema.IsValid())
	{
		const FAssetDataTagMapSharedView::FFindTagResult AssemblyType = InAssetData.TagsAndValues.FindTag(UCineAssembly::AssetRegistryTag_AssemblyType);
		if (AssemblyType.IsSet())
		{
			return !AssemblyType.GetValue().Equals(Schema.GetAssetName());
		}
		return true;
	}
	return false;
}

ECheckBoxState FCineAssemblyCustomization::IsSubAssemblyChecked(int32 Index) const
{
	if (SubAssemblyNames.IsValidIndex(Index))
	{
		const FTemplateString& SubAssemblyName = SubAssemblyNames[Index];
		const bool bResult = Algo::ContainsBy(CustomizedCineAssembly->SubAssemblyNames, SubAssemblyName.Template, &FTemplateString::Template);
		return bResult ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void FCineAssemblyCustomization::SubAssemblyCheckStateChanged(ECheckBoxState CheckBoxState, int32 Index)
{
	if (SubAssemblyNames.IsValidIndex(Index))
	{
		FTemplateString& SubAssemblyName = SubAssemblyNames[Index];

		if (CheckBoxState == ECheckBoxState::Checked)
		{
			CustomizedCineAssembly->SubAssemblyNames.Add(SubAssemblyName);
		}
		else if (CheckBoxState == ECheckBoxState::Unchecked)
		{
			const int32 IndexToRemove = Algo::IndexOfBy(CustomizedCineAssembly->SubAssemblyNames, SubAssemblyName.Template, &FTemplateString::Template);
			CustomizedCineAssembly->SubAssemblyNames.RemoveAt(IndexToRemove);
		}
	}
}

FText FCineAssemblyCustomization::GetTemplateText(int32 Index) const
{
	if (SubAssemblyNames.IsValidIndex(Index))
	{
		const FTemplateString& SubAssemblyName = SubAssemblyNames[Index];
		return FText::FromString(FPaths::GetBaseFilename(SubAssemblyName.Template));
	}
	return FText::GetEmpty();
}

FText FCineAssemblyCustomization::GetResolvedText(int32 Index)
{
	EvaluateTokenStrings();

	if (SubAssemblyNames.IsValidIndex(Index))
	{
		const FTemplateString& SubAssemblyName = SubAssemblyNames[Index];
		return FText::FromString(FPaths::GetBaseFilename(SubAssemblyName.Resolved.ToString()));
	}
	return FText::GetEmpty();
}

void FCineAssemblyCustomization::OnTemplateTextCommitted(const FText& InText, ETextCommit::Type InCommitType, int32 Index)
{
	if (SubAssemblyNames.IsValidIndex(Index))
	{
		FTemplateString& SubAssemblyName = SubAssemblyNames[Index];

		FTemplateString* TemplateString = Algo::FindBy(CustomizedCineAssembly->SubAssemblyNames, SubAssemblyName.Template, &FTemplateString::Template);

		const FString Path = FPaths::GetPath(SubAssemblyName.Template);
		SubAssemblyName.Template = Path / InText.ToString();

		if (TemplateString)
		{
			TemplateString->Template = SubAssemblyName.Template;
		}

		EvaluateTokenString(SubAssemblyName);
	}
}

void FCineAssemblyCustomization::EvaluateTokenStrings()
{
	FDateTime CurrentTime = FDateTime::Now();
	if ((CurrentTime - LastTokenUpdateTime).GetSeconds() >= 1.0f)
	{
		for (FTemplateString& SubAssemblyName : SubAssemblyNames)
		{
			EvaluateTokenString(SubAssemblyName);
		}

		LastTokenUpdateTime = CurrentTime;
	}
}

void FCineAssemblyCustomization::EvaluateTokenString(FTemplateString& TokenString)
{
	TokenString.Resolved = UCineAssemblyNamingTokens::GetResolvedText(TokenString.Template, CustomizedCineAssembly);
}

TSharedRef<SWidget> FCineAssemblyCustomization::BuildProductionNameMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	// Always add a "None" option
	FName NoActiveProductionName = NAME_None;
	MenuBuilder.AddMenuEntry(
		FText::FromName(NoActiveProductionName),
		FText::FromName(NoActiveProductionName),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() 
			{ 
				CustomizedCineAssembly->Modify();
				CustomizedCineAssembly->Production = FGuid();
				CustomizedCineAssembly->ProductionName = TEXT("None");
			})),
		NAME_None,
		EUserInterfaceActionType::None
	);

	const UProductionSettings* const ProductionSettings = GetDefault<UProductionSettings>();

	// Add a menu option with the production name for each production available in this project
	const TArray<FCinematicProduction> Productions = ProductionSettings->GetProductions();
	for (const FCinematicProduction& Production : Productions)
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(Production.ProductionName),
			FText::FromString(Production.ProductionName),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Production]() 
				{
					CustomizedCineAssembly->Modify();
					CustomizedCineAssembly->Production = Production.ProductionID;
					CustomizedCineAssembly->ProductionName = Production.ProductionName;
				})),
			NAME_None,
			EUserInterfaceActionType::None
		);
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
