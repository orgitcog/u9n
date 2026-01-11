// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationMarkersRenderer.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "IWaveformTransformation.h"
#include "Layout/Geometry.h"
#include "PropertyHandle.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundWave.h"
#include "Widgets/Input/SButton.h"
#include <Blueprint/WidgetLayoutLibrary.h>

#define LOCTEXT_NAMESPACE "WaveformTransformationMarkerRenderer"

FWaveformTransformationMarkerRenderer::FWaveformTransformationMarkerRenderer()
{
	const UWaveformTransformationsWidgetsSettings* Settings = GetDefault<UWaveformTransformationsWidgetsSettings>();
	check(Settings);
	Settings->OnSettingChanged().AddRaw(this, &FWaveformTransformationMarkerRenderer::OnWidgetSettingsUpdated);
	ApplyWidgetSettings(Settings);
}

FWaveformTransformationMarkerRenderer::~FWaveformTransformationMarkerRenderer()
{
	FSlateApplication::Get().OnFocusChanging().Remove(PopupHandle);
	FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationStateHandle);
}

int32 FWaveformTransformationMarkerRenderer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = DrawMarkerHandles(AllottedGeometry, OutDrawElements, LayerId);

	return LayerId;
}

int32 FWaveformTransformationMarkerRenderer::DrawMarkerHandles(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	TArray<FVector2D> LinePoints;
	LinePoints.SetNumUninitialized(2);

	check(TransformationWaveInfo.NumChannels > 0);
	const float MarkerHeight = AllottedGeometry.Size.Y * InteractionRatioYDelta;
	constexpr float MarkerWidth = InteractionPixelXDelta * 0.8;
	FSlateRoundedBoxBrush RoundedBoxBrush(FLinearColor::Red, 2);
	FSlateFontInfo Font = FAppStyle::GetFontStyle("Regular");
	Font.Size = LabelFontSize;
	
	uint32 ColorIndex = 0;

	if (LoopPoints.Num() > 0)
	{
		check(LoopColors.Num() > 0);
	}
	for (const FSoundWaveCuePoint& LoopRegion : LoopPoints)
	{
		const float LoopRegionPosition = static_cast<float>(LoopRegion.FramePosition) * PixelsPerFrame;
		LinePoints[0] = FVector2D(LoopRegionPosition, 0.f);
		LinePoints[1] = FVector2D(LoopRegionPosition, AllottedGeometry.Size.Y);

		ColorIndex = (ColorIndex + 1) % LoopColors.Num();

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			LoopColors[ColorIndex],
			false
		);


		const float MarkerCenter = LoopRegionPosition;
		const float	HandleStart = MarkerCenter - MarkerWidth / 2;
		const float LoopRegionPixelLength = static_cast<float>(LoopRegion.FrameLength) * PixelsPerFrame;
		FSlateBrush Brush;
		FPaintGeometry HandleGeometry = AllottedGeometry.ToPaintGeometry(FVector2f(MarkerWidth, MarkerHeight), FSlateLayoutTransform(FVector2f(HandleStart, 0)));
		FPaintGeometry LoopBoxGeometry = AllottedGeometry.ToPaintGeometry(FVector2f(LoopRegionPixelLength, AllottedGeometry.GetLocalSize().Y), FSlateLayoutTransform(FVector2f(MarkerCenter, 0)));
		LinePoints[0] = FVector2D(LoopRegionPosition + LoopRegionPixelLength, 0.f);
		LinePoints[1] = FVector2D(LoopRegionPosition + LoopRegionPixelLength, AllottedGeometry.Size.Y);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			LoopColors[ColorIndex],
			false
		);

		FLinearColor LoopBoxColor = LoopColors[ColorIndex];
		
		//Make LoopBox always half as opaque as the Marker
		LoopBoxColor = LoopColors[ColorIndex];

		float colorMultiplier = 0.5;

		// Highlight selection
		if (MarkersArray->SelectedCue != INDEX_NONE && MarkersArray->SelectedCue == LoopRegion.CuePointID)
		{
			colorMultiplier = 0.9;
		}
		
		LoopBoxColor.A *= colorMultiplier;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			LoopBoxGeometry,
			&Brush,
			ESlateDrawEffect::None,
			LoopBoxColor
		);

		// If the loop region has no width or too small to draw handles, use the marker to move it
		if (static_cast<float>(LoopRegion.FrameLength) < LoopHandlePixelWidth * 2.f)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				HandleGeometry,
				&RoundedBoxBrush,
				ESlateDrawEffect::None,
				LoopColors[ColorIndex]
			);
		}

		FVector2D TextOffset(MarkerCenter + MarkerWidth, 0);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(AllottedGeometry.Size, FSlateLayoutTransform(TextOffset)),
			LoopRegion.Label,
			Font,
			ESlateDrawEffect::None,
			LabelTextColor
		);

		// If we have a loop region highlighted by the mouse, draw the handles
		const bool DrawRegionHandles = (HighlightedCue != INDEX_NONE && HighlightedCue == LoopRegion.CuePointID && static_cast<float>(LoopRegion.FrameLength) > LoopHandlePixelWidth);

		if (DrawRegionHandles)
		{
			FPaintGeometry LeftHandleGeometry = AllottedGeometry.ToPaintGeometry(FVector2f(LoopHandlePixelWidth, AllottedGeometry.GetLocalSize().Y), FSlateLayoutTransform(FVector2f(LoopRegionPosition, 0)));
			FPaintGeometry RightHandleGeometry = AllottedGeometry.ToPaintGeometry(FVector2f(LoopHandlePixelWidth, AllottedGeometry.GetLocalSize().Y), FSlateLayoutTransform(FVector2f(LoopRegionPosition + LoopRegionPixelLength - LoopHandlePixelWidth, 0)));
			const bool bIsLeftHandleHighlighted = IsPositionInLoopHandleArea(MousePosition, AllottedGeometry, LoopRegion, true);
			const bool bIsRightHandleHighlighted = IsPositionInLoopHandleArea(MousePosition, AllottedGeometry, LoopRegion, false);

			FString HighlightHex = TEXT("#0078D7");
			FColor HighlightColor = FColor::FromHex(HighlightHex);
			FLinearColor HighlightLinearColor = FLinearColor::FromSRGBColor(HighlightColor);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				LeftHandleGeometry,
				&Brush,
				ESlateDrawEffect::None,
				bIsLeftHandleHighlighted ? HighlightLinearColor : FLinearColor::White
			);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				RightHandleGeometry,
				&Brush,
				ESlateDrawEffect::None,
				bIsRightHandleHighlighted ? HighlightLinearColor : FLinearColor::White
			);
		}
	}

	for (const FSoundWaveCuePoint& Marker : CuePoints)
	{
		const float MarkerPosition = static_cast<float>(Marker.FramePosition) * static_cast<float>(PixelsPerFrame);
		LinePoints[0] = FVector2D(MarkerPosition, 0.f);
		LinePoints[1] = FVector2D(MarkerPosition, AllottedGeometry.Size.Y);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			MarkerColor,
			false
		);

		const float	HandleStart = MarkerPosition - MarkerWidth / 2;
		FPaintGeometry HandleGeometry = AllottedGeometry.ToPaintGeometry(FVector2f(MarkerWidth, MarkerHeight), FSlateLayoutTransform(FVector2f(HandleStart, 0)));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			HandleGeometry,
			&RoundedBoxBrush,
			ESlateDrawEffect::None,
			MarkersArray->SelectedCue == Marker.CuePointID ? SelectedMarkerColor : MarkerColor
		);

		FVector2D TextOffset(MarkerPosition + MarkerWidth, 0);
		FSlateDrawElement::MakeText(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(AllottedGeometry.Size, FSlateLayoutTransform(TextOffset)),
			Marker.Label,
			Font,
			ESlateDrawEffect::None,
			LabelTextColor
		);
	}

	return LayerId;
}

void FWaveformTransformationMarkerRenderer::SetPropertyHandles(const TArray<TSharedRef<IPropertyHandle>>& InPropertyHandles)
{
	FWaveformTransformationRendererBase::SetPropertyHandles(InPropertyHandles);

	MarkersHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationMarkers, Markers));
	StartLoopTimeHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationMarkers, StartLoopTime));
	EndLoopTimeHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationMarkers, EndLoopTime));
	IsPreviewingLoopRegionHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationMarkers, bIsPreviewingLoopRegion));

	MarkersArray = TStrongObjectPtr<UWaveCueArray>(CastChecked<UWaveCueArray>(GetPropertyValue<UObject*>(MarkersHandle)));

	MarkersArray->ModifyMarkerLoop.BindLambda([this](ELoopModificationControls Modification) { ModifyMarkerLoopRegion(Modification); });
	MarkersArray->CycleMarkerLoop.BindLambda([this](ELoopModificationControls Modification) { CycleMarkerLoopRegion(Modification); });

	// Flag that the loop preview needs to refresh next Tick to avoid issues starting transactions during an Undo
	MarkersArray->CueChanged.BindLambda([this]() { bLoopRegionPreviewNeedsRefresh = true; });
}

void FWaveformTransformationMarkerRenderer::ModifyMarkerLoopRegion(ELoopModificationControls Modification)
{
	FSoundWaveCuePoint* LoopRegion = GetSelectedMarker();

	if (LoopRegion == nullptr)
	{
		return;
	}

	check(MarkersArray);
	check(TransformationWaveInfo.NumChannels > 0);
	check(TransformationWaveInfo.TotalNumSamples > 0);
	check(SizeIncrements.Num() > 0);

	int64 FramesToShift = static_cast<int64>((SizeIncrements[SelectedIncrement] / 1000.0f) * TransformationWaveInfo.SampleRate);
	int64 MaxFrames = static_cast<int64>(TransformationWaveInfo.TotalNumSamples) / static_cast<int64>(TransformationWaveInfo.NumChannels);
	check(MaxFrames > 0);
	
	BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("SetNumericPropertyTransaction", "Modifying Marker"), nullptr);

	MarkersArray->Modify();

	switch (Modification)
	{
	case ELoopModificationControls::None:
		break;
	case ELoopModificationControls::LeftHandleIncrement:
	{
		LoopRegion->FramePosition -= FramesToShift;
		LoopRegion->FrameLength += FramesToShift;
	}
		break;
	case ELoopModificationControls::LeftHandleDecrement:
	{
		LoopRegion->FramePosition += FramesToShift;
		LoopRegion->FrameLength -= FramesToShift;
	}
		break;
	case ELoopModificationControls::RightHandleIncrement:
	{
		LoopRegion->FrameLength += FramesToShift;
	}
		break;
	case ELoopModificationControls::RightHandleDecrement:
	{
		LoopRegion->FrameLength -= FramesToShift;
	}
		break;
	case ELoopModificationControls::IncreaseIncrement:
	{
		if (SelectedIncrement < SizeIncrements.Num() - 1)
		{
			++SelectedIncrement;
		}
	}
	break;
	case ELoopModificationControls::DecreaseIncrement:
	{
		if (SelectedIncrement > 0)
		{
			--SelectedIncrement;
		}
	}
	default:
		break;
	}

	// Prevent the frame position from exceeding the wave length
	LoopRegion->FramePosition = FMath::Clamp(LoopRegion->FramePosition, 0, MaxFrames - 1);
	// FrameLength can exceed MaxFrames because the loop will be cut by ProcessAudio
	// but length must be > 0 or it is no longer a loop
	LoopRegion->FrameLength = FMath::Max(LoopRegion->FrameLength, 1);

	PreviewSelectedLoop();

	EndTransaction();
}

void FWaveformTransformationMarkerRenderer::CycleMarkerLoopRegion(ELoopModificationControls Modification)
{
	check(MarkersArray);

	const int32 MarkerArrayLength = MarkersArray->CuesAndLoops.Num();

	FSoundWaveCuePoint* SelectedCuePointPtr = nullptr;

	// If there is no active selection, pick first or last elmeent
	if (MarkersArray->SelectedCue == INDEX_NONE && MarkerArrayLength > 0)
	{
		switch (Modification)
		{
		case ELoopModificationControls::SelectNextLoop:
		{
			MarkersArray->SelectedCue = MarkersArray->CuesAndLoops[0].CuePointID;
			SelectedCuePointPtr = &MarkersArray->CuesAndLoops[0];
		}
			break;
		case ELoopModificationControls::SelectPreviousLoop:
		{
			MarkersArray->SelectedCue = MarkersArray->CuesAndLoops[MarkerArrayLength - 1].CuePointID;
			SelectedCuePointPtr = &MarkersArray->CuesAndLoops[MarkerArrayLength - 1];
		}
			break;
		default:
			break;
		}
		return;
	}

	// Since cues can be removed and added the ids do not match list indexes, so search for matches
	for (int i = 0; i < MarkerArrayLength; i++)
	{
		if (MarkersArray->CuesAndLoops[i].CuePointID == MarkersArray->SelectedCue)
		{
			// Check if we move backwards of forwards
			if (Modification == ELoopModificationControls::SelectNextLoop)
			{
				MarkersArray->SelectedCue = MarkersArray->CuesAndLoops[(i + 1) % MarkerArrayLength].CuePointID;
				SelectedCuePointPtr = &MarkersArray->CuesAndLoops[(i + 1) % MarkerArrayLength];
				break;
			}
			else if (Modification == ELoopModificationControls::SelectPreviousLoop)
			{
				MarkersArray->SelectedCue = MarkersArray->CuesAndLoops[(i - 1 + MarkerArrayLength) % MarkerArrayLength].CuePointID;
				SelectedCuePointPtr = &MarkersArray->CuesAndLoops[(i - 1 + MarkerArrayLength) % MarkerArrayLength];
				break;
			}
		}
	}

	check(TransformationWaveInfo.SampleRate > 0);
	if (SelectedCuePointPtr && SelectedCuePointPtr->IsLoopRegion())
	{
		PreviewSelectedLoop();
	}
	else
	{
		SetIsPreviewingLoopRegion(0.0, -1.0, false);
	}
}

FSoundWaveCuePoint* FWaveformTransformationMarkerRenderer::GetSelectedMarker() const
{
	check(MarkersArray);

	if (MarkersArray->SelectedCue == INDEX_NONE)
	{
		return nullptr;
	}

	for (int i = 0; i < MarkersArray->CuesAndLoops.Num(); i++)
	{
		if (MarkersArray->CuesAndLoops[i].CuePointID == MarkersArray->SelectedCue)
		{
			return &MarkersArray->CuesAndLoops[i];
		}
	}

	return nullptr;
}

void FWaveformTransformationMarkerRenderer::SelectLoopRegionByKeyboard(const FKey& PressedKey)
{
	check(MarkersArray);

	for (int i = 0; i < IndexKeyCodes.Num(); i++)
	{
		// Check we have loop regions in this index
		if (MarkersArray->CuesAndLoops.Num() <= i)
		{
			return;
		}

		if (PressedKey == IndexKeyCodes[i])
		{
			MarkersArray->SelectedCue = i;

			check(TransformationWaveInfo.SampleRate > 0);
			if (GetSelectedMarker() && GetSelectedMarker()->IsLoopRegion())
			{
				PreviewSelectedLoop();
			}
			else
			{
				SetIsPreviewingLoopRegion(0.0, -1.0, false);
			}
		}
	}
}

void FWaveformTransformationMarkerRenderer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{ 
	check(TransformationWaveInfo.NumChannels > 0);
	const float NumFrames = static_cast<float>(TransformationWaveInfo.TotalNumSamples) / static_cast<float>(TransformationWaveInfo.NumChannels);
	check(NumFrames > 0);
	PixelsPerFrame = FMath::Max(AllottedGeometry.GetLocalSize().X / NumFrames, UE_SMALL_NUMBER);
	ensure(PixelsPerFrame > 0);
	MarkerInInteractionRange.SetMinLoopSize((LoopHandlePixelWidth / PixelsPerFrame) * 3);

	FVector2D MouseAbsolutePosition = UWidgetLayoutLibrary::GetMousePositionOnPlatform();
	MousePosition = AllottedGeometry.AbsoluteToLocal(MouseAbsolutePosition);

	check(MarkersArray != nullptr);

	TMap<int32, FSoundWaveCuePoint> CuePointMap;
	CuePointMap.Reserve(MarkersArray->CuesAndLoops.Num());

	bool IsMarkerInInteractionRangeValid = false;
	for (const FSoundWaveCuePoint& Marker : MarkersArray->CuesAndLoops)
	{
		//Fix duplicate loop region markers for SoundWaves that have not been reimported 
		//after import fix
		ensure(Marker.CuePointID != INDEX_NONE);
		if (!CuePointMap.Contains(Marker.CuePointID))
		{
			CuePointMap.Emplace(Marker.CuePointID, Marker);
		}
		else if (Marker.FrameLength > 0)
		{
			CuePointMap[Marker.CuePointID].FrameLength = Marker.FrameLength;
		}

		if (&Marker == MarkerInInteractionRange.GetMarker())
		{
			IsMarkerInInteractionRangeValid = true;
		}

		check(TransformationWaveInfo.NumChannels > 0);
		int64 StartFrameOffset = TransformationWaveInfo.StartFrameOffset / TransformationWaveInfo.NumChannels;
		int64 NumAvailableSamples = static_cast<int64>(TransformationWaveInfo.NumSamplesAvailable) / TransformationWaveInfo.NumChannels;

		// If loop region is no longer within the available frames, revert Loop preview handles 
		if (Marker.CuePointID == MarkersArray->SelectedCue && Marker.IsLoopRegion() &&
			(Marker.FramePosition + Marker.FrameLength < StartFrameOffset || Marker.FramePosition > StartFrameOffset + NumAvailableSamples))
		{
			MarkersHandle->NotifyPreChange();
			MarkersArray->Modify();
			MarkersArray->SelectedCue = INDEX_NONE;
			
			SetIsPreviewingLoopRegion(0.0, -1.0, false);
			EndTransaction();
		}
	}

	//Invalidate Marker pointer if it has been removed from the array
	if (!IsMarkerInInteractionRangeValid)
	{
		MarkerInInteractionRange.SetMarkerInInteractionRange(nullptr);
	}

	CuePoints.Empty();
	LoopPoints.Empty();
	for (TPair<int32, FSoundWaveCuePoint>& Pair : CuePointMap)
	{
		if (Pair.Value.IsLoopRegion())
		{
			LoopPoints.Add(MoveTemp(Pair.Value));
		}
		else
		{
			CuePoints.Add(MoveTemp(Pair.Value));
		}
	}
	CuePointMap.Empty();

	if (bLoopRegionPreviewNeedsRefresh)
	{
		SetActiveLoopRegion(MarkersArray->SelectedCue);

		bLoopRegionPreviewNeedsRefresh = false;
	}
}

FReply FWaveformTransformationMarkerRenderer::OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalCursorPosition = GetLocalCursorPosition(MouseEvent, MyGeometry);

	MarkerInteractionType = EvaluateInteractionTypeFromCursorPosition(LocalCursorPosition, MyGeometry, MouseEvent.GetEffectingButton());

	if (MarkerInteractionType != EMarkerInteractionType::None)
	{
		MarkersHandle->NotifyPreChange();

		return FReply::Handled().CaptureMouse(OwnerWidget.AsShared()).PreventThrottling();
	}

	return FReply::Unhandled();
}

FReply FWaveformTransformationMarkerRenderer::OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MarkersArray->SelectedCue == INDEX_NONE)
	{
		SetIsPreviewingLoopRegion(0.0, -1.0, false);
	}
	else
	{
		PreviewSelectedLoop();
	}

	if (MarkersArray == nullptr)
	{
		return FReply::Unhandled();
	}

	if (MarkerInteractionType != EMarkerInteractionType::None)
	{
		MarkersArray->Modify();
		SetPropertyValueDependingOnInteractionType(MouseEvent, MyGeometry);
		
		MarkersHandle->NotifyPostChange(EPropertyChangeType::ValueSet);

		// If we know it was an interaction that started a transaction, end the transaction
		if (MarkerInteractionType == EMarkerInteractionType::ScrubbingMarkerHandleLeft || MarkerInteractionType == EMarkerInteractionType::ScrubbingMarkerHandleRight
			|| MarkerInteractionType == EMarkerInteractionType::LoopHandle || MarkerInteractionType == EMarkerInteractionType::MarkerHandle)
		{
			EndTransaction();
		}

		MarkerInteractionType = EMarkerInteractionType::None;
		
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply FWaveformTransformationMarkerRenderer::OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && MarkerInteractionType != EMarkerInteractionType::None)
	{
		SetPropertyValueDependingOnInteractionType(MouseEvent, MyGeometry);

		return FReply::Handled().CaptureMouse(OwnerWidget.AsShared());
	}

	if (MarkersArray == nullptr)
	{
		return FReply::Unhandled();
	}

	// Evaluate highlight state for loop handles
	HighlightedCue = INDEX_NONE;
	for (const FSoundWaveCuePoint& CuePoint : MarkersArray->CuesAndLoops)
	{
		if (CuePoint.IsLoopRegion() && IsPositionInLoopArea(MousePosition, MyGeometry, &CuePoint))
		{
			HighlightedCue = CuePoint.CuePointID;
		}
	}

	return FReply::Unhandled();
}

FCursorReply FWaveformTransformationMarkerRenderer::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	// Grab hand if moving a regular marker
	if (MarkerInteractionType == EMarkerInteractionType::MarkerHandle)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	}

	if (MarkersArray == nullptr)
	{
		return FCursorReply::Unhandled();
	}

	// Set resize on loop handle hover, loops are seperated as we want highlight interactions to superscede region highlights
	for (const FSoundWaveCuePoint& CuePoint : MarkersArray->CuesAndLoops)
	{
		const bool bIsLeftHandleHighlighted = IsPositionInLoopHandleArea(MousePosition, MyGeometry, CuePoint, true) || 
			(MarkerInteractionType == EMarkerInteractionType::ScrubbingMarkerHandleLeft && HighlightedCue == CuePoint.CuePointID);
		const bool bIsRightHandleHighlighted = IsPositionInLoopHandleArea(MousePosition, MyGeometry, CuePoint, false) || 
			(MarkerInteractionType == EMarkerInteractionType::ScrubbingMarkerHandleRight && HighlightedCue == CuePoint.CuePointID);

		if (CuePoint.IsLoopRegion() && (bIsLeftHandleHighlighted || bIsRightHandleHighlighted))
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
		}
		else if (!CuePoint.IsLoopRegion() && IsPositionInInteractionRange(CuePoint.FramePosition, MousePosition, MyGeometry))
		{
			return FCursorReply::Cursor(EMouseCursor::GrabHand);
		}
	}

	// Set grab hand on loop region moving
	for (const FSoundWaveCuePoint& CuePoint : MarkersArray->CuesAndLoops)
	{
		if (CuePoint.IsLoopRegion() && IsPositionInLoopArea(MousePosition, MyGeometry, &CuePoint))
		{
			return FCursorReply::Cursor(EMouseCursor::GrabHand);
		}
	}

	return FCursorReply::Unhandled();
}

FReply FWaveformTransformationMarkerRenderer::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	SelectLoopRegionByKeyboard(InKeyEvent.GetKey());
	return FReply::Handled();
}

void FWaveformTransformationMarkerRenderer::OnWidgetSettingsUpdated(const FName& PropertyName, const UWaveformTransformationsWidgetsSettings* Settings)
{
	check(Settings)

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformTransformationsWidgetsSettings, MarkerColor))
	{
		MarkerColor = Settings->MarkerColor;
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformTransformationsWidgetsSettings, LoopColors))
	{
		SetLoopColors(Settings->LoopColors);
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformTransformationsWidgetsSettings, LabelTextColor))
	{
		LabelTextColor = Settings->LabelTextColor;
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformTransformationsWidgetsSettings, LabelFontSize))
	{
		LabelFontSize = Settings->LabelFontSize;
	}
}

void FWaveformTransformationMarkerRenderer::ApplyWidgetSettings(const UWaveformTransformationsWidgetsSettings* Settings)
{
	check(Settings)

	MarkerColor = Settings->MarkerColor;
	SetLoopColors(Settings->LoopColors);
	LabelTextColor = Settings->LabelTextColor;
	LabelFontSize = Settings->LabelFontSize;
}

void FWaveformTransformationMarkerRenderer::SetLoopColors(const TArray<FLinearColor>& InColors)
{
	LoopColors = InColors;
	if (LoopColors.Num() < 1)
	{
		LoopColors = WaveformTransformationWidgetSharedDefaults::DefaultLoopColors;
	}
}

bool FWaveformTransformationMarkerRenderer::IsPositionInInteractionRange(const int64 InFramePosition, const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry) const
{
	TRange InteractionRange(TRange<float>::Inclusive(static_cast<float>(InFramePosition) * PixelsPerFrame - InteractionPixelXDelta, static_cast<float>(InFramePosition) * PixelsPerFrame + InteractionPixelXDelta));
	if (InteractionRange.Contains(InLocalCursorPosition.X) &&
		InLocalCursorPosition.Y < WidgetGeometry.GetLocalSize().Y * InteractionRatioYDelta)
	{
		return true;
	}
	
	return false;
}

bool FWaveformTransformationMarkerRenderer::IsPositionInLoopArea(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry, const FSoundWaveCuePoint* CueMarker, bool IncludeHandleArea) const
{
	check(TransformationWaveInfo.NumChannels > 0);
	check(CueMarker != nullptr);
	ensure(LoopHandlePixelWidth > 0);

	float LoopRegionPosition = static_cast<float>(CueMarker->FramePosition) * static_cast<float>(PixelsPerFrame);
	float LoopRegionPixelLength = static_cast<float>(CueMarker->FrameLength) * static_cast<float>(PixelsPerFrame);

	// If we want the handles to be included for the area check, as having the mouse in the handle area has different behaviour to the total loop area
	if (!IncludeHandleArea)
	{
		LoopRegionPosition += LoopHandlePixelWidth;
		LoopRegionPixelLength -= LoopHandlePixelWidth * 2;
	}

	check(LoopRegionPixelLength >= 0);

	if (InLocalCursorPosition.X >= LoopRegionPosition && InLocalCursorPosition.X <= LoopRegionPosition + LoopRegionPixelLength &&
		InLocalCursorPosition.Y <= WidgetGeometry.GetLocalSize().Y && InLocalCursorPosition.Y >= 0)
	{
		return true;
	}

	return false;
}

bool FWaveformTransformationMarkerRenderer::IsPositionInLoopHandleArea(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry, const FSoundWaveCuePoint& CueMarker, bool IsLeftHandle) const
{
	check(TransformationWaveInfo.NumChannels > 0);
	ensure(LoopHandlePixelWidth > 0);

	float LoopRegionPosition = IsLeftHandle ?
		static_cast<float>(CueMarker.FramePosition) * static_cast<float>(PixelsPerFrame) :
		(static_cast<float>(CueMarker.FramePosition + CueMarker.FrameLength) * static_cast<float>(PixelsPerFrame)) - LoopHandlePixelWidth;

	// Check that only the handle within the loop area is highlighted
	if (static_cast<float>(InLocalCursorPosition.X) > LoopRegionPosition && static_cast<float>(InLocalCursorPosition.X) < LoopRegionPosition + LoopHandlePixelWidth &&
		static_cast<float>(InLocalCursorPosition.Y) < WidgetGeometry.GetLocalSize().Y && InLocalCursorPosition.Y > 0.0)
	{
		return true;
	}

	return false;
}

//Returns true if there is a marker in interaction range
bool FWaveformTransformationMarkerRenderer::SetMarkerInInteractionRange(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry)
{
	check(TransformationWaveInfo.NumChannels > 0);
	check(TransformationWaveInfo.SampleRate > 0);
	ensure(LoopHandlePixelWidth > 0);
	check(MarkersArray != nullptr);

	InteractionRanges.Reset(MarkersArray->CuesAndLoops.Num());
	
	for (FSoundWaveCuePoint& CuePoint : MarkersArray->CuesAndLoops)
	{
		if (IsPositionInInteractionRange(CuePoint.FramePosition, InLocalCursorPosition, WidgetGeometry))
		{
			MarkerInInteractionRange.SetMarkerInInteractionRange(&CuePoint);
			MarkersArray->SelectedCue = CuePoint.CuePointID;
			
			return true;
		}

		if (CuePoint.FrameLength == 0)
		{
			continue;
		}

		// Check highlights of handles independantly first over loop area
		if (IsPositionInLoopHandleArea(InLocalCursorPosition, WidgetGeometry, CuePoint, true) || IsPositionInLoopHandleArea(InLocalCursorPosition, WidgetGeometry, CuePoint, false))
		{
			MarkerInInteractionRange.SetMarkerInInteractionRange(&CuePoint, true);

			return true;
		}
	}

	check(PixelsPerFrame > 0);

	// Evaluate loop area if no handles are selected
	for (FSoundWaveCuePoint& CuePoint : MarkersArray->CuesAndLoops)
	{
		if (CuePoint.IsLoopRegion() && IsPositionInLoopArea(InLocalCursorPosition, WidgetGeometry, &CuePoint))
		{
			const int64 LoopRegionPosition = static_cast<int64>(InLocalCursorPosition.X / PixelsPerFrame - static_cast<double>(CuePoint.FramePosition));

			MarkerInInteractionRange.SetMarkerInInteractionRange(&CuePoint, false, LoopRegionPosition);
			MarkersArray->SelectedCue = CuePoint.CuePointID;

			return true;
		}
	}

	// MarkerPtr is set to null if there are no markers in range to handle cases where the mouse is not over any markers
	MarkerInInteractionRange.SetMarkerInInteractionRange(nullptr);
	
	return false;
}

bool FWaveformTransformationMarkerRenderer::SelectLoopArea(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry)
{
	check(TransformationWaveInfo.NumChannels > 0);
	ensure(LoopHandlePixelWidth > 0);

	for (FSoundWaveCuePoint& LoopRegion : LoopPoints)
	{
		// Check mouse position bounds
		if (LoopRegion.IsLoopRegion() && IsPositionInLoopArea(InLocalCursorPosition, WidgetGeometry, &LoopRegion))
		{
			SetActiveLoopRegion(LoopRegion.CuePointID);

			return true;
		}
	}

	SetActiveLoopRegion(INDEX_NONE);

	return false;
}

void FWaveformTransformationMarkerRenderer::SetActiveLoopRegion(int32 CuePointID)
{
	check(MarkersArray != nullptr);

	MarkersArray->Modify();
	MarkersArray->SelectedCue = CuePointID;

	check(TransformationWaveInfo.SampleRate > 0);
	// Revert Loop preview handles
	if (CuePointID == INDEX_NONE || GetSelectedMarker() == nullptr || !GetSelectedMarker()->IsLoopRegion())
	{
		SetIsPreviewingLoopRegion(0.0, -1.0, false);
	}
	else
	{
		PreviewSelectedLoop();
	}

	EndTransaction();
}

void FWaveformTransformationMarkerRenderer::PreviewSelectedLoop()
{
	check(TransformationWaveInfo.SampleRate > 0);
	check(TransformationWaveInfo.NumChannels > 0);

	const int64 StartFrameOffset = TransformationWaveInfo.StartFrameOffset / TransformationWaveInfo.NumChannels;
	const int64 EndFramePosition = StartFrameOffset + TransformationWaveInfo.NumSamplesAvailable / TransformationWaveInfo.NumChannels;

	for (FSoundWaveCuePoint CuePoint : MarkersArray->CuesAndLoops)
	{
		const int64 LoopEndFramePosition = CuePoint.FramePosition + CuePoint.FrameLength;
		
		// Only preview a loop if part of it is within the available frames (accounting for TrimFades and other transformations)
		if (CuePoint.CuePointID == MarkersArray->SelectedCue && CuePoint.IsLoopRegion() &&
			LoopEndFramePosition >= StartFrameOffset && CuePoint.FramePosition <= EndFramePosition)
		{
			check(CuePoint.FrameLength > 0);

			// Set Loop preview handles
			const float StartLoopPosInSeconds = static_cast<float>(CuePoint.FramePosition) / TransformationWaveInfo.SampleRate;
			const float EndLoopPosInSeconds = static_cast<float>(LoopEndFramePosition) / TransformationWaveInfo.SampleRate;

			SetIsPreviewingLoopRegion(static_cast<double>(StartLoopPosInSeconds), static_cast<double>(EndLoopPosInSeconds), true);
		}
	}
}

void FWaveformTransformationMarkerRenderer::SetIsPreviewingLoopRegion(double InStartTime, double InEndTime, bool bIsPreviewing)
{
	// Stop any current loop previewing so StartLoopTime is never greater than EndLoopTime (Other than when EndLoopTime is invalid)
	IsPreviewingLoopRegionHandle->SetValue(false);
	StartLoopTimeHandle->SetValue(0.0);
	EndLoopTimeHandle->SetValue(-1.0);

	IsPreviewingLoopRegionHandle->SetValue(bIsPreviewing);
	StartLoopTimeHandle->SetValue(InStartTime);
	EndLoopTimeHandle->SetValue(InEndTime);
}

FVector2D FWaveformTransformationMarkerRenderer::GetLocalCursorPosition(const FPointerEvent& MouseEvent, const FGeometry& EventGeometry)
{
	const FVector2D ScreenSpacePosition = MouseEvent.GetScreenSpacePosition();
	
	return  EventGeometry.AbsoluteToLocal(ScreenSpacePosition);
}

FWaveformTransformationMarkerRenderer::EMarkerInteractionType FWaveformTransformationMarkerRenderer::EvaluateInteractionTypeFromCursorPosition(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry, const FKey MouseButton)
{
	if (MouseButton == EKeys::RightMouseButton)
	{
		if (SetMarkerInInteractionRange(InLocalCursorPosition, WidgetGeometry) && MarkerInInteractionRange.GetMarker() != nullptr && MarkerInInteractionRange.GetMarker()->IsLoopRegion() == false)
		{
			return EMarkerInteractionType::RightClickMarker;
		}
	}

	if (MouseButton != EKeys::LeftMouseButton)
	{
		return EMarkerInteractionType::None;
	}

	if (MarkersArray == nullptr)
	{
		return EMarkerInteractionType::None;
	}

	for (FSoundWaveCuePoint CuePoint : MarkersArray->CuesAndLoops)
	{
		if (!CuePoint.IsLoopRegion())
		{
			continue;
		}			
			
		if (IsPositionInLoopHandleArea(InLocalCursorPosition, WidgetGeometry, CuePoint, true))
		{
			SetMarkerInInteractionRange(InLocalCursorPosition, WidgetGeometry);
			BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("ResizingMarker", "ResizingMarker"), nullptr);
			MarkersArray->Modify();
			return EMarkerInteractionType::ScrubbingMarkerHandleLeft;
		}

		if (IsPositionInLoopHandleArea(InLocalCursorPosition, WidgetGeometry, CuePoint, false))
		{
			SetMarkerInInteractionRange(InLocalCursorPosition, WidgetGeometry);
			BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("ResizingMarker", "ResizingMarker"), nullptr);
			MarkersArray->Modify();
			return EMarkerInteractionType::ScrubbingMarkerHandleRight;
		}
	}

	if (SetMarkerInInteractionRange(InLocalCursorPosition, WidgetGeometry))
	{
		BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("MoveMarker", "MoveMarker"), nullptr);
		MarkersArray->Modify();
		return EMarkerInteractionType::MarkerHandle;
	}
	
	if (SelectLoopArea(InLocalCursorPosition, WidgetGeometry))
	{
		SetMarkerInInteractionRange(InLocalCursorPosition, WidgetGeometry);
		BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("MoveLoop", "MoveLoop"), nullptr);
		MarkersArray->Modify();
		return EMarkerInteractionType::LoopHandle;
	}

	return EMarkerInteractionType::None;
}

void FWaveformTransformationMarkerRenderer::SetPropertyValueDependingOnInteractionType(const FPointerEvent& MouseEvent, const FGeometry& WidgetGeometry)
{
	ensure(PixelsPerFrame > 0);
	check(MarkersHandle);

	const FVector2D LocalCursorPosition = GetLocalCursorPosition(MouseEvent, WidgetGeometry);
	const float LocalCursorXClamped = FMath::Clamp(LocalCursorPosition.X, 0.f, WidgetGeometry.GetLocalSize().X);
	MarkersHandle->NotifyPreChange();

	switch (MarkerInteractionType)
	{
	case EMarkerInteractionType::None:
		break;
	case EMarkerInteractionType::MarkerHandle:
		{
			check(PixelsPerFrame > 0);
			MarkerInInteractionRange.SetMarkerPosition(LocalCursorXClamped / PixelsPerFrame, false);
		}
		break;
	case EMarkerInteractionType::ScrubbingMarkerHandleRight:
		{
			if (MarkerInInteractionRange.GetMarker())
			{
				check(PixelsPerFrame > 0);
				check(TransformationWaveInfo.NumChannels > 0);
				MarkerInInteractionRange.SetMarkerPosition((LocalCursorXClamped / PixelsPerFrame), false);
				HighlightedCue = MarkerInInteractionRange.GetMarker()->CuePointID;
			}
		}
		break;
	case EMarkerInteractionType::ScrubbingMarkerHandleLeft:
	{
		if (MarkerInInteractionRange.GetMarker())
		{
			check(PixelsPerFrame > 0);
			check(TransformationWaveInfo.NumChannels > 0);
			MarkerInInteractionRange.SetMarkerPosition((LocalCursorXClamped / PixelsPerFrame), true);
			HighlightedCue = MarkerInInteractionRange.GetMarker()->CuePointID;
		}
	}
		break;
	case EMarkerInteractionType::LoopHandle:
	{
		if (MarkerInInteractionRange.GetMarker())
		{
			check(PixelsPerFrame > 0);
			MarkerInInteractionRange.SetMarkerPosition(LocalCursorXClamped / PixelsPerFrame, true);
		}
	}
		break;
	case EMarkerInteractionType::RightClickMarker:
	{
		ShowConvertToLoopAtMarker(WidgetGeometry, MouseEvent, MarkerInInteractionRange.GetMarker());
	}
		break;
	}
}

void FWaveformTransformationMarkerRenderer::ShowConvertToLoopAtMarker(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FSoundWaveCuePoint* CueToModify)
{
	check(CueToModify);
	check(MarkersArray != nullptr);
	const FVector2D LocalCursorPosition = MouseEvent.GetScreenSpacePosition();

	TSharedRef<SVerticalBox> MenuContent = SNew(SVerticalBox);

	if (TSharedPtr<SWindow> LockedFadeModeMenuWindow = CreateLoopMenuWindow.Pin())
	{
		FSlateApplication::Get().RequestDestroyWindow(LockedFadeModeMenuWindow.ToSharedRef());
	}

	MenuContent->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
				.Text(LOCTEXT("WaveformTransformationMarkerRendererConvertLoop","Convert To Loop Region"))
				.OnClicked_Lambda([this, CueToModify]() -> FReply
					{
						BeginTransaction(TEXT("PropertyEditor"), LOCTEXT("ConvertMarkerToLoop", "ConvertMarkerToLoop"), nullptr);

						MarkersArray->Modify();
						MarkersArray->EnableLoopRegion(CueToModify);
						CueToModify->FrameLength = MarkerInInteractionRange.GetMinLoopSize();

						EndTransaction();

						if (TSharedPtr<SWindow> LockedFadeModeMenuWindow = CreateLoopMenuWindow.Pin())
						{
							FSlateApplication::Get().RequestDestroyWindow(LockedFadeModeMenuWindow.ToSharedRef());
						}
						return FReply::Handled();
					})
		];

	TSharedRef<SWindow> MenuWindow = SNew(SWindow)
		.AutoCenter(EAutoCenter::None)
		.ScreenPosition(LocalCursorPosition)
		.SizingRule(ESizingRule::Autosized)
		.SupportsTransparency(EWindowTransparency::None)
		.IsPopupWindow(true)
		.CreateTitleBar(false)
		[
			MenuContent
		];

	CreateLoopMenuWindow = MenuWindow;

	FSlateApplication::Get().AddWindow(MenuWindow);

	FSlateApplication::Get().OnFocusChanging().Remove(PopupHandle);
	FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationStateHandle);

	ApplicationActivationStateHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddLambda([this](bool isActive)
		{
			if (!isActive)
			{
				if (TSharedPtr<SWindow> LockedLoopMenuWindow = CreateLoopMenuWindow.Pin())
				{
					FSlateApplication::Get().RequestDestroyWindow(LockedLoopMenuWindow.ToSharedRef());
					FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationStateHandle);
				}
			}
		});

	// If focus is lost on the popup, destroy it to prevent popups hanging around
	PopupHandle = FSlateApplication::Get().OnFocusChanging().AddLambda([this](const FFocusEvent& FocusEvent, const FWeakWidgetPath& WeakWidgetPath
		, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& WidgetPath, const TSharedPtr<SWidget>& NewWidget)
		{
			if (CreateLoopMenuWindow != nullptr && CreateLoopMenuWindow.IsValid())
			{
				if (TSharedPtr<SWindow> LockedLoopMenuWindow = CreateLoopMenuWindow.Pin())
				{
					if (OldWidget && !OldWidget->IsHovered() && LockedLoopMenuWindow == OldWidget)
					{
						FSlateApplication::Get().RequestDestroyWindow(LockedLoopMenuWindow.ToSharedRef());
						FSlateApplication::Get().OnFocusChanging().Remove(PopupHandle);
					}
				}
			}
		});
}

void FWaveformTransformationMarkerRenderer::FMarkerInInteractionRange::SetMarkerInInteractionRange(FSoundWaveCuePoint* InMarkerPtr, const bool InIsLoopRegionEndMarker, int64 GrabXOffset)
{
	MarkerPtr = InMarkerPtr;
	IsLoopRegionEndMarker = InIsLoopRegionEndMarker;
	GrabOffset = GrabXOffset;
}

void FWaveformTransformationMarkerRenderer::FMarkerInInteractionRange::SetMarkerPosition(const float InPosition, bool bIsLeft)
{
	check(MarkerPtr);

	if (IsLoopRegionEndMarker)
	{
		//FMath::Max prevents the user from collapsing the loop
		if (bIsLeft)
		{
			int64 MoveDifference = MarkerPtr->FramePosition - static_cast<int64>(InPosition);
			int64 NewFrameLength = FMath::Max(MinLoopSize, MarkerPtr->FrameLength + MoveDifference);
			
			MarkerPtr->FramePosition = static_cast<int64>(InPosition);
			MarkerPtr->FrameLength = NewFrameLength;
		}
		else
		{
			MarkerPtr->FrameLength = FMath::Max(static_cast<int64>(InPosition), MarkerPtr->FramePosition + MinLoopSize) - MarkerPtr->FramePosition;
		}
	}
	else
	{
		MarkerPtr->FramePosition = FMath::Max(0, static_cast<int64>(InPosition) - GrabOffset);
	}
}

#undef LOCTEXT_NAMESPACE