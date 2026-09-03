// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolTake.h"
#include "AssetRegistry/AssetData.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISequencer.h"
#include "Items/NavigationToolItemUtils.h"
#include "MovieSceneSequence.h"
#include "MovieSceneToolHelpers.h"
#include "NavigationTool.h"
#include "NavigationToolView.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolTake"

namespace UE::SequenceNavigator
{

DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnTakeEntrySelected, const TSharedPtr<SNavigationToolTake::FTakeItemInfo> /*InTakeInfo*/);

class SNavigationToolTakeEntry : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolTakeEntry) {}
		SLATE_EVENT(FOnTakeEntrySelected, OnEntrySelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<SNavigationToolTake::FTakeItemInfo>& InTakeEntry)
	{
		TakeEntry  = InTakeEntry;
		MenuButtonStyle = &FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Menu.Button");

		OnTakeEntrySelected = InArgs._OnEntrySelected;

		ChildSlot
		[
			SNew(SBox)
			.WidthOverride(120.f)
			[
				SNew(SBorder)
				.BorderImage(this, &SNavigationToolTakeEntry::GetBorderImage)
				.Padding(FMargin(12.f, 1.f, 12.f, 1.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TakeEntry->DisplayName))
					.ColorAndOpacity(FStyleColors::Foreground)
				]

			]
		];
	}

	const FSlateBrush* GetBorderImage() const
	{
		if (IsHovered())
		{
			return &MenuButtonStyle->Hovered;
		}
		return &MenuButtonStyle->Normal;
	}

	virtual FReply OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override
	{
		if (OnTakeEntrySelected.IsBound())
		{
			return OnTakeEntrySelected.Execute(TakeEntry);
		}
		return FReply::Handled();
	}

protected:
	TSharedPtr<SNavigationToolTake::FTakeItemInfo> TakeEntry;
	FOnTakeEntrySelected OnTakeEntrySelected;
	const FButtonStyle* MenuButtonStyle = nullptr;
};

class FTakeDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FTakeDragDropOp, FDragDropOperation)

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNullWidget::NullWidget;
	}

	static TSharedRef<FTakeDragDropOp> New(const TSharedPtr<SNavigationToolTake::FTakeItemInfo>& InTakeInfo)
	{
		const TSharedRef<FTakeDragDropOp> Operation = MakeShared<FTakeDragDropOp>();
		Operation->TakeInfo = InTakeInfo;
		Operation->Construct();
		return Operation;
	}

	TSharedPtr<SNavigationToolTake::FTakeItemInfo> TakeInfo;
};

void SNavigationToolTake::Construct(const FArguments& InArgs
	, const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRowWidget)
{
	WeakItem = InItem;
	WeakView = InView;
	WeakRowWidget = InRowWidget;

	WeakTool = InView->GetOwnerTool();

	CacheTakes();

	if (!ActiveTakeInfo.IsValid())
	{
		return;
	}

	ChildSlot
	[
		SNew(SComboBox<TSharedPtr<SNavigationToolTake::FTakeItemInfo>>)
		.OptionsSource(&CachedTakes)
		.InitiallySelectedItem(ActiveTakeInfo)
		.OnSelectionChanged(this, &SNavigationToolTake::OnSelectionChanged)
		.OnGenerateWidget(this, &SNavigationToolTake::GenerateTakeWidget)
		[
			GenerateTakeWidget(ActiveTakeInfo)
		]
	];
}

TSharedRef<SWidget> SNavigationToolTake::GenerateTakeWidget(const TSharedPtr<FTakeItemInfo> InTakeInfo)
{
	const FText TakeInfoText = FText::Format(LOCTEXT("TakeNumberLabel", "({0}/{1})")
		, FText::FromString(FString::FromInt(InTakeInfo->TakeIndex + 1))
		, FText::FromString(FString::FromInt(CachedTakes.Num())));

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::FromInt(InTakeInfo->TakeNumber)))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(5.f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), TEXT("SmallText"))
			.ColorAndOpacity(FStyleColors::Hover)
			.Text(TakeInfoText)
		];
}

void SNavigationToolTake::RemoveItemColor() const
{
	if (const TSharedPtr<INavigationTool> Tool = WeakTool.Pin())
	{
		Tool->RemoveItemColor(WeakItem.Pin());
	}
}

FSlateColor SNavigationToolTake::GetBorderColor() const
{
	if (IsHovered())
	{
		return FSlateColor::UseForeground();
	}
	return FSlateColor::UseSubduedForeground();
}

FReply SNavigationToolTake::OnTakeEntrySelected(const TSharedPtr<FTakeItemInfo> InTakeInfo)
{
	if (const TSharedPtr<INavigationTool> Tool = WeakTool.Pin())
	{
		SetActiveTake(InTakeInfo->WeakSequence.Get());

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SNavigationToolTake::Press()
{
	bIsPressed = true;
}

void SNavigationToolTake::Release()
{
	bIsPressed = false;
}

FReply SNavigationToolTake::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		Press();

		const TSharedRef<SNavigationToolTake> This = SharedThis(this);
		return FReply::Handled().CaptureMouse(This).DetectDrag(This, EKeys::LeftMouseButton);
	}

	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SNavigationToolTake::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = FReply::Unhandled();

	if (bIsPressed && IsHovered() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Release();
		Reply = FReply::Handled();
	}

	if (Reply.GetMouseCaptor().IsValid() == false && HasMouseCapture())
	{
		Reply.ReleaseMouseCapture();
	}

	return Reply;
}

void SNavigationToolTake::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	Release();

	SCompoundWidget::OnFocusLost(InFocusEvent);
}

FReply SNavigationToolTake::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Handled().BeginDragDrop(FTakeDragDropOp::New(ActiveTakeInfo));
	}

	return SCompoundWidget::OnDragDetected(MyGeometry, MouseEvent);
}

void SNavigationToolTake::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (const TSharedPtr<FTakeDragDropOp> DragOp = DragDropEvent.GetOperationAs<FTakeDragDropOp>())
	{
		OnTakeEntrySelected(DragOp->TakeInfo);
	}

	SCompoundWidget::OnDragEnter(MyGeometry, DragDropEvent);
}

void SNavigationToolTake::OnSelectionChanged(const TSharedPtr<FTakeItemInfo> InTakeInfo, const ESelectInfo::Type InSelectType)
{
	if (InTakeInfo.IsValid() && InTakeInfo->WeakSequence.IsValid())
	{
		SetActiveTake(InTakeInfo->WeakSequence.Get());
	}
}

void SNavigationToolTake::CacheTakes()
{
	CachedTakes.Reset();

	UMovieSceneSubSection* const SubSection = Cast<UMovieSceneSubSection>(GetSequenceItemSubSection(WeakItem.Pin()));
	if (!SubSection)
	{
		return;
	}

	TArray<FAssetData> AssetData;
	uint32 CurrentTakeNumber = INDEX_NONE;
	MovieSceneToolHelpers::GatherTakes(SubSection, AssetData, CurrentTakeNumber);

	AssetData.Sort([SubSection](const FAssetData& InA, const FAssetData& InB)
		{
			uint32 TakeNumberA = INDEX_NONE;
			uint32 TakeNumberB = INDEX_NONE;
			if (MovieSceneToolHelpers::GetTakeNumber(SubSection, InA, TakeNumberA)
				&& MovieSceneToolHelpers::GetTakeNumber(SubSection, InB, TakeNumberB))
			{
				return TakeNumberA < TakeNumberB;
			}
			return true;
		});

	uint32 TakeIndex = 0;
	for (const FAssetData& ThisAssetData : AssetData)
	{
		uint32 TakeNumber = INDEX_NONE;
		if (MovieSceneToolHelpers::GetTakeNumber(SubSection, ThisAssetData, TakeNumber))
		{
			if (UMovieSceneSequence* const Sequence = Cast<UMovieSceneSequence>(ThisAssetData.GetAsset()))
			{
				const TSharedRef<FTakeItemInfo> NewTakeInfo = MakeShared<FTakeItemInfo>();
				NewTakeInfo->TakeIndex = TakeIndex;
				NewTakeInfo->TakeNumber = TakeNumber;
				NewTakeInfo->DisplayName = Sequence->GetDisplayName().ToString();
				NewTakeInfo->WeakSequence = Sequence;
				CachedTakes.Add(NewTakeInfo);

				if (TakeNumber == CurrentTakeNumber)
				{
					ActiveTakeInfo = NewTakeInfo;
				}

				++TakeIndex;
			}
		}
	}
}

void SNavigationToolTake::SetActiveTake(UMovieSceneSequence* const InSequence)
{
	const TSharedPtr<INavigationTool> Tool = WeakTool.Pin();
	if (!Tool.IsValid())
	{
		return;
	}

	const TSharedPtr<ISequencer> Sequencer = Tool->GetSequencer();
	if (!Sequencer.IsValid())
	{
		return;
	}

	bool bChangedTake = false;

	const FScopedTransaction Transaction(LOCTEXT("ChangeTake_Transaction", "Change Take"));

	TArray<UMovieSceneSection*> Sections;
	Sequencer->GetSelectedSections(Sections);

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		if (!Sections[SectionIndex]->IsA<UMovieSceneSubSection>())
		{
			continue;
		}

		UMovieSceneSubSection* Section = Cast<UMovieSceneSubSection>(Sections[SectionIndex]);
		UMovieSceneSubTrack* SubTrack = CastChecked<UMovieSceneSubTrack>(Section->GetOuter());

		TRange<FFrameNumber> NewSectionRange = Section->GetRange();
		FFrameNumber		 NewSectionStartOffset = Section->Parameters.StartFrameOffset;
		int32                NewSectionPrerollFrames = Section->GetPreRollFrames();
		int32                NewRowIndex = Section->GetRowIndex();
		FFrameNumber         NewSectionStartTime = NewSectionRange.GetLowerBound().IsClosed() ? UE::MovieScene::DiscreteInclusiveLower(NewSectionRange) : 0;
		int32                NewSectionRowIndex = Section->GetRowIndex();
		FColor               NewSectionColorTint = Section->GetColorTint();

		const int32 Duration = (NewSectionRange.GetLowerBound().IsClosed() && NewSectionRange.GetUpperBound().IsClosed()) ? UE::MovieScene::DiscreteSize(NewSectionRange) : 1;
		UMovieSceneSubSection* NewSection = SubTrack->AddSequence(InSequence, NewSectionStartTime, Duration);

		if (NewSection != nullptr)
		{
			SubTrack->RemoveSection(*Section);

			NewSection->SetRange(NewSectionRange);
			NewSection->Parameters.StartFrameOffset = NewSectionStartOffset;
			NewSection->Parameters.TimeScale = Section->Parameters.TimeScale.DeepCopy(NewSection);
			NewSection->SetPreRollFrames(NewSectionPrerollFrames);
			NewSection->SetRowIndex(NewSectionRowIndex);
			NewSection->SetColorTint(NewSectionColorTint);

			UMovieSceneCinematicShotSection* ShotSection = Cast<UMovieSceneCinematicShotSection>(Section);
			UMovieSceneCinematicShotSection* NewShotSection = Cast<UMovieSceneCinematicShotSection>(NewSection);

			// If the old shot's name is not the same as the sequence's name, assume the user had customized the shot name, so carry it over
			if (ShotSection && NewShotSection && ShotSection->GetSequence() && ShotSection->GetShotDisplayName() != ShotSection->GetSequence()->GetName())
			{
				NewShotSection->SetShotDisplayName(ShotSection->GetShotDisplayName());
			}

			bChangedTake = true;
		}
	}

	if (bChangedTake)
	{
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
