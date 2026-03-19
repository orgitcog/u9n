// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigModuleDetails.h"
#include "Widgets/SWidget.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "ModularRigController.h"
#include "ControlRigBlueprint.h"
#include "ControlRigEditorStyle.h"
#include "ControlRigElementDetails.h"
#include "Graph/ControlRigGraph.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "SEnumCombo.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/AppStyle.h"
#include "Editor/SModularRigTreeView.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Features/IModularFeatures.h"
#include "IPropertyAccessEditor.h"
#include "ModularRigRuleManager.h"
#include "PropertyPath.h"
#include "ScopedTransaction.h"
#include "Editor/SRigHierarchyTreeView.h"
#include "Widgets/SRigVMVariantTagWidget.h"
#include "Algo/Sort.h"
#include "Editor/SRigConnectorTargetWidget.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "ControlRigModuleDetails"

static const FText ControlRigModuleDetailsMultipleValues = LOCTEXT("MultipleValues", "Multiple Values");

static void RigModuleDetails_GetCustomizedInfo(TSharedRef<IPropertyHandle> InStructPropertyHandle, UControlRigBlueprint*& OutBlueprint)
{
	TArray<UObject*> Objects;
	InStructPropertyHandle->GetOuterObjects(Objects);
	for (UObject* Object : Objects)
	{
		if (Object->IsA<UControlRigBlueprint>())
		{
			OutBlueprint = CastChecked<UControlRigBlueprint>(Object);
			break;
		}

		OutBlueprint = Object->GetTypedOuter<UControlRigBlueprint>();
		if(OutBlueprint)
		{
			break;
		}

		if(const UControlRig* ControlRig = Object->GetTypedOuter<UControlRig>())
		{
			OutBlueprint = Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy);
			if(OutBlueprint)
			{
				break;
			}
		}
	}

	if (OutBlueprint == nullptr)
	{
		TArray<UPackage*> Packages;
		InStructPropertyHandle->GetOuterPackages(Packages);
		for (UPackage* Package : Packages)
		{
			if (Package == nullptr)
			{
				continue;
			}

			TArray<UObject*> SubObjects;
			Package->GetDefaultSubobjects(SubObjects);
			for (UObject* SubObject : SubObjects)
			{
				if (UControlRig* Rig = Cast<UControlRig>(SubObject))
				{
					UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(Rig->GetClass()->ClassGeneratedBy);
					if (Blueprint)
					{
						if(Blueprint->GetOutermost() == Package)
						{
							OutBlueprint = Blueprint;
							break;
						}
					}
				}
			}

			if (OutBlueprint)
			{
				break;
			}
		}
	}
}

static UControlRigBlueprint* RigModuleDetails_GetBlueprintFromRig(UModularRig* InRig)
{
	if(InRig == nullptr)
	{
		return nullptr;
	}

	UControlRigBlueprint* Blueprint = InRig->GetTypedOuter<UControlRigBlueprint>();
	if(Blueprint == nullptr)
	{
		Blueprint = Cast<UControlRigBlueprint>(InRig->GetClass()->ClassGeneratedBy);
	}
	return Blueprint;
}

void FRigModuleInstanceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PerModuleInfos.Reset();

	TArray<TWeakObjectPtr<UObject>> DetailObjects;
	DetailBuilder.GetObjectsBeingCustomized(DetailObjects);
	for(TWeakObjectPtr<UObject> DetailObject : DetailObjects)
	{
		if(UControlRig* ModuleInstance = Cast<UControlRig>(DetailObject))
		{
			if(const UModularRig* ModularRig = Cast<UModularRig>(ModuleInstance->GetOuter()))
			{
				if(const FRigModuleInstance* Module = ModularRig->FindModule(ModuleInstance))
				{
					const FName ModuleName = Module->Name;

					FPerModuleInfo Info;
					Info.ModuleName = ModuleName;
					Info.Module = ModularRig->GetHandle(ModuleName);
					if(!Info.Module.IsValid())
					{
						return;
					}
					
					if(const UControlRigBlueprint* Blueprint = Info.GetBlueprint())
					{
						if(const UModularRig* DefaultModularRig = Cast<UModularRig>(Blueprint->GeneratedClass->GetDefaultObject()))
						{
							Info.DefaultModule = DefaultModularRig->GetHandle(ModuleName);
						}
					}

					PerModuleInfos.Add(Info);
				}
			}
		}
	}

	// don't customize if the 
	if(PerModuleInfos.IsEmpty())
	{
		return;
	}

	TArray<FName> OriginalCategoryNames;
	DetailBuilder.GetCategoryNames(OriginalCategoryNames);

	IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory(TEXT("General"), LOCTEXT("General", "General"));
	{
		static const FText NameTooltip = LOCTEXT("NameTooltip", "The name is used to determine the long name (the full path) and to provide a unique address within the rig.");
		GeneralCategory.AddCustomRow(FText::FromString(TEXT("Name")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Name")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(NameTooltip)
		]
		.ValueContent()
		[
			SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigModuleInstanceDetails::GetName)
			.OnTextCommitted(this, &FRigModuleInstanceDetails::SetName, DetailBuilder.GetPropertyUtilities())
			.ToolTipText(NameTooltip)
			.OnVerifyTextChanged(this, &FRigModuleInstanceDetails::OnVerifyNameChanged)
		];

		GeneralCategory.AddCustomRow(FText::FromString(TEXT("RigClass")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("RigClass")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(true)
		]
		.ValueContent()
		[
			SNew(SButton)
			.ContentPadding(0.0f)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.OnClicked(this, &FRigModuleInstanceDetails::HandleOpenRigModuleAsset)
			.Cursor(EMouseCursor::Default)
			.Text(this, &FRigModuleInstanceDetails::GetRigClassPath)
		];

		TWeakPtr<FRigModuleInstanceDetails> WeakThisPtr = SharedThis(this);

		GeneralCategory.AddCustomRow(FText::FromString(TEXT("Variant Tags")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Variant Tags")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(true)
		]
		.ValueContent()
		[
			SNew(SRigVMVariantTagWidget)
			.Orientation(EOrientation::Orient_Horizontal)
			.CanAddTags(false)
			.EnableContextMenu(false)
			.OnGetTags_Lambda([WeakThisPtr]() -> TArray<FRigVMTag>
			{
				TArray<FRigVMTag> Tags;
				if (TSharedPtr<FRigModuleInstanceDetails> StrongThis = WeakThisPtr.Pin())
				{
					const TArray<FPerModuleInfo>& Infos = StrongThis->PerModuleInfos;
					for (int32 InfoIndex=0; InfoIndex<Infos.Num(); ++InfoIndex)
					{
						const FPerModuleInfo& ModuleInfo = Infos[InfoIndex]; 
						if(ModuleInfo.Module.IsValid())
						{
							if (const FRigModuleInstance* Module = ModuleInfo.GetModule())
							{
								if (const UControlRigBlueprint* ModuleBlueprint = Cast<UControlRigBlueprint>(Module->GetRig()->GetClass()->ClassGeneratedBy))
								{
									if (InfoIndex == 0)
									{
										Tags = ModuleBlueprint->GetAssetVariant().Tags;
									}
									else
									{
										const TArray<FRigVMTag>& OtherTags = ModuleBlueprint->GetAssetVariant().Tags;
										bool bSameArray = Tags.Num() == OtherTags.Num();
										if (bSameArray)
										{
											for (const FRigVMTag& OtherTag : OtherTags)
											{
												if (!Tags.ContainsByPredicate([OtherTag](const FRigVMTag& Tag) { return OtherTag.Name == Tag.Name; }))
												{
													return {};
												}
											}
										}
										else
										{
											return {};
										}
									}
								}
							}
						}
					}
				}
				return Tags;
			})
		];
	}

	IDetailCategoryBuilder& ConnectionsCategory = DetailBuilder.EditCategory(TEXT("Connections"), LOCTEXT("Connections", "Connections"));
	{
		bool bDisplayConnectors = PerModuleInfos.Num() >= 1;
		if (PerModuleInfos.Num() > 1)
		{
			UModularRig* ModularRig = PerModuleInfos[0].GetModularRig();
			for (FPerModuleInfo& Info : PerModuleInfos)
			{
				if (Info.GetModularRig() != ModularRig)
				{
					bDisplayConnectors = false;
					break;
				}
			}
		}

		TArray<FRigModuleConnector> Connectors;
		TArray<TOptional<bool>> IsArrayConnector;
		if (bDisplayConnectors)
		{
			Connectors = GetConnectors();
			
			// sort connectors primary first, then secondary, then optional
			Algo::SortBy(Connectors, [](const FRigModuleConnector& Connector) -> int32
			{
				return Connector.IsPrimary() ? 0 : (Connector.IsOptional() ? 2 : 1);
			});

			IsArrayConnector.Reserve(Connectors.Num());
			
			for(const FRigModuleConnector& Connector : Connectors)
			{
				const FText Label = FText::FromString(Connector.Name);
				IsArrayConnector.Emplace();
				TOptional<bool>& IsArray = IsArrayConnector.Last();
				
				TArray<FRigElementResolveResult> Matches;
				for (int32 ModuleIndex=0; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
				{
					const FPerModuleInfo& Info = PerModuleInfos[ModuleIndex];
					if (const FRigModuleInstance* Module = Info.GetModule())
					{
						if (UModularRig* ModularRig = Info.GetModularRig())
						{
							if (URigHierarchy* Hierarchy = ModularRig->GetHierarchy())
							{
								const FRigHierarchyModulePath ConnectorModulePath(Info.ModuleName.ToString(), Connector.Name);
								FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);
								if (FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(Hierarchy->Find(ConnectorKey)))
								{
									if(ModuleIndex == 0)
									{
										IsArray = ConnectorElement->IsArrayConnector();
									}
									else if(IsArray.Get(false) != ConnectorElement->IsArrayConnector())
									{
										IsArray.Reset();
									}
								}
							}
						}
					}
				}
			}
		}

		if(bDisplayConnectors)
		{
			for(int32 ConnectorIndex = 0; ConnectorIndex < Connectors.Num(); ConnectorIndex++)
			{
				const FRigModuleConnector& Connector = Connectors[ConnectorIndex];
				const FString ConnectorName = Connector.Name;
				const FText Label = FText::FromString(Connector.Name);
				bool bIsArrayConnector = IsArrayConnector[ConnectorIndex].Get(false);

				FRigTreeDelegates RigTreeDelegates;
				RigTreeDelegates.OnGetHierarchy = FOnGetRigTreeHierarchy::CreateLambda([this]()
				{
					return PerModuleInfos[0].GetModularRig()->GetHierarchy();
				});

				static const FSlateBrush* PrimaryBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorPrimary");
				static const FSlateBrush* SecondaryBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorSecondary");
				static const FSlateBrush* OptionalBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorOptional");

				const FSlateBrush* IconBrush = Connector.IsPrimary() ? PrimaryBrush : (Connector.IsOptional() ? OptionalBrush : SecondaryBrush);

				TSharedRef<SVerticalBox> ValueContentVerticalBox = SNew(SVerticalBox);

				TOptional<TArray<FRigElementKey>> TargetKeys;
				if (PerModuleInfos.Num() >= 1)
				{
					for (int32 ModuleIndex=0; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
					{
						const FRigHierarchyModulePath ConnectorModulePath(PerModuleInfos[ModuleIndex].ModuleName.ToString(), Connector.Name);
						FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);
						if(const UControlRigBlueprint* Blueprint = PerModuleInfos[ModuleIndex].GetBlueprint())
						{
							const TArray<FRigElementKey> CurrentTargets = Blueprint->ModularRigModel.Connections.FindTargetsFromConnector(ConnectorKey);
							if (!TargetKeys.IsSet())
							{
								TargetKeys = CurrentTargets;
							}
							else
							{
								const TArray<FRigElementKey>& FlatKeys = TargetKeys.GetValue();
								if(FlatKeys.Num() != CurrentTargets.Num())
								{
									TargetKeys.Reset();
									break;
								}

								for(int32 TargetKeyIndex = 0; TargetKeyIndex < CurrentTargets.Num(); TargetKeyIndex++)
								{
									if(FlatKeys[TargetKeyIndex] != CurrentTargets[TargetKeyIndex])
									{
										TargetKeys.Reset();
										break;
									}
								}

								if(!TargetKeys.IsSet())
								{
									break;
								}
							}
						}
					}
				}

				const FRigHierarchyModulePath ConnectorModulePath(PerModuleInfos[0].ModuleName.ToString(), Connector.Name);
				FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);

				FDetailWidgetRow& ConnectorRow = ConnectionsCategory.AddCustomRow(Label)
				.NameContent()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.f, 0.f, 4.f, 0.f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					[
						SNew(SImage)
						.Image(IconBrush)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.DesiredSizeOverride(FVector2D(16, 16))
					]
									
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.f, 0.f, 0.f, 0.f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					[
						SNew(STextBlock)
						.Text(Label)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.IsEnabled(true)
					]
				];

				if(TargetKeys.IsSet())
				{
					TSharedPtr<SHorizontalBox> HorizontalBox;
					
					ConnectorRow
					.ValueContent()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SAssignNew(HorizontalBox, SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 0.f, 0.f)
						[
							SNew(SRigConnectorTargetWidget)
							.Outer(PerModuleInfos[0].GetBlueprint())
							.ConnectorKey(ConnectorKey)
							.IsArray(bIsArrayConnector)
							.ExpandArrayByDefault(true)
							.Targets(TargetKeys.GetValue())
							.OnSetTargetArray(FRigConnectorTargetWidget_SetTargetArray::CreateSP(this, &FRigModuleInstanceDetails::OnConnectorTargetChanged, Connector))
							.RigTreeDelegates(RigTreeDelegates)
						]
					];

					if(!bIsArrayConnector)
					{
						// Reset button
						HorizontalBox->AddSlot()
						.AutoWidth()
						.Padding(4.f, 0.f, 0.f, 0.f)
						.VAlign(VAlign_Center)
						[
							SAssignNew(ResetConnectorButton.FindOrAdd(Connector.Name), SButton)
							.ButtonStyle( FAppStyle::Get(), "NoBorder" )
							.ButtonColorAndOpacity_Lambda([this, Connector]()
							{
								const TSharedPtr<SButton>& Button = ResetConnectorButton.FindRef(Connector.Name);
								return Button.IsValid() && Button->IsHovered()
									? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
									: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
							})
							.OnClicked_Lambda([this, Connector]()
							{
								for (FPerModuleInfo& Info : PerModuleInfos)
								{
									const FRigHierarchyModulePath ConnectorModulePath(Info.ModuleName.ToString(), Connector.Name);
									FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);
									Info.GetBlueprint()->GetModularRigController()->DisconnectConnector(ConnectorKey);
								}
								return FReply::Handled();
							})
							.ContentPadding(1.f)
							.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Reset_Connector", "Reset Connector"))
							[
								SNew(SImage)
								//.DesiredSizeOverride(FVector2D(16,16))
								.ColorAndOpacity_Lambda( [this, Connector]()
								{
									const TSharedPtr<SButton>& Button = ResetConnectorButton.FindRef(Connector.Name);
									return Button.IsValid() && Button->IsHovered()
									? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
									: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
								})
								.Image(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "PropertyWindow.DiffersFromDefault").GetIcon())
							]
						];

						// Use button
						HorizontalBox->AddSlot()
						.AutoWidth()
						.Padding(0.f, 0.f, 0.f, 0.f)
						.VAlign(VAlign_Center)
						[
							SAssignNew(UseSelectedButton.FindOrAdd(Connector.Name), SButton)
							.ButtonStyle( FAppStyle::Get(), "NoBorder" )
							.ButtonColorAndOpacity_Lambda([this, Connector]()
							{
								const TSharedPtr<SButton>& Button = UseSelectedButton.FindRef(Connector.Name);
								return Button.IsValid() && Button->IsHovered()
									? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
									: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
							})
							.OnClicked_Lambda([this, Connector]()
							{
								if (UModularRig* ModularRig = PerModuleInfos[0].GetModularRig())
								{
									const TArray<FRigElementKey>& Selected = ModularRig->GetHierarchy()->GetSelectedKeys();
									if (Selected.Num() > 0)
									{
										for (FPerModuleInfo& Info : PerModuleInfos)
										{
											const FRigHierarchyModulePath ConnectorModulePath(Info.ModuleName.ToString(), Connector.Name);
											FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);
											Info.GetBlueprint()->GetModularRigController()->ConnectConnectorToElements(ConnectorKey, Selected, true, ModularRig->GetModularRigSettings().bAutoResolve);
										}
									}
								}
								return FReply::Handled();
							})
							.ContentPadding(1.f)
							.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Use_Selected", "Use Selected"))
							[
								SNew(SImage)
								//.DesiredSizeOverride(FVector2D(16,16))
								.ColorAndOpacity_Lambda( [this, Connector]()
								{
									const TSharedPtr<SButton>& Button = UseSelectedButton.FindRef(Connector.Name);
									return Button.IsValid() && Button->IsHovered()
									? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
									: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
								})
								.Image(FAppStyle::GetBrush("Icons.CircleArrowLeft"))
							]
						];

						// Select in hierarchy button
						HorizontalBox->AddSlot()
						.AutoWidth()
						.Padding(0.f, 0.f, 0.f, 0.f)
						.VAlign(VAlign_Center)
						[
							SAssignNew(SelectElementButton.FindOrAdd(Connector.Name), SButton)
							.ButtonStyle( FAppStyle::Get(), "NoBorder" )
							.ButtonColorAndOpacity_Lambda([this, Connector]()
							{
								const TSharedPtr<SButton>& Button = SelectElementButton.FindRef(Connector.Name);
								return Button.IsValid() && Button->IsHovered()
									? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
									: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
							})
							.OnClicked_Lambda([this, Connector]()
							{
								if (UModularRig* ModularRig = PerModuleInfos[0].GetModularRig())
								{
									const FRigHierarchyModulePath ConnectorModulePath(PerModuleInfos[0].ModuleName.ToString(), Connector.Name);
									FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);
									if (const FRigElementKeyRedirector::FKeyArray* TargetKeys = ModularRig->GetElementKeyRedirector().FindExternalKey(ConnectorKey))
									{
										TArray<FRigElementKey> KeysToSelect;
										KeysToSelect.Append(*TargetKeys);
										ModularRig->GetHierarchy()->GetController()->SetSelection(KeysToSelect, true);
									}
								}
								return FReply::Handled();
							})
							.ContentPadding(1.f)
							.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Select_Element", "Select Element"))
							[
								SNew(SImage)
								//.DesiredSizeOverride(FVector2D(16,16))
								.ColorAndOpacity_Lambda( [this, Connector]()
								{
									const TSharedPtr<SButton>& Button = SelectElementButton.FindRef(Connector.Name);
									return Button.IsValid() && Button->IsHovered()
									? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
									: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
								})
								.Image(FAppStyle::GetBrush("Icons.Search"))
							]
						];
					}
				}
				else
				{
					ConnectorRow
					.ValueContent()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(ControlRigModuleDetailsMultipleValues)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					];
				}
			}
		}
	}

	for(const FName& OriginalCategoryName : OriginalCategoryNames)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(OriginalCategoryName);
		{
			IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

			TArray<TSharedRef<IPropertyHandle>> DefaultProperties;
			Category.GetDefaultProperties(DefaultProperties, true, true);

			for(const TSharedRef<IPropertyHandle>& DefaultProperty : DefaultProperties)
			{
				const FProperty* Property = DefaultProperty->GetProperty();
				if(Property == nullptr)
				{
					DetailBuilder.HideProperty(DefaultProperty);
					continue;
				}

				// skip advanced properties for now
				const bool bAdvancedDisplay = Property->HasAnyPropertyFlags(CPF_AdvancedDisplay);
				if(bAdvancedDisplay)
				{
					DetailBuilder.HideProperty(DefaultProperty);
					continue;
				}

				// skip non-public properties for now
				const bool bIsPublic = Property->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst);
				const bool bIsInstanceEditable = !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);
				if(!bIsPublic || !bIsInstanceEditable)
				{
					DetailBuilder.HideProperty(DefaultProperty);
					continue;
				}

				TDelegate<void(const FPropertyChangedEvent&)> OnValueChangedDelegate = TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FRigModuleInstanceDetails::OnConfigValueChanged);
				DefaultProperty->SetOnPropertyValueChangedWithData(OnValueChangedDelegate);
				DefaultProperty->SetOnChildPropertyValueChangedWithData(OnValueChangedDelegate);

				FPropertyBindingWidgetArgs BindingArgs;
				BindingArgs.Property = (FProperty*)Property;
				BindingArgs.CurrentBindingText = TAttribute<FText>::CreateLambda([this, Property]()
				{
					return GetBindingText(Property);
				});
				BindingArgs.CurrentBindingImage = TAttribute<const FSlateBrush*>::CreateLambda([this, Property]()
				{
					return GetBindingImage(Property);
				});
				BindingArgs.CurrentBindingColor = TAttribute<FLinearColor>::CreateLambda([this, Property]()
				{
					return GetBindingColor(Property);
				});

				BindingArgs.OnCanBindPropertyWithBindingChain.BindLambda([](const FProperty* InProperty, TConstArrayView<FBindingChainElement> InBindingChain) -> bool { return true; });
				BindingArgs.OnCanBindToClass.BindLambda([](UClass* InClass) -> bool { return false; });
				BindingArgs.OnCanRemoveBinding.BindRaw(this, &FRigModuleInstanceDetails::CanRemoveBinding);
				BindingArgs.OnRemoveBinding.BindSP(this, &FRigModuleInstanceDetails::HandleRemoveBinding);

				BindingArgs.bGeneratePureBindings = true;
				BindingArgs.bAllowNewBindings = true;
				BindingArgs.bAllowArrayElementBindings = false;
				BindingArgs.bAllowStructMemberBindings = false;
				BindingArgs.bAllowUObjectFunctions = false;

				BindingArgs.MenuExtender = MakeShareable(new FExtender);
				BindingArgs.MenuExtender->AddMenuExtension(
					"Properties",
					EExtensionHook::After,
					nullptr,
					FMenuExtensionDelegate::CreateSPLambda(this, [this, Property](FMenuBuilder& MenuBuilder)
					{
						FillBindingMenu(MenuBuilder, Property);
					})
				);

				TSharedPtr<SWidget> ValueWidget = DefaultProperty->CreatePropertyValueWidgetWithCustomization(DetailBuilder.GetDetailsViewSharedPtr().Get());

				const bool bShowChildren = true;
				Category.AddProperty(DefaultProperty).CustomWidget(bShowChildren)
				.NameContent()
				[
					DefaultProperty->CreatePropertyNameWidget()
				]

				.ValueContent()
				[
					ValueWidget ? ValueWidget.ToSharedRef() : SNullWidget::NullWidget
					// todo: if the property is bound / or partially bound
					// mark the property value widget as disabled / read only.
				]

				.ExtensionContent()
				[
					PropertyAccessEditor.MakePropertyBindingWidget(nullptr, BindingArgs)
				];
			}
		}
	}
}

FText FRigModuleInstanceDetails::GetName() const
{
	const FRigModuleInstance* FirstModule = PerModuleInfos[0].GetModule();
	if(FirstModule == nullptr)
	{
		return FText();
	}
	
	const FName FirstValue = FirstModule->Name;
	if(PerModuleInfos.Num() > 1)
	{
		bool bSame = true;
		for (int32 i=1; i<PerModuleInfos.Num(); ++i)
		{
			if(const FRigModuleInstance* Module = PerModuleInfos[i].GetModule())
			{
				if (!Module->Name.IsEqual(FirstValue, ENameCase::IgnoreCase))
				{
					bSame = false;
					break;
				}
			}
		}
		if (!bSame)
		{
			return ControlRigModuleDetailsMultipleValues;
		}
	}
	return FText::FromName(FirstValue);
}

void FRigModuleInstanceDetails::SetName(const FText& InValue, ETextCommit::Type InCommitType, const TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	if(InValue.IsEmpty())
	{
		return;
	}
	
	for (FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				const FName OldModuleName = ModuleInstance->Name;
				(void)Controller->RenameModule(OldModuleName, *InValue.ToString(), true);
			}
		}
	}
}

bool FRigModuleInstanceDetails::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	if(InText.IsEmpty())
	{
		static const FText EmptyNameIsNotAllowed = LOCTEXT("EmptyNameIsNotAllowed", "Empty name is not allowed.");
		OutErrorMessage = EmptyNameIsNotAllowed;
		return false;
	}

	for (FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				if (!Controller->CanRenameModule(ModuleInstance->Name, *InText.ToString(), OutErrorMessage))
				{
					return false;
				}
			}
		}
	}

	return true;
}

FText FRigModuleInstanceDetails::GetRigClassPath() const
{
	if(PerModuleInfos.Num() > 1)
	{
		if(const FRigModuleInstance* FirstModule = PerModuleInfos[0].GetModule())
		{
			bool bSame = true;
			for (int32 i=1; i<PerModuleInfos.Num(); ++i)
			{
				if(const FRigModuleInstance* Module = PerModuleInfos[i].GetModule())
				{
					if (Module->GetRig()->GetClass() != FirstModule->GetRig()->GetClass())
					{
						bSame = false;
						break;
					}
				}
			}
			if (!bSame)
			{
				return ControlRigModuleDetailsMultipleValues;
			}
		}
	}

	if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (const UControlRig* ModuleRig = Module->GetRig())
		{
			return FText::FromString(ModuleRig->GetClass()->GetClassPathName().ToString());
		}
	}

	return FText();
}

FReply FRigModuleInstanceDetails::HandleOpenRigModuleAsset() const
{
	if(PerModuleInfos.Num() == 1)
	{
		if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
		{
			if (const UControlRig* ModuleRig = Module->GetRig())
			{
				if(const UObject* Blueprint = ModuleRig->GetClass()->ClassGeneratedBy)
				{
					const FSoftObjectPath SoftObjectPath(Blueprint);
					TArray<FAssetData> AssetData;
					AssetData.Add(FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssetByObjectPath(SoftObjectPath.GetWithoutSubPath()));
					FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().SyncBrowserToAssets(AssetData);
					return FReply::Handled();
				}
			}
		}
	}
	return FReply::Unhandled();
}

TArray<FRigModuleConnector> FRigModuleInstanceDetails::GetConnectors() const
{
	if(PerModuleInfos.Num() > 1)
	{
		TArray<FRigModuleConnector> CommonConnectors;
		if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
		{
			if (const UControlRig* ModuleRig = Module->GetRig())
			{
				CommonConnectors = ModuleRig->GetRigModuleSettings().ExposedConnectors;
			}
		}
		for (int32 ModuleIndex=1; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
		{
			if (const FRigModuleInstance* Module = PerModuleInfos[ModuleIndex].GetModule())
			{
				if (const UControlRig* ModuleRig = Module->GetRig())
				{
					const TArray<FRigModuleConnector>& ModuleConnectors = ModuleRig->GetRigModuleSettings().ExposedConnectors;
					CommonConnectors = CommonConnectors.FilterByPredicate([ModuleConnectors](const FRigModuleConnector& Connector)
					{
						return ModuleConnectors.Contains(Connector);
					});
				}
			}
		}
		return CommonConnectors;
	}

	if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (const UControlRig* ModuleRig = Module->GetRig())
		{
			return ModuleRig->GetRigModuleSettings().ExposedConnectors;
		}
	}

	return TArray<FRigModuleConnector>();
}

FRigElementKeyRedirector FRigModuleInstanceDetails::GetConnections() const
{
	if(PerModuleInfos.Num() > 1)
	{
		return FRigElementKeyRedirector();
	}

	if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (UControlRig* ModuleRig = Module->GetRig())
		{
			return ModuleRig->GetElementKeyRedirector();
		}
	}

	return FRigElementKeyRedirector();
}

void FRigModuleInstanceDetails::OnConfigValueChanged(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	UControlRigBlueprint* Blueprint = PerModuleInfos[0].GetBlueprint();
	if(Blueprint == nullptr)
	{
		return;
	}

	FString PropertyPathString;
	{
		TSharedRef< FPropertyPath > PropertyPath = FPropertyPath::CreateEmpty();
		FProperty* Property = InPropertyChangedEvent.Property;
		if(!ensure(Property))
		{
			return;
		}

		TMap<FString, int32> PropertyNameStack;
		InPropertyChangedEvent.GetArrayIndicesPerObject(0, PropertyNameStack);

		if (PropertyNameStack.Num() > 0)
		{
			TArray<FString> PropertyNames;
			PropertyNameStack.GetKeys(PropertyNames);

			// the property names in the array are provided in reverse order
			for(int32 Index = PropertyNames.Num() - 1; Index >= 0; Index--)
			{
				if(!PropertyPath->IsValid())
				{
					ensure(InPropertyChangedEvent.GetNumObjectsBeingEdited() > 0);
					Property = FControlRigOverrideValue::FindProperty(InPropertyChangedEvent.GetObjectBeingEdited(0)->GetClass(), PropertyNames[Index]);
					if(!ensure(Property != nullptr))
					{
						return;
					}
				}
				else
				{
					if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
					{
						Property = ArrayProperty->Inner;
					}
				
					const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
					if(!ensure(StructProperty != nullptr))
					{
						return;
					}

					Property = FControlRigOverrideValue::FindProperty(StructProperty->Struct, PropertyNames[Index]);
					if(!ensure(Property))
					{
						return;
					}
				}
				
				PropertyPath->AddProperty(
					{
						TWeakFieldPtr<FProperty>(Property),
						InPropertyChangedEvent.GetArrayIndex(PropertyNames[Index])
					}
				);
			}
		}
		else
		{
			PropertyPath->AddProperty(
				{
					TWeakFieldPtr<FProperty>(InPropertyChangedEvent.MemberProperty),
					InPropertyChangedEvent.GetArrayIndex(InPropertyChangedEvent.GetMemberPropertyName().ToString())
				}
			);
		}

		PropertyPathString = PropertyPath->ToString();
	}
	
	// we need to shorten the path if we already have data on something above this.
	bool bFoundValueWithShorterPath = true;
	while(bFoundValueWithShorterPath)
	{
		bFoundValueWithShorterPath = false;

		for(const FPerModuleInfo& Info : PerModuleInfos)
		{
			if (const FRigModuleReference* ModuleReference = Blueprint->ModularRigModel.FindModule(Info.ModuleName))
			{
				for(const FControlRigOverrideValue& Override : ModuleReference->ConfigOverrides)
				{
					const FString& ParentPath = Override.GetPath();
					if(FControlRigOverrideContainer::IsChildPathOf(PropertyPathString, ParentPath))
					{
						bFoundValueWithShorterPath = true;
						PropertyPathString = ParentPath;
						break;
					}
				}
			}
			
			if(bFoundValueWithShorterPath)
			{
				break;
			}
		}
	}

	TMap<FName, FControlRigOverrideValue> ModuleValues;
	ModuleValues.Reserve(PerModuleInfos.Num());
	
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if (const UControlRig* ModuleRig = ModuleInstance->GetRig())
			{
				FControlRigOverrideValue ConfigValue = FControlRigOverrideValue(PropertyPathString, ModuleRig);
				ModuleValues.Add(ModuleInstance->Name, ConfigValue);
			}
		}
	}

	FScopedTransaction Transaction(LOCTEXT("ModuleHierarchySetConfigValue", "Set Module Config Value"));
	for (const TPair<FName, FControlRigOverrideValue>& Value : ModuleValues)
	{
		UModularRigController* Controller = Blueprint->GetModularRigController();
		Controller->SetConfigValueInModule(Value.Key, Value.Value);
	}
}

bool FRigModuleInstanceDetails::OnConnectorTargetChanged(TArray<FRigElementKey> InTargets, FRigModuleConnector InConnector)
{
	bool bResult = true;
	FScopedTransaction Transaction(LOCTEXT("ModuleHierarchyResolveConnector", "Resolve Connector"));
	for (FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UModularRigController* Controller = Info.GetBlueprint()->GetModularRigController())
		{
			const FRigHierarchyModulePath ConnectorModulePath(Info.ModuleName.ToString(), InConnector.Name);
			FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);
			
			if (!InTargets.IsEmpty())
			{
				const FModularRigSettings& Settings = Info.GetModularRig()->GetModularRigSettings();
				if(!Controller->ConnectConnectorToElements(ConnectorKey, InTargets, true, Settings.bAutoResolve))
				{
					bResult = false;
				}
			}
			else
			{
				if(!Controller->DisconnectConnector(ConnectorKey))
				{
					bResult = false;
				}
			}
		}
	}
	return bResult;
}

const FRigModuleInstanceDetails::FPerModuleInfo& FRigModuleInstanceDetails::FindModule(const FName& InModuleName) const
{
	const FPerModuleInfo* Info = FindModuleByPredicate([InModuleName](const FPerModuleInfo& Info)
	{
		if(const FRigModuleInstance* Module = Info.GetModule())
		{
			return Module->Name == InModuleName;
		}
		return false;
	});

	if(Info)
	{
		return *Info;
	}

	static const FPerModuleInfo EmptyInfo;
	return EmptyInfo;
}

const FRigModuleInstanceDetails::FPerModuleInfo* FRigModuleInstanceDetails::FindModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const
{
	return PerModuleInfos.FindByPredicate(InPredicate);
}

bool FRigModuleInstanceDetails::ContainsModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const
{
	return PerModuleInfos.ContainsByPredicate(InPredicate);
}

void FRigModuleInstanceDetails::RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule, UClass* InClass)
{
	TSharedRef<FPropertySection> MetadataSection = PropertyEditorModule.FindOrCreateSection(InClass->GetFName(), "Metadata", LOCTEXT("Metadata", "Metadata"));
	MetadataSection->AddCategory("Metadata");
}

FText FRigModuleInstanceDetails::GetBindingText(const FProperty* InProperty) const
{
	const FName VariableName = InProperty->GetFName();
	FText FirstValue;
	for (int32 ModuleIndex=0; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
	{
		if (const FRigModuleReference* ModuleReference = PerModuleInfos[ModuleIndex].GetReference())
		{
			if(ModuleReference->Bindings.Contains(VariableName))
			{
				const FText BindingText = FText::FromString(ModuleReference->Bindings.FindChecked(VariableName));
				if(ModuleIndex == 0)
				{
					FirstValue = BindingText;
				}
				else if(!FirstValue.EqualTo(BindingText))
				{
					return ControlRigModuleDetailsMultipleValues;
				}
			}
		}
	}
	return FirstValue;
}

const FSlateBrush* FRigModuleInstanceDetails::GetBindingImage(const FProperty* InProperty) const
{
	static const FLazyName TypeIcon(TEXT("Kismet.VariableList.TypeIcon"));
	static const FLazyName ArrayTypeIcon(TEXT("Kismet.VariableList.ArrayTypeIcon"));

	if(CastField<FArrayProperty>(InProperty))
	{
		return FAppStyle::GetBrush(ArrayTypeIcon);
	}
	return FAppStyle::GetBrush(TypeIcon);
}

FLinearColor FRigModuleInstanceDetails::GetBindingColor(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FEdGraphPinType PinType;
		const UEdGraphSchema_K2* Schema_K2 = GetDefault<UEdGraphSchema_K2>();
		if (Schema_K2->ConvertPropertyToPinType(InProperty, PinType))
		{
			const URigVMEdGraphSchema* Schema = GetDefault<URigVMEdGraphSchema>();
			return Schema->GetPinTypeColor(PinType);
		}
	}
	return FLinearColor::White;
}

void FRigModuleInstanceDetails::FillBindingMenu(FMenuBuilder& MenuBuilder, const FProperty* InProperty) const
{
	if(PerModuleInfos.IsEmpty())
	{
		return;
	}

	UControlRigBlueprint* Blueprint = PerModuleInfos[0].GetBlueprint();
	UModularRigController* Controller = Blueprint->GetModularRigController();

	TArray<FString> CombinedBindings;
	for(int32 Index = 0; Index < PerModuleInfos.Num(); Index++)
	{
		const FPerModuleInfo& Info  = PerModuleInfos[Index];
		const TArray<FString> Bindings = Controller->GetPossibleBindings(Info.ModuleName, InProperty->GetFName());
		if(Index == 0)
		{
			CombinedBindings = Bindings;
		}
		else
		{
			// reduce the set of bindings to the overall possible bindings
			CombinedBindings.RemoveAll([Bindings](const FString& Binding)
			{
				return !Bindings.Contains(Binding);
			});
		}
	}

	if(CombinedBindings.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(
			FUIAction(FExecuteAction()),
			SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoBindingAvailable", "No bindings available for this property."))
					.ColorAndOpacity(FLinearColor::White)
				]
			);
		return;
	}

	// sort lexically
	CombinedBindings.Sort();

	// create a map of all of the variables per menu prefix (the module path the variables belong to)
	struct FPerMenuData
	{
		FString Name;
		FString ParentMenuPath;
		TArray<FString> SubMenuPaths;
		TArray<FString> Variables;

		static void SetupMenu(
			TSharedRef<FRigModuleInstanceDetails const> ThisDetails,
			const FProperty* InProperty,
			FMenuBuilder& InMenuBuilder,
			const FString& InMenuPath,
			TSharedRef<TMap<FString, FPerMenuData>> PerMenuData)
		{
			FPerMenuData& Data = PerMenuData->FindChecked((InMenuPath));

			Data.SubMenuPaths.Sort();
			Data.Variables.Sort();

			for(const FString& VariablePath : Data.Variables)
			{
				FString VariableName = VariablePath;
				(void)FRigHierarchyModulePath(VariablePath).Split(nullptr, &VariableName);
				
				InMenuBuilder.AddMenuEntry(
					FUIAction(FExecuteAction::CreateLambda([ThisDetails, InProperty, VariablePath]()
					{
						ThisDetails->HandleChangeBinding(InProperty, VariablePath);
					})),
					SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(1.0f, 0.0f)
						[
							SNew(SImage)
							.Image(ThisDetails->GetBindingImage(InProperty))
							.ColorAndOpacity(ThisDetails->GetBindingColor(InProperty))
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(VariableName))
							.ColorAndOpacity(FLinearColor::White)
						]
					);
			}

			for(const FString& SubMenuPath : Data.SubMenuPaths)
			{
				const FPerMenuData& SubMenuData = PerMenuData->FindChecked(SubMenuPath);

				const FText Label = FText::FromString(SubMenuData.Name);
				static const FText TooltipFormat = LOCTEXT("BindingMenuTooltipFormat", "Access to all variables of the {0} module");
				const FText Tooltip = FText::Format(TooltipFormat, Label);  
				InMenuBuilder.AddSubMenu(Label, Tooltip, FNewMenuDelegate::CreateLambda([ThisDetails, InProperty, SubMenuPath, PerMenuData](FMenuBuilder& SubMenuBuilder)
				{
					SetupMenu(ThisDetails, InProperty, SubMenuBuilder, SubMenuPath, PerMenuData);
				}));
			}
		}
	};
	
	// define the root menu
	const TSharedRef<TMap<FString, FPerMenuData>> PerMenuData = MakeShared<TMap<FString, FPerMenuData>>();
	PerMenuData->FindOrAdd(FString());

	// make sure all levels of the menu are known and we have the variables available
	for(const FString& BindingPath : CombinedBindings)
	{
		FString MenuPath;
		(void)FRigHierarchyModulePath(BindingPath).Split(&MenuPath, nullptr);

		FString PreviousMenuPath = MenuPath;
		FString ParentMenuPath = MenuPath, RemainingPath;
		while(FRigHierarchyModulePath(ParentMenuPath).Split(&ParentMenuPath, &RemainingPath))
		{
			// scope since the map may change at the end of this block
			{
				FPerMenuData& Data = PerMenuData->FindOrAdd(MenuPath);
				if(Data.Name.IsEmpty())
				{
					Data.Name = RemainingPath;
				}
			}
			
			PerMenuData->FindOrAdd(ParentMenuPath).SubMenuPaths.AddUnique(PreviousMenuPath);
			PerMenuData->FindOrAdd(PreviousMenuPath).ParentMenuPath = ParentMenuPath;
			PerMenuData->FindOrAdd(PreviousMenuPath).Name = RemainingPath;
			if(!ParentMenuPath.Contains(FRigHierarchyModulePath::ModuleNameSuffix))
			{
				PerMenuData->FindOrAdd(FString()).SubMenuPaths.AddUnique(ParentMenuPath);
				PerMenuData->FindOrAdd(ParentMenuPath).Name = ParentMenuPath;
			}
			PreviousMenuPath = ParentMenuPath;
		}

		FPerMenuData& Data = PerMenuData->FindOrAdd(MenuPath);
		if(Data.Name.IsEmpty())
		{
			Data.Name = MenuPath;
		}

		Data.Variables.Add(BindingPath);
		if(!MenuPath.IsEmpty())
		{
			PerMenuData->FindChecked(Data.ParentMenuPath).SubMenuPaths.AddUnique(MenuPath);
		}
	}

	// build the menu
	FPerMenuData::SetupMenu(SharedThis(this), InProperty, MenuBuilder, FString(), PerMenuData);
}

bool FRigModuleInstanceDetails::CanRemoveBinding(FName InPropertyName) const
{
	// offer the "removing binding" button if any of the selected module instances
	// has a binding for the given variable
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if(ModuleInstance->VariableBindings.Contains(InPropertyName))
			{
				return true;
			}
		}
	}
	return false; 
}

void FRigModuleInstanceDetails::HandleRemoveBinding(FName InPropertyName) const
{
	FScopedTransaction Transaction(LOCTEXT("RemoveModuleVariableTransaction", "Remove Binding"));
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				Controller->UnBindModuleVariable(ModuleInstance->Name, InPropertyName);
			}
		}
	}
}

void FRigModuleInstanceDetails::HandleChangeBinding(const FProperty* InProperty, const FString& InNewVariablePath) const
{
	FScopedTransaction Transaction(LOCTEXT("BindModuleVariableTransaction", "Bind Module Variable"));
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				Controller->BindModuleVariable(ModuleInstance->Name, InProperty->GetFName(), InNewVariablePath);
			}
		}
	}
}

FReply FRigModuleInstanceDetails::OnAddTargetToArrayConnector(const FString InConnectorName, const TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	FScopedTransaction Transaction(LOCTEXT("AddTargetToArrayConnector", "Add Target To Array Connector"));
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				const FRigHierarchyModulePath ConnectorModulePath(Info.ModuleName.ToString(), InConnectorName);
				FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);
				Controller->AddTargetToArrayConnector(ConnectorKey, FRigElementKey(), true, false, false);
				Blueprint->RecompileModularRig();
			}
		}
	}
	PropertyUtilities->RequestForceRefresh();
	return FReply::Handled();
}

FReply FRigModuleInstanceDetails::OnClearTargetsForArrayConnector(const FString InConnectorName, const TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	FScopedTransaction Transaction(LOCTEXT("TransactionClearTargetsForArrayConnector", "Clear Targets For Array Connector"));
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				const FRigHierarchyModulePath ConnectorModulePath(Info.ModuleName.ToString(), InConnectorName);
				FRigElementKey ConnectorKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);
				Controller->DisconnectConnector(ConnectorKey);
			}
		}
	}
	PropertyUtilities->RequestForceRefresh();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
