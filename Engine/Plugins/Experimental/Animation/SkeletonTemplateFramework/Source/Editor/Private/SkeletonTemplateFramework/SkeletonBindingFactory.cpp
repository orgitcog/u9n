// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateFramework/SkeletonBindingFactory.h"


#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "SkeletonTemplateFramework/SkeletonBinding.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "SkeletonBindingFactory"

class SSkeletonBindingCreateDialog final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSkeletonBindingCreateDialog){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		bOkClicked = false;

		ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FAppStyle::GetBrush("ChildWindow.Background"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10.0f)
						[
							SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 0.0f, 0.0f, 5.0f)
								[
									CreateSkeletonPicker()
								]
						]
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("NewAnimBlueprintDialog.AreaBorder"))
							[
								CreateSkeletonTemplatePicker()
							]
						]
				]

				// Create/Cancel buttons
				+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(10.0f)
					[
						SNew(SUniformGridPanel)
							.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
							.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
							.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
							+ SUniformGridPanel::Slot(0,0)
							[
								SNew(SButton)
									.ToolTipText(LOCTEXT("CreateButtonTooltip", "Create a new Skeleton Binding"))
									.IsEnabled_Lambda([this]()
										{
											return SkeletonAsset.IsValid() && SkeletonTemplateAsset.IsValid();
										})
									.HAlign(HAlign_Center)
									.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
									.OnClicked(this, &SSkeletonBindingCreateDialog::OnCreateClicked)
									.Text(LOCTEXT("CreateButtonText", "Create"))
							]
						+ SUniformGridPanel::Slot(1,0)
						[
							SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
								.OnClicked(this, &SSkeletonBindingCreateDialog::OnCancelClicked)
								.Text(LOCTEXT("CancelButtonText", "Cancel"))
						]
					]
			]
		];
	}
	
	bool ConfigureProperties(TObjectPtr<USkeletonBindingFactory> InFactory)
	{
		Factory = InFactory;

		const TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("CreateSkeletonBinding", "Create Skeleton Binding"))
			.SizingRule(ESizingRule::UserSized)
			.MinWidth(500.0f)
			.MinHeight(800.0f)
			.SupportsMinimize(false)
			.SupportsMaximize(false)
			[
				AsShared()
			];

		PickerWindow = Window;

		GEditor->EditorAddModalWindow(Window);
		return bOkClicked;
	}

private:
	TSharedRef<SWidget> CreateSkeletonTemplatePicker()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.Filter.ClassPaths.Add(USkeletonTemplate::StaticClass()->GetClassPathName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData)
			{
				SkeletonTemplateAsset = AssetData;
			});
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.InitialAssetSelection = SkeletonTemplateAsset;
		AssetPickerConfig.bShowPathInColumnView = false;
		AssetPickerConfig.bShowTypeInColumnView = false;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = false;
	
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(5.0f)
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				];
	}

	TSharedRef<SWidget> CreateSkeletonPicker()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.Filter.ClassPaths.Add(USkeleton::StaticClass()->GetClassPathName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData)
			{
				SkeletonAsset = AssetData;
			});
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.InitialAssetSelection = SkeletonAsset;
		AssetPickerConfig.HiddenColumnNames =
		{
			"DiskSize",
			"AdditionalPreviewSkeletalMeshes",
			"PreviewSkeletalMesh"
		};
		AssetPickerConfig.bShowPathInColumnView = false;
		AssetPickerConfig.bShowTypeInColumnView = false;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = false;
	
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(5.0f)
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				];
	}
	
	void CloseDialog(bool bWasPicked=false)
	{
		bOkClicked = bWasPicked;
		if (PickerWindow.IsValid())
		{
			PickerWindow.Pin()->RequestDestroyWindow();
		}
	}

	FReply OnCreateClicked()
	{
		Factory->Skeleton = Cast<USkeleton>(SkeletonAsset.GetAsset());
		Factory->SkeletonTemplate = Cast<USkeletonTemplate>(SkeletonTemplateAsset.GetAsset());
		
		CloseDialog(true);
		return FReply::Handled();
	}
	
	FReply OnCancelClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			CloseDialog();
			return FReply::Handled();
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	TObjectPtr<USkeletonBindingFactory> Factory;

	TWeakPtr<SWindow> PickerWindow;

	FAssetData SkeletonTemplateAsset;

	FAssetData SkeletonAsset;
	
	bool bOkClicked;
};

USkeletonBindingFactory::USkeletonBindingFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = USkeletonBinding::StaticClass();
}

bool USkeletonBindingFactory::ConfigureProperties()
{
	const TSharedRef<SSkeletonBindingCreateDialog> Dialog = SNew(SSkeletonBindingCreateDialog);
	return Dialog->ConfigureProperties(this);
}

UObject* USkeletonBindingFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	EObjectFlags FlagsToUse = Flags | RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted;
	if (InParent == GetTransientPackage())
	{
		FlagsToUse &= ~RF_Standalone;
	}

	USkeletonBinding* NewBinding = NewObject<USkeletonBinding>(InParent, Class, Name, FlagsToUse);
	NewBinding->InitializeFrom(SkeletonTemplate, Skeleton);

	return NewBinding;
}

#undef LOCTEXT_NAMESPACE