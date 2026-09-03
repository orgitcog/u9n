// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateIM.h"

#include "Misc/SlateIMLogging.h"
#include "Misc/SlateIMManager.h"
#include "Misc/SlateIMSlotData.h"
#include "Misc/SlateIMWidgetScope.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SImButton.h"
#include "Widgets/SImCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_ENGINE
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SlateMaterialBrush.h"
#endif

namespace SlateIM
{
	template<typename DataType>
	struct FSlateIMDataStore : ISlateMetaData
	{
		SLATE_METADATA_TYPE(FSlateIMDataStore, ISlateMetaData);

		FSlateIMDataStore(DataType&& InData)
			: Data(MoveTemp(InData))
		{}

		FSlateIMDataStore(const DataType& InData)
			: Data(InData)
		{}

		DataType Data;
	};
	
	void Text(const FStringView& InText, const FTextBlockStyle* TextStyle /*= nullptr*/)
	{
		Text(InText, FSlateColor::UseForeground(), TextStyle);
	}

	void Text(const FStringView& InText, FSlateColor Color, const FTextBlockStyle* TextStyle /*= nullptr*/)
	{
		FWidgetScope<STextBlock> Scope;
		TSharedPtr<STextBlock> TextBlock = Scope.GetWidget();

		Scope.HashData(Color);
		Scope.HashData(TextStyle);
		Scope.HashStringView(InText);

		if (!TextBlock)
		{
			TextBlock =
				SNew(STextBlock)
				.Text(FText::FromStringView(InText))
				.ColorAndOpacity(Color);

			if (TextStyle)
			{
				TextBlock->SetTextStyle(TextStyle);
			}

			Scope.UpdateWidget(TextBlock);
		}
		else if(Scope.IsDataHashDirty())
		{
			TextBlock->SetText(FText::FromStringView(InText));
			TextBlock->SetTextStyle(TextStyle);
			TextBlock->SetColorAndOpacity(Color);
		}
	}

	bool EditableText(FString& InOutText, const FStringView& HintText, const FEditableTextBoxStyle* TextStyle)
	{
		FWidgetScope<SEditableTextBox> Scope;
		TSharedPtr<SEditableTextBox> EditableText = Scope.GetWidget();
		const float MinWidth = FSlateIMManager::Get().NextMinWidth.Get(Defaults::InputWidgetWidth);

		bool bWasActivated = false;

		Scope.HashData(TextStyle);
		Scope.HashStringView(InOutText);
		Scope.HashStringView(HintText);

		if (!EditableText)
		{
			TWeakPtr<FSlateIMWidgetActivationMetadata> ActivationData = Scope.GetOrCreateActivationMetadata().ToWeakPtr();

			EditableText =
				SNew(SEditableTextBox)
				.MinDesiredWidth(MinWidth)
				.Text(FText::FromStringView(InOutText))
				.HintText(FText::FromStringView(HintText))
				.OnTextChanged_Lambda([ActivationData](const FText& NewText)
				{
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				})
				.OnTextCommitted_Lambda([ActivationData](const FText& NewText, ETextCommit::Type)
				{
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				});

			if (TextStyle)
			{
				EditableText->SetStyle(TextStyle);
			}
			
			Scope.UpdateWidget(EditableText);
		}
		else
		{
			bWasActivated = Scope.IsActivatedThisFrame();

			const bool bIsHashDirty = Scope.IsDataHashDirty();

			if (bIsHashDirty)
			{
				EditableText->SetStyle(TextStyle);
				EditableText->SetHintText(FText::FromStringView(HintText));
			}
			
			if (bWasActivated)
			{
				InOutText = EditableText->GetText().ToString();
			}
			else if (bIsHashDirty)
			{
				EditableText->SetText(FText::FromStringView(InOutText));
			}
			
			EditableText->SetMinimumDesiredWidth(MinWidth);
		}

		return bWasActivated;
	}

	void Image_Internal(const FSlateColor& ColorAndOpacity, FVector2D DesiredSize, TFunctionRef<const FSlateBrush*(const TSharedRef<SImage>&)> GetBrush)
	{
		FWidgetScope<SImage> Scope(Defaults::Padding, HAlign_Left, VAlign_Top);
		TSharedPtr<SImage> ImageWidget = Scope.GetWidget();

		if (!ImageWidget)
		{
			ImageWidget =
				SNew(SImage)
				.ColorAndOpacity(ColorAndOpacity)
				.DesiredSizeOverride(DesiredSize == FVector2D::ZeroVector ? TOptional<FVector2D>() : DesiredSize);

			ImageWidget->SetImage(GetBrush(ImageWidget.ToSharedRef()));
			Scope.UpdateWidget(ImageWidget);
		}
		else
		{
			ImageWidget->SetImage(GetBrush(ImageWidget.ToSharedRef()));
			ImageWidget->SetColorAndOpacity(ColorAndOpacity);
			ImageWidget->SetDesiredSizeOverride(DesiredSize == FVector2D::ZeroVector ? TOptional<FVector2D>() : DesiredSize);
		}
	}

	void Image(const FSlateBrush* ImageBrush, const FSlateColor& ColorAndOpacity, FVector2D DesiredSize)
	{
		Image_Internal(ColorAndOpacity, DesiredSize, [ImageBrush](const TSharedRef<SImage>&) { return ImageBrush; });
	}

	void Image(const FName ImageStyleName, const FSlateColor& ColorAndOpacity, FVector2D DesiredSize)
	{
		Image(FAppStyle::Get().GetBrush(ImageStyleName), ColorAndOpacity, DesiredSize);
	}

#if WITH_ENGINE
	struct FPinnedImageResource
	{
		FSlateBrush Brush;
		TStrongObjectPtr<UObject> PinnedResource;
	};
	using FUObjectImageResource = FSlateIMDataStore<FPinnedImageResource>;
	void Image(UTexture2D* ImageTexture, const FSlateColor& ColorAndOpacity, FVector2D DesiredSize)
	{
		auto MakeTextureBrush = [ImageTexture]() -> FSlateBrush
		{
			if (ImageTexture)
			{
				FSlateBrush Brush;
				Brush.SetResourceObject(ImageTexture);
				Brush.ImageSize = FVector2D(ImageTexture->GetSizeX(), ImageTexture->GetSizeY());
				return Brush;
			}
			return FSlateNoResource();
		};
		
		Image_Internal(ColorAndOpacity, DesiredSize, [&MakeTextureBrush, ImageTexture](const TSharedRef<SImage>& ImageWidget)
		{
			TSharedPtr<FUObjectImageResource> Resource = ImageWidget->GetMetaData<FUObjectImageResource>();
			if (!Resource)
			{
				Resource = MakeShared<FUObjectImageResource>(FPinnedImageResource{ MakeTextureBrush(), TStrongObjectPtr<UObject>(ImageTexture) });
				ImageWidget->AddMetadata(Resource.ToSharedRef());
			}
			else if (Resource->Data.PinnedResource.Get() != ImageTexture)
			{
				Resource->Data = FPinnedImageResource{ MakeTextureBrush(), TStrongObjectPtr<UObject>(ImageTexture) };
			}

			return &Resource->Data.Brush;
		});
	}

	void Image(UTextureRenderTarget2D* ImageRenderTarget, const FSlateColor& ColorAndOpacity, FVector2D DesiredSize)
	{
		auto MakeRTBrush = [ImageRenderTarget]() -> FSlateBrush
		{
			if (ImageRenderTarget)
			{
				FSlateBrush Brush;
				Brush.SetResourceObject(ImageRenderTarget);
				Brush.ImageSize = FVector2D(ImageRenderTarget->SizeX, ImageRenderTarget->SizeY);
				return Brush;
			}
			return FSlateNoResource();
		};
		
		Image_Internal(ColorAndOpacity, DesiredSize, [&MakeRTBrush, ImageRenderTarget](const TSharedRef<SImage>& ImageWidget)
		{
			TSharedPtr<FUObjectImageResource> Resource = ImageWidget->GetMetaData<FUObjectImageResource>();
			if (!Resource)
			{
				Resource = MakeShared<FUObjectImageResource>(FPinnedImageResource{ MakeRTBrush(), TStrongObjectPtr<UObject>(ImageRenderTarget) });
				ImageWidget->AddMetadata(Resource.ToSharedRef());
			}
			else if (Resource->Data.PinnedResource.Get() != ImageRenderTarget)
			{
				Resource->Data = FPinnedImageResource{ MakeRTBrush(), TStrongObjectPtr<UObject>(ImageRenderTarget) };
			}

			return &Resource->Data.Brush;
		});
	}

	void Image(UMaterialInterface* ImageMaterial, FVector2D BrushSize, const FSlateColor& ColorAndOpacity, FVector2D DesiredSize)
	{
		auto MakeMaterialBrush = [ImageMaterial, &BrushSize]() -> FSlateBrush
		{
			if (ImageMaterial)
			{
				return FSlateMaterialBrush(*ImageMaterial, BrushSize);
			}
			return FSlateNoResource();
		};
		
		Image_Internal(ColorAndOpacity, DesiredSize, [&MakeMaterialBrush, ImageMaterial](const TSharedRef<SImage>& ImageWidget)
		{
			TSharedPtr<FUObjectImageResource> Resource = ImageWidget->GetMetaData<FUObjectImageResource>();
			if (!Resource)
			{
				Resource = MakeShared<FUObjectImageResource>(FPinnedImageResource{ MakeMaterialBrush(), TStrongObjectPtr<UObject>(ImageMaterial) });
				ImageWidget->AddMetadata(Resource.ToSharedRef());
			}
			else if (Resource->Data.PinnedResource.Get() != ImageMaterial)
			{
				Resource->Data = FPinnedImageResource{ MakeMaterialBrush(), TStrongObjectPtr<UObject>(ImageMaterial) };
			}

			return &Resource->Data.Brush;
		});
	}
#endif

	bool Button(const FStringView& InText, const FButtonStyle* InStyle)
	{
		FWidgetScope<SImButton> Scope;
		TSharedPtr<SImButton> ButtonWidget = Scope.GetWidget();

		Scope.HashStringView(InText);
		Scope.HashData(InStyle);

		bool bWasClicked = false;
		if (!ButtonWidget)
		{
			ButtonWidget =
				SNew(SImButton)
				.OnClicked_Lambda([ActivationData = Scope.GetOrCreateActivationMetadata().ToWeakPtr()]()
				{
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
					return FReply::Handled();
				});

			ButtonWidget->SetText(InText);

			if (InStyle)
			{
				ButtonWidget->SetButtonStyle(InStyle);
			}

			Scope.UpdateWidget(ButtonWidget);
		}
		else
		{
			bWasClicked = Scope.IsActivatedThisFrame();

			if (Scope.IsDataHashDirty())
			{
				ButtonWidget->SetText(InText);
				if (InStyle)
				{
					ButtonWidget->SetButtonStyle(InStyle);
				}
			}
		}

		return bWasClicked;
	}

	bool CheckBox(const FStringView& InText, bool& InOutCurrentState, const FCheckBoxStyle* CheckBoxStyle)
	{
		ECheckBoxState CurrentEnumState = InOutCurrentState ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

		const bool bValueChanged = CheckBox(InText, CurrentEnumState, CheckBoxStyle);
		if (bValueChanged)
		{
			InOutCurrentState = CurrentEnumState == ECheckBoxState::Checked;
		}

		return bValueChanged;
	}

	bool CheckBox(const FStringView& InText, ECheckBoxState& InOutCurrentState, const FCheckBoxStyle* CheckBoxStyle)
	{
		FWidgetScope<SImCheckBox> Scope;
		TSharedPtr<SImCheckBox> CheckboxWidget = Scope.GetWidget();

		bool bValueChanged = false;

		Scope.HashStringView(InText);
		Scope.HashData(CheckBoxStyle);

		if (!CheckboxWidget)
		{
			CheckboxWidget =
				SNew(SImCheckBox)
				.IsChecked(InOutCurrentState)
				.OnCheckStateChanged_Lambda([ActivationData = Scope.GetOrCreateActivationMetadata().ToWeakPtr()](ECheckBoxState NewState)
				{
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				});

			CheckboxWidget->SetText(FText::FromStringView(InText));

			if (CheckBoxStyle)
			{
				CheckboxWidget->SetStyle(CheckBoxStyle);
			}

			Scope.UpdateWidget(CheckboxWidget);
		}
		else
		{
			bValueChanged = Scope.IsActivatedThisFrame();
			if (bValueChanged)
			{
				InOutCurrentState = CheckboxWidget->GetCheckedState();
			}
			else
			{
				CheckboxWidget->SetIsChecked(InOutCurrentState);
			}

			if (Scope.IsDataHashDirty())
			{
				CheckboxWidget->SetText(FText::FromStringView(InText));
				if (CheckBoxStyle)
				{
					CheckboxWidget->SetStyle(CheckBoxStyle);
				}
			}
		}

		return bValueChanged;
	}

	template<typename NumericType>
	bool SpinBox_Internal(NumericType& InOutValue, TOptional<NumericType> Min, TOptional<NumericType> Max, const FSpinBoxStyle* SpinBoxStyle = nullptr)
	{
		FWidgetScope<SSpinBox<NumericType>> Scope(Defaults::Padding, Defaults::HAlign, Defaults::VAlign, Defaults::bAutoSize, Defaults::InputWidgetWidth);
		TSharedPtr<SSpinBox<NumericType>> SpinBoxWidget = Scope.GetWidget();

		Scope.HashData(SpinBoxStyle);

		class FSpinBoxState : public ISlateMetaData
		{
		public:
			SLATE_METADATA_TYPE(FSpinBoxState, ISlateMetaData)
			bool bIsChanging = false;
		};

		bool bValueChanged = false;
		if (!SpinBoxWidget)
		{
			TSharedRef<FSpinBoxState> SpinBoxState = MakeShared<FSpinBoxState>();
			TWeakPtr<FSlateIMWidgetActivationMetadata> ActivationData = Scope.GetOrCreateActivationMetadata().ToWeakPtr();
			SpinBoxWidget =
				SNew(SSpinBox<NumericType>)
				.MinValue(Min)
				.MaxValue(Max)
				.Value(InOutValue)
				.OnBeginSliderMovement_Lambda([SpinBoxState](){ SpinBoxState->bIsChanging = true; })
				.OnEndSliderMovement_Lambda([SpinBoxState, ActivationData](NumericType NewVal)
				{
					SpinBoxState->bIsChanging = false;
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				})
				.OnValueChanged_Lambda([ActivationData](NumericType NewVal)
				{
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				})
				.OnValueCommitted_Lambda([ActivationData](NumericType NewVal, ETextCommit::Type CommitType)
				{
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				});

			SpinBoxWidget->AddMetadata(SpinBoxState);
			Scope.UpdateWidget(SpinBoxWidget);

			if (SpinBoxStyle)
			{
				SpinBoxWidget->SetWidgetStyle(SpinBoxStyle);
			}
		}
		else
		{
			SpinBoxWidget->SetMinValue(Min);
			SpinBoxWidget->SetMaxValue(Max);

			bValueChanged = Scope.IsActivatedThisFrame();
			if (bValueChanged)
			{
				InOutValue = SpinBoxWidget->GetValue();
			}
			else if (SpinBoxWidget->GetValue() != InOutValue)
			{
				TSharedPtr<FSpinBoxState> SpinBoxState = SpinBoxWidget->template GetMetaData<FSpinBoxState>();
				if (ensure(SpinBoxState) && !SpinBoxState->bIsChanging)
				{
					SpinBoxWidget->SetValue(InOutValue);
				}
			}

			if (SpinBoxStyle && Scope.IsDataHashDirty())
			{
				SpinBoxWidget->SetWidgetStyle(SpinBoxStyle);
				SpinBoxWidget->InvalidateStyle();
			}
		}

		return bValueChanged;
	}


	bool SpinBox(float& InOutValue, TOptional<float> Min, TOptional<float> Max, const FSpinBoxStyle* SpinBoxStyle)
	{
		return SpinBox_Internal<float>(InOutValue, Min, Max, SpinBoxStyle);
	}

	bool SpinBox(double& InOutValue, TOptional<double> Min, TOptional<double> Max, const FSpinBoxStyle* SpinBoxStyle)
	{
		return SpinBox_Internal<double>(InOutValue, Min, Max, SpinBoxStyle);
	}

	bool SpinBox(int32& InOutValue, TOptional<int32> Min, TOptional<int32> Max, const FSpinBoxStyle* SpinBoxStyle)
	{
		return SpinBox_Internal<int32>(InOutValue, Min, Max, SpinBoxStyle);
	}

	bool Slider(float& InOutValue, float Min, float Max, float Step, const FSliderStyle* SliderStyle /*= nullptr*/)
	{
		FWidgetScope<SSlider> Scope(Defaults::Padding, Defaults::HAlign, Defaults::VAlign, Defaults::bAutoSize, Defaults::InputWidgetWidth);
		TSharedPtr<SSlider> SliderWidget = Scope.GetWidget();

		Scope.HashData(SliderStyle);

		bool bValueChanged = false;
		if (!SliderWidget)
		{
			SliderWidget =
				SNew(SSlider)
				.MinValue(Min)
				.MaxValue(Max)
				.StepSize(Step)
				.Value(InOutValue)
				.OnValueChanged_Lambda([ActivationData = Scope.GetOrCreateActivationMetadata().ToWeakPtr()](float NewVal)
				{
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				});

			if (SliderStyle)
			{
				SliderWidget->SetStyle(SliderStyle);
			}

			Scope.UpdateWidget(SliderWidget);
		}
		else
		{
			SliderWidget->SetMinAndMaxValues(Min, Max);
			SliderWidget->SetStepSize(Step);

			bValueChanged = Scope.IsActivatedThisFrame();
			if (bValueChanged)
			{
				InOutValue = SliderWidget->GetValue();
			}
			else
			{
				SliderWidget->SetValue(InOutValue);
			}
			
			if (Scope.IsDataHashDirty() && SliderStyle)
			{
				SliderWidget->SetStyle(SliderStyle);
			}
		}

		return bValueChanged;
	}

	void ProgressBar(TOptional<float> Percent, const FProgressBarStyle* ProgressBarStyle /*= nullptr*/)
	{
		FWidgetScope<SProgressBar> Scope(Defaults::Padding, Defaults::HAlign, Defaults::VAlign, Defaults::bAutoSize, Defaults::InputWidgetWidth);
		TSharedPtr<SProgressBar> ProgressBarWidget = Scope.GetWidget();

		Scope.HashData(ProgressBarStyle);

		if (!ProgressBarWidget)
		{
			ProgressBarWidget =
				SNew(SProgressBar)
				.Percent(Percent);

			if (ProgressBarStyle)
			{
				ProgressBarWidget->SetStyle(ProgressBarStyle);
			}

			Scope.UpdateWidget(ProgressBarWidget);
		}
		else
		{
			ProgressBarWidget->SetPercent(Percent);
			
			if (Scope.IsDataHashDirty() && ProgressBarStyle)
			{
				ProgressBarWidget->SetStyle(ProgressBarStyle);
			}
		}
	}

	bool ComboBox(const TArray<FString>& ComboItems, int32& InOutSelectedItemIndex, bool bForceRefresh, const FComboBoxStyle* InComboStyle)
	{
		FWidgetScope<STextComboBox> Scope;
		TSharedPtr<STextComboBox> ComboWidget = Scope.GetWidget();

		Scope.HashData(InComboStyle);
		
		using FComboBoxData = FSlateIMDataStore<TArray<TSharedPtr<FString>>>;
		
		auto BuildComboOptions = [&]()
		{
			TArray<TSharedPtr<FString>> Options;
			Options.Reserve(ComboItems.Num());

			for (const FString& Item : ComboItems)
			{
				Options.Add(MakeShared<FString>(Item));
			}
			return Options;
		};

		bool bValueChanged = false;
		if (!ComboWidget)
		{
			TSharedRef<FComboBoxData> WidgetIMData = MakeShared<FComboBoxData>(BuildComboOptions());

			ComboWidget =
				SNew(STextComboBox)
				.OptionsSource(&WidgetIMData->Data)
				.OnSelectionChanged_Lambda([ActivationData = Scope.GetOrCreateActivationMetadata().ToWeakPtr()](TSharedPtr<FString> NewVal, ESelectInfo::Type SelectType)
				{
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				});

			if (InComboStyle)
			{
				ComboWidget->SetStyle(InComboStyle);
			}
			
			ComboWidget->AddMetadata(WidgetIMData);

			Scope.UpdateWidget(ComboWidget);
		}
		else
		{
			TSharedPtr<FComboBoxData> ComboBoxData = ComboWidget->GetMetaData<FComboBoxData>();
			if (bForceRefresh)
			{
				ComboBoxData->Data = BuildComboOptions();
				if (InOutSelectedItemIndex != INDEX_NONE && !ComboWidget->IsOpen() && ComboBoxData->Data.IsValidIndex(InOutSelectedItemIndex))
				{
					ComboWidget->SetSelectedItem(ComboBoxData->Data[InOutSelectedItemIndex]);
				}
			}
			else
			{
				bValueChanged = Scope.IsActivatedThisFrame();
				if (bValueChanged)
				{
					TSharedPtr<FString> SelectedItem = ComboWidget->GetSelectedItem();
					const int32 NewSelectedIndex = SelectedItem.IsValid() ? ComboBoxData->Data.IndexOfByKey(SelectedItem) : INDEX_NONE;
					UE_LOG(LogSlateIM, Verbose, TEXT("Combo Selection Changed %d -> %d"), InOutSelectedItemIndex, NewSelectedIndex);
					InOutSelectedItemIndex = NewSelectedIndex;
				}
			}

			if (Scope.IsDataHashDirty())
			{
				ComboWidget->SetStyle(InComboStyle);
			}
		}

		return bValueChanged;
	}

	bool SelectionList(const TArray<FString>& ListItems, int32& InOutSelectedItemIndex, bool bForceRefresh, const FTableViewStyle* InStyle)
	{
		SCOPED_NAMED_EVENT_TEXT("SlateIM::SelectionList", FColorList::Goldenrod);
		using ListViewType = SListView<TSharedPtr<FString>>;
		using FListViewData = FSlateIMDataStore<TArray<TSharedPtr<FString>>>;

		FWidgetScope<ListViewType> Scope;
		TSharedPtr<ListViewType> ListWidget = Scope.GetWidget();

		Scope.HashData(InStyle);

		auto BuildListItems = [&]()
		{
			TArray<TSharedPtr<FString>> ListItemsSource;
			ListItemsSource.Reserve(ListItems.Num());
			for(const FString& Item : ListItems)
			{
				ListItemsSource.Add(MakeShared<FString>(Item));
			}
			return ListItemsSource;
		};

		bool bSelectionChanged = false;
		if (!ListWidget)
		{
			TSharedRef<FListViewData> ListViewData = MakeShared<FListViewData>(BuildListItems());
			ListWidget = SNew(ListViewType)
				.SelectionMode(ESelectionMode::Single)
				.ListViewStyle(InStyle)
				.ListItemsSource(&(ListViewData->Data))
				.OnSelectionChanged_Lambda([ActivationData = Scope.GetOrCreateActivationMetadata().ToWeakPtr()](TSharedPtr<FString> NewVal, ESelectInfo::Type SelectType)
				{
					UE_LOG(LogSlateIM, Verbose, TEXT("Selected %s"), NewVal.IsValid() ? **NewVal.Get() : TEXT("[NULL]"));
					FSlateIMManager::Get().ActivateWidget(ActivationData.Pin());
				})
				.OnGenerateRow_Lambda([](TSharedPtr<FString> ListItem, const TSharedRef<STableViewBase>& OwnerTable)
				{
					return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
						.Padding(4.f)
						[
							SNew(STextBlock).Text(FText::FromString(*ListItem))
						];
				});

			ListWidget->AddMetadata<FListViewData>(ListViewData);
			
			if (ListViewData->Data.IsValidIndex(InOutSelectedItemIndex))
			{
				ListWidget->SetItemSelection(ListViewData->Data[InOutSelectedItemIndex], true);
			}
			Scope.UpdateWidget(ListWidget);
		}
		else
		{
			TSharedPtr<FListViewData> ListViewData = ListWidget->GetMetaData<FListViewData>();
			if (bForceRefresh)
			{
				ListViewData->Data = BuildListItems();
				ListWidget->RequestListRefresh();
			
				if (ListViewData->Data.IsValidIndex(InOutSelectedItemIndex))
				{
					ListWidget->SetItemSelection(ListViewData->Data[InOutSelectedItemIndex], true);
				}
				else
				{
					ListWidget->ClearSelection();
				}
			}
			else
			{
				bSelectionChanged = Scope.IsActivatedThisFrame();
				if (bSelectionChanged)
				{
					TArray<TSharedPtr<FString>> SelectedItems = ListWidget->GetSelectedItems();
					const int32 NewSelectedIndex = SelectedItems.Num() > 0 ? ListViewData->Data.IndexOfByKey(SelectedItems[0]) : INDEX_NONE;
					UE_LOG(LogSlateIM, Verbose, TEXT("List Selection Changed %d -> %d"), InOutSelectedItemIndex, NewSelectedIndex);
					InOutSelectedItemIndex = NewSelectedIndex;
				}
			}

			if (Scope.IsDataHashDirty())
			{
				ListWidget->SetStyle(InStyle);
			}
		}

		return bSelectionChanged;
	}

	void Spacer(const FVector2D& Size)
	{
		FWidgetScope<SSpacer> Scope(Defaults::Padding, HAlign_Left, VAlign_Top);
		TSharedPtr<SSpacer> SpacerWidget = Scope.GetWidget();

		if (!SpacerWidget)
		{
			SpacerWidget = SNew(SSpacer)
				.Size(Size);
			Scope.UpdateWidget(SpacerWidget);
		}
		else
		{
			SpacerWidget->SetSize(Size);
		}
	}

	void Widget(TSharedRef<SWidget> InWidget)
	{
		FWidgetScope<SWidget> Scope(InWidget, Defaults::Padding, HAlign_Left, VAlign_Top);
		TSharedPtr<SWidget> Widget = Scope.GetWidget();

		if (!Widget)
		{
			Scope.UpdateWidget(InWidget);
		}
	}
}
