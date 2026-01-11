// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphNode.h"

#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGSettingsWithDynamicInputs.h"
#include "Metadata/PCGDefaultValueInterface.h"

#include "PCGEditorGraphNodeBase.h"
#include "PCGEditorStyle.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Pins/SPCGEditorGraphPinBool.h"
#include "Pins/SPCGEditorGraphPinNumSlider.h"
#include "Pins/SPCGEditorGraphPinString.h"
#include "Pins/SPCGEditorGraphPinVectorSlider.h"

#include "GraphEditorSettings.h"
#include "IDocumentation.h"
#include "PCGEditorGraphNode.h"
#include "SCommentBubble.h"
#include "SGraphPin.h"
#include "SPinTypeSelector.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphNode"

void SPCGEditorGraphNode::Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode)
{
	GraphNode = InNode;
	PCGEditorGraphNode = InNode;

	if (InNode)
	{
		InNode->OnNodeChangedDelegate.BindSP(this, &SPCGEditorGraphNode::OnNodeChanged);
	}

	UpdateGraphNode();
}

void SPCGEditorGraphNode::CreateAddPinButtonWidget()
{
	// Add Pin Button (+) Source Reference: Engine\Source\Editor\GraphEditor\Private\KismetNodes\SGraphNodeK2Sequence.cpp
	const TSharedPtr<SWidget> AddPinButton = AddPinButtonContent(LOCTEXT("AddSourcePin", "Add Pin"), LOCTEXT("AddSourcePinTooltip", "Add a dynamic source input pin"));

	FMargin AddPinPadding = Settings->GetInputPinPadding();
	AddPinPadding.Top += 6.0f;

	check(PCGEditorGraphNode->GetPCGNode());
	const UPCGSettingsWithDynamicInputs* NodeSettings = CastChecked<UPCGSettingsWithDynamicInputs>(PCGEditorGraphNode->GetPCGNode()->GetSettings());

	const int32 Index = NodeSettings->GetStaticInputPinNum() + NodeSettings->GetDynamicInputPinNum();
	LeftNodeBox->InsertSlot(Index)
		.AutoHeight()
		.VAlign(VAlign_Bottom)
		.Padding(AddPinPadding)
		[
			AddPinButton.ToSharedRef()
		];
}

void SPCGEditorGraphNode::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();

	if (PCGEditorGraphNode->CanUserAddRemoveDynamicInputPins())
	{
		CreateAddPinButtonWidget();
	}
}

const FSlateBrush* SPCGEditorGraphNode::GetNodeBodyBrush() const
{
	const bool bNeedsTint = PCGEditorGraphNode && PCGEditorGraphNode->GetPCGNode() && PCGEditorGraphNode->GetPCGNode()->IsInstance();
	if (bNeedsTint)
	{
		return FAppStyle::GetBrush("Graph.Node.TintedBody");
	}
	else
	{
		return FAppStyle::GetBrush("Graph.Node.Body");
	}
}

TSharedRef<SWidget> SPCGEditorGraphNode::CreateTitleWidget(TSharedPtr<SNodeTitle> InNodeTitle)
{
	// Reimplementation of the SGraphNode::CreateTitleWidget so we can control the style
	const bool bIsInstanceNode = (PCGEditorGraphNode && PCGEditorGraphNode->GetPCGNode() && PCGEditorGraphNode->GetPCGNode()->IsInstance());

	SAssignNew(InlineEditableText, SInlineEditableTextBlock)
	.Style(FPCGEditorStyle::Get(), bIsInstanceNode ? "PCG.Node.InstancedNodeTitleInlineEditableText" : "PCG.Node.NodeTitleInlineEditableText")
	.Text(InNodeTitle.Get(), &SNodeTitle::GetHeadTitle)
	.OnVerifyTextChanged(this, &SPCGEditorGraphNode::OnVerifyNameTextChanged)
	.OnTextCommitted(this, &SPCGEditorGraphNode::OnNameTextCommited)
	.IsReadOnly(this, &SPCGEditorGraphNode::IsNameReadOnly)
	.IsSelected(this, &SPCGEditorGraphNode::IsSelectedExclusively)
	.MultiLine(false)
	.MaximumLength(UPCGEditorGraphNode::MaxNodeNameCharacterCount)
	.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
	.DelayedLeftClickEntersEditMode(false);

	InlineEditableText->SetColorAndOpacity(TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateSP(this, &SPCGEditorGraphNode::GetNodeTitleTextColor)));

	return InlineEditableText.ToSharedRef();
}

TSharedPtr<SGraphPin> SPCGEditorGraphNode::CreatePinWidget(UEdGraphPin* InPin) const
{
	const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
	UPCGSettings* NodeSettings = PCGNode ? PCGNode->GetSettings() : nullptr;

	if (InPin && NodeSettings && NodeSettings->Implements<UPCGSettingsDefaultValueProvider>())
	{
		const IPCGSettingsDefaultValueProvider* DefaultValueInterface = CastChecked<IPCGSettingsDefaultValueProvider>(NodeSettings);
		if (DefaultValueInterface->DefaultValuesAreEnabled() && DefaultValueInterface->IsPinDefaultValueActivated(InPin->PinName))
		{
			// Set the string default value to match the settings' source of truth.
			InPin->DefaultValue = DefaultValueInterface->GetPinDefaultValueAsString(InPin->PinName);

			// To link the transaction to the settings for Undo/Redo.
			TDelegate<void()> OnModify = FSimpleDelegate::CreateLambda([NodeSettingsPtr = TWeakObjectPtr(NodeSettings)]
			{
				if (NodeSettingsPtr.IsValid())
				{
					NodeSettingsPtr.Pin()->Modify();
				}
			});

			switch (DefaultValueInterface->GetPinDefaultValueType(InPin->PinName))
			{
				case EPCGMetadataTypes::Name: // fall-through
				case EPCGMetadataTypes::String:
					return SNew(SPCGEditorGraphPinString, InPin, std::move(OnModify));
				// Float is converted to double by the property accessor under the hood
				case EPCGMetadataTypes::Float: // fall-through
				case EPCGMetadataTypes::Double:
					return SNew(SPCGEditorGraphPinNumSlider<double>, InPin, std::move(OnModify));
				case EPCGMetadataTypes::Integer32:
					return SNew(SPCGEditorGraphPinNumSlider<int32>, InPin, std::move(OnModify))
						.MinDesiredBoxWidth(40.f);
				case EPCGMetadataTypes::Integer64:
					return SNew(SPCGEditorGraphPinNumSlider<int64>, InPin, std::move(OnModify))
						.MinDesiredBoxWidth(40.f);
				case EPCGMetadataTypes::Vector:
					return SNew(SPCGEditorGraphPinVectorSlider<FVector>, InPin, std::move(OnModify));
				case EPCGMetadataTypes::Vector2:
					return SNew(SPCGEditorGraphPinVectorSlider<FVector2D>, InPin, std::move(OnModify));
				case EPCGMetadataTypes::Vector4:
					return SNew(SPCGEditorGraphPinVectorSlider<FVector4>, InPin, std::move(OnModify));
				case EPCGMetadataTypes::Rotator:
					return SNew(SPCGEditorGraphPinVectorSlider<FRotator>, InPin, std::move(OnModify));
				case EPCGMetadataTypes::Boolean:
					return SNew(SPCGEditorGraphPinBool, InPin, std::move(OnModify));
				// @todo_pcg: Will be added once widgets are created.
				case EPCGMetadataTypes::SoftObjectPath: // fall-through
				case EPCGMetadataTypes::SoftClassPath:  // fall-through
				case EPCGMetadataTypes::Quaternion:     // fall-through
				case EPCGMetadataTypes::Transform:      // fall-through
				default:
					break;
			}
		}
	}

	return SNew(SPCGEditorGraphNodePin, InPin);
}

EVisibility SPCGEditorGraphNode::IsAddPinButtonVisible() const
{
	if (PCGEditorGraphNode && PCGEditorGraphNode->IsNodeEnabled() && PCGEditorGraphNode->CanUserAddRemoveDynamicInputPins() && SGraphNode::IsAddPinButtonVisible() == EVisibility::Visible)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Hidden;
}

FReply SPCGEditorGraphNode::OnAddPin()
{
	check(PCGEditorGraphNode);

	PCGEditorGraphNode->OnUserAddDynamicInputPin();
	
	return FReply::Handled();
}

void SPCGEditorGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	check(PCGEditorGraphNode);
	UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();

	if (PCGNode && PinToAdd->GetPinObj())
	{
		const bool bIsInPin = PinToAdd->GetPinObj()->Direction == EEdGraphPinDirection::EGPD_Input;
		const FName& PinName = PinToAdd->GetPinObj()->PinName;

		if(UPCGPin* Pin = (bIsInPin ? PCGNode->GetInputPin(PinName) : PCGNode->GetOutputPin(PinName)))
		{
			const bool bIsMultiData = Pin->Properties.bAllowMultipleData;
			const bool bIsMultiConnections = Pin->AllowsMultipleConnections();

			// Check for special types
			if (Pin->GetCurrentTypes() == EPCGDataType::Param)
			{
				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Param_IN_C : PCGEditorStyleConstants::Pin_Param_OUT_C);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Param_IN_DC : PCGEditorStyleConstants::Pin_Param_OUT_DC);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
			else if (Pin->GetCurrentTypes() == EPCGDataType::Spatial)
			{
				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Composite_IN_C : PCGEditorStyleConstants::Pin_Composite_OUT_C);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Composite_IN_DC : PCGEditorStyleConstants::Pin_Composite_OUT_DC);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
			else if (Pin->Properties.Usage == EPCGPinUsage::DependencyOnly)
			{
				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(PCGEditorStyleConstants::Pin_Graph_Dependency_C);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(PCGEditorStyleConstants::Pin_Graph_Dependency_DC);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
			else
			{
				// Node outputs are always single collection (SC).
				static const FName* PinBrushes[] =
				{
					&PCGEditorStyleConstants::Pin_SD_SC_IN_C,
					&PCGEditorStyleConstants::Pin_SD_SC_IN_DC,
					&PCGEditorStyleConstants::Pin_SD_MC_IN_C,
					&PCGEditorStyleConstants::Pin_SD_MC_IN_DC,
					&PCGEditorStyleConstants::Pin_MD_SC_IN_C,
					&PCGEditorStyleConstants::Pin_MD_SC_IN_DC,
					&PCGEditorStyleConstants::Pin_MD_MC_IN_C,
					&PCGEditorStyleConstants::Pin_MD_MC_IN_DC,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_DC,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_DC,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_DC,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_DC
				};

				const int32 ConnectedIndex = (bIsInPin ? 0 : 8) + (bIsMultiData ? 4 : 0) + (bIsMultiConnections ? 2 : 0);
				const int32 DisconnectedIndex = ConnectedIndex + 1;

				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(*PinBrushes[ConnectedIndex]);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(*PinBrushes[DisconnectedIndex]);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
		}
	}

	SGraphNode::AddPin(PinToAdd);

	// The base class does not give an override to change the padding of the pin widgets, so do it here. Our input pins widgets include
	// a small marker to indicate the pin is required, which need to display at the left edge of the node, so remove left padding.
	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		const int LastIndex = LeftNodeBox->GetChildren()->Num() - 1;
		check(LastIndex >= 0);

		SVerticalBox::FSlot& PinSlot = LeftNodeBox->GetSlot(LastIndex);

		FMargin Margin = Settings->GetInputPinPadding();
		Margin.Left = 0;
		PinSlot.SetPadding(Margin);
	}
}

TArray<FOverlayWidgetInfo> SPCGEditorGraphNode::GetOverlayWidgets(bool bSelected, const FVector2f& WidgetSize) const
{
	TArray<FOverlayWidgetInfo> OverlayWidgets = SGraphNode::GetOverlayWidgets(bSelected, WidgetSize);

	if (UsesHiGenOverlay())
	{
		AddHiGenOverlayWidget(OverlayWidgets);
	}

	if (UsesGPUOverlay())
	{
		AddGPUOverlayWidget(OverlayWidgets);
	}

	return OverlayWidgets;
}

void SPCGEditorGraphNode::GetOverlayBrushes(bool bSelected, const FVector2f& WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const
{
	check(PCGEditorGraphNode);
	
	float YOffsetLeft = 0.0f;
	// Start lower down to be clear of grid size label.
	float YOffsetRight = UsesHiGenOverlay() ? 18.0f : 0.0f;

	auto AddOverlayBrush = [&YOffsetLeft, &YOffsetRight, &Brushes, this](const FName& BrushName, bool bRightSide = false)
	{
		const FSlateBrush* Brush = FPCGEditorStyle::Get().GetBrush(BrushName);

		if (Brush)
		{
			float& YOffset = bRightSide ? YOffsetRight : YOffsetLeft;

			FOverlayBrushInfo BrushInfo;
			BrushInfo.Brush = Brush;
			BrushInfo.OverlayOffset = FVector2f(0.0f, YOffset) - Brush->GetImageSize() / 2.0f;

			if (bRightSide)
			{
				BrushInfo.OverlayOffset.X += GetDesiredSize().X;
			}

			Brushes.Add(BrushInfo);

			YOffset += Brush->GetImageSize().Y;
		}
	};

	if (PCGEditorGraphNode->GetTriggeredGPUUpload())
	{
		AddOverlayBrush(TEXT("PCG.NodeOverlay.GPUUpload"), /*bRightSide=*/true);
	}

	if (PCGEditorGraphNode->GetTriggeredGPUReadback())
	{
		AddOverlayBrush(TEXT("PCG.NodeOverlay.GPUReadback"));
	}

	if (PCGEditorGraphNode->IsCulledFromExecution())
	{
		AddOverlayBrush(PCGEditorStyleConstants::Node_Overlay_Inactive);
	}

	if (const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode())
	{
		if (PCGNode->GetSettingsInterface() && PCGNode->GetSettingsInterface()->bDebug)
		{
			AddOverlayBrush(TEXT("PCG.NodeOverlay.Debug"));
		}
	}

	if (PCGEditorGraphNode->GetInspected())
	{
		AddOverlayBrush(TEXT("PCG.NodeOverlay.Inspect"));
	}
}

void SPCGEditorGraphNode::OnNodeChanged()
{
	// Avoid crashing inside slate if we got triggered from a non-game-thread via any
	//  experimental worker-thread executor
	// @todo_pcg: revisit
	if (IsInGameThread()) 
	{
		UpdateGraphNode(); 
	}
	else
	{
		ExecuteOnGameThread(UE_SOURCE_LOCATION, [this]() { UpdateGraphNode(); });
	}
}

bool SPCGEditorGraphNode::UsesHiGenOverlay() const
{
	return PCGEditorGraphNode->GetInspectedGenerationGrid() != EPCGHiGenGrid::Uninitialized && PCGEditorGraphNode->IsNodeEnabled();
}

bool SPCGEditorGraphNode::UsesGPUOverlay() const
{
	return PCGEditorGraphNode->GetSettings() && PCGEditorGraphNode->GetSettings()->ShouldExecuteOnGPU();
}

FLinearColor SPCGEditorGraphNode::GetGridLabelColor(EPCGHiGenGrid NodeGrid)
{
	// All colours hand tweaked to give a kind of "temperature scale" for the hierarchy.
	switch (NodeGrid)
	{
	case EPCGHiGenGrid::Unbounded:
		return FColor(255, 255, 255, 255);
	case EPCGHiGenGrid::Grid4194304: // fall-through
	case EPCGHiGenGrid::Grid2097152: // fall-through
	case EPCGHiGenGrid::Grid1048576: // fall-through
	case EPCGHiGenGrid::Grid524288: // fall-through
	case EPCGHiGenGrid::Grid262144: // fall-through
	case EPCGHiGenGrid::Grid131072: // fall-through
	case EPCGHiGenGrid::Grid65536: // fall-through
	case EPCGHiGenGrid::Grid32768: // fall-through
	case EPCGHiGenGrid::Grid16384: // fall-through
	case EPCGHiGenGrid::Grid8192: // fall-through
	case EPCGHiGenGrid::Grid4096: // fall-through
	case EPCGHiGenGrid::Grid2048:
		return FColor(53, 60, 171, 255);
	case EPCGHiGenGrid::Grid1024:
		return FColor(31, 82, 210, 255);
	case EPCGHiGenGrid::Grid512:
		return FColor(16, 120, 217, 255);
	case EPCGHiGenGrid::Grid256:
		return FColor(8, 151, 208, 255);
	case EPCGHiGenGrid::Grid128:
		return FColor(9, 170, 188, 255);
	case EPCGHiGenGrid::Grid64:
		return FColor(64, 185, 150, 255);
	case EPCGHiGenGrid::Grid32:
		return FColor(144, 189, 114, 255);
	case EPCGHiGenGrid::Grid16:
		return FColor(207, 185, 89, 255);
	case EPCGHiGenGrid::Grid8:
		return FColor(252, 189, 61, 255);
	case EPCGHiGenGrid::Grid4:
		return FColor(243, 227, 28, 255);
	default:
		ensure(false);
		return FLinearColor::White;
	}
}

// @todo_pcg: Should return a FOverlayWidgetInfo, rather than updating a passed in argument array
void SPCGEditorGraphNode::AddHiGenOverlayWidget(TArray<FOverlayWidgetInfo>& OverlayWidgets) const
{
	check(PCGEditorGraphNode);
	check(UsesHiGenOverlay());

	// Higen grid size overlay widget. All magic numbers below hand tweaked to match UI mockup.
	const EPCGHiGenGrid InspectedGrid = PCGEditorGraphNode->GetInspectedGenerationGrid();

	FText GenerationGridText;
	const EPCGHiGenGrid Grid = PCGEditorGraphNode->GetGenerationGrid();

	if (Grid == EPCGHiGenGrid::Unbounded)
	{
		GenerationGridText = FText::FromString(TEXT("UB"));
	}
	else
	{
		// Meters are easier on the eyes.
		const uint32 GridSize = PCGHiGenGrid::GridToGridSize(Grid) / 100;
		GenerationGridText = FText::AsNumber(GridSize, &FNumberFormattingOptions::DefaultNoGrouping());
	}

	FLinearColor Tint = FLinearColor::White;
	if (Grid != EPCGHiGenGrid::Uninitialized)
	{
		Tint = GetGridLabelColor(Grid);
	}
	else if (PCGEditorGraphNode->IsDisplayAsDisabledForced())
	{
		Tint.A *= 0.35f;
	}

	// Create a border brush for each combination of grids, to workaround issue where the tint does not apply
	// to the border element.
	const FSlateBrush* BorderBrush = GetBorderBrush(InspectedGrid, Grid);

	FLinearColor TextColor = FColor::White;
	FLinearColor BackgroundColor = FColor::Black;
	if (InspectedGrid == Grid)
	{
		// Flip colors for active grid to highlight them.
		Swap(TextColor, BackgroundColor);
	}

	TSharedPtr<SWidget> GridSizeLabel =
		SNew(SHorizontalBox)
		.Visibility(EVisibility::Visible)
		+ SHorizontalBox::Slot()
		[
			SNew(SBorder)
			.BorderImage(BorderBrush)
			.Padding(FMargin(12, 3))
			.ColorAndOpacity(Tint)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "Graph.Node.NodeTitle")
				.Text(GenerationGridText)
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(TextColor)
			]
		];

	FOverlayWidgetInfo GridSizeLabelInfo(GridSizeLabel);
	GridSizeLabelInfo.OverlayOffset = FVector2D(GetDesiredSize().X - 30.0f, -9.0f);

	OverlayWidgets.Add(GridSizeLabelInfo);
}

// @todo_pcg: Should return a FOverlayWidgetInfo, rather than updating a passed in argument array
void SPCGEditorGraphNode::AddGPUOverlayWidget(TArray<FOverlayWidgetInfo>& OverlayWidgets) const
{
	check(PCGEditorGraphNode);
	check(UsesGPUOverlay())

	constexpr float BorderRadius = 7.0f;
	constexpr float BorderStroke = 1.0f;
	constexpr FLinearColor BorderColor(0.5f, 0.5f, 0.5f, 0.5f);
	constexpr FLinearColor TextColor(0.5f, 0.5f, 0.5f, 0.8f);
	const FText GPUText = LOCTEXT("GPULabel", "GPU");

	const FSlateBrush* BorderBrush = new FSlateRoundedBoxBrush(FLinearColor::Transparent, BorderRadius, BorderColor, BorderStroke);

	TSharedPtr<SWidget> GPUUsageLabel =
		SNew(SHorizontalBox)
		.Visibility(EVisibility::Visible)
		+ SHorizontalBox::Slot()
		[
			SNew(SBorder)
			.BorderImage(BorderBrush)
			.Padding(FMargin(4, 3))
			[
				SNew(STextBlock)
				.TextStyle(FPCGEditorStyle::Get(), "PCG.Node.AdditionalOverlayWidgetText")
				.Text(std::move(GPUText))
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(TextColor)
			]
		];

	FOverlayWidgetInfo GPUUsageLabelInfo(GPUUsageLabel);
	GPUUsageLabelInfo.OverlayOffset = FVector2D(GetDesiredSize().X - 34.0f, GetDesiredSize().Y + 5.0f);

	OverlayWidgets.Add(GPUUsageLabelInfo);
}

const FSlateBrush* SPCGEditorGraphNode::GetBorderBrush(EPCGHiGenGrid InspectedGrid, EPCGHiGenGrid NodeGrid) const
{
	if (InspectedGrid == NodeGrid)
	{
		return FPCGEditorStyle::Get().GetBrush(PCGEditorStyleConstants::Node_Overlay_GridSizeLabel_Active_Border);
	}

	// Hand tweaked multiplier to fade child node grid size labels.
	const float Opacity = (InspectedGrid < NodeGrid) ? 1.0f : 0.5f;

	return new FSlateRoundedBoxBrush(
		FLinearColor::Black * Opacity,
		PCGEditorStyleConstants::Node_Overlay_GridSizeLabel_BorderRadius,
		GetGridLabelColor(NodeGrid) * Opacity,
		PCGEditorStyleConstants::Node_Overlay_GridSizeLabel_BorderStroke);
}

#undef LOCTEXT_NAMESPACE
