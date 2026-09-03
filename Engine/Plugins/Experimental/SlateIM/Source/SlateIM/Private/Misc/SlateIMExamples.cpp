// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_SLATEIM_EXAMPLES
#include "SlateIMExamples.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/App.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "SlateIM.h"
#include "SlateIMLogging.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/StyleColors.h"

#if WITH_EDITOR
#include "SLevelViewport.h"
#endif
#if WITH_ENGINE
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#endif

namespace SlateIM::Private
{
	struct FExposedSlateStyle : public FSlateStyleSet
	{
	private:
		FExposedSlateStyle()
			: FSlateStyleSet(TEXT("ExposedSlateStyleSet"))
		{}
		
	public:
		TArray<FName> GetBrushStyleKeys(const FString& SearchString) const
		{
			TArray<FName> Keys;
			BrushResources.GenerateKeyArray(Keys);

			if (SearchString.IsEmpty())
			{
				return Keys;
			}
			
			return Keys.FilterByPredicate([&SearchString](const FName& Key)
			{
				return Key.ToString().ToLower().Contains(SearchString);
			});
		}

		template<typename WidgetStyle>
		TMap<FName, const WidgetStyle*> GetWidgetStyles(const FString& SearchString) const
		{
			TMap<FName, const WidgetStyle*> Styles;

			for (const auto& StylePair : WidgetStyleValues)
			{
				if (StylePair.Value->GetTypeName() == WidgetStyle::TypeName)
				{
					if (SearchString.IsEmpty() || StylePair.Key.ToString().ToLower().Contains(SearchString))
					{
						Styles.Add(StylePair.Key, static_cast<const WidgetStyle*>(&StylePair.Value.Get()));
					}
				}
			}

			return Styles;
		}
	};

	FLinearColor GetKeyStateColor(const FKey& Key)
	{
		if (SlateIM::IsKeyPressed(Key))
		{
			return FLinearColor::Green;
		}
		else if (SlateIM::IsKeyHeld(Key))
		{
			return FLinearColor::Blue;	
		}
		else if (SlateIM::IsKeyReleased(Key))
		{
			return FLinearColor::Red;	
		}

		return FLinearColor(0.1, 0.1, 0.1);
	}
}

void FSlateIMTestWidget::Draw()
{
	double LastTime = CurrentTime;
	CurrentTime = 0;
	FScopedDurationTimer Timer(CurrentTime);

	TimeSinceLastUpdate += FApp::GetDeltaTime();

	SCOPED_NAMED_EVENT_TEXT("FSlateIMTestWidget::Draw", FColorList::Goldenrod);
	SlateIM::BeginBorder(FAppStyle::GetBrush("ToolPanel.GroupBorder"));
	// Basic perf measurement, outside of the scrollbox so that it's "pinned"
	if (TimeSinceLastUpdate > 0.5f)
	{
		TimeText = FString::Printf(TEXT("%.3f ms"), LastTime * 1000);
		TimeSinceLastUpdate = 0;
	}
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::BeginHorizontalStack();
	{
#if WITH_ENGINE
		if (GEngine && SlateIM::Button(TEXT("Open Style Browser")))
		{
			GEngine->Exec(nullptr, TEXT("SlateIM.ToggleSlateStyleBrowser"));
		}
#endif
		
		SlateIM::Fill();
		SlateIM::HAlign(HAlign_Right);
		SlateIM::Padding({ 5.f, 5.f, 5.f, 0.f });
		SlateIM::Text(TimeText);
	}
	SlateIM::EndHorizontalStack();
	
	// Full root scrollbox
	SlateIM::Fill();
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::Padding({ 5.f });
	SlateIM::BeginScrollBox();
	{
		{
			SCOPED_NAMED_EVENT_TEXT("Input Widget Examples", FColorList::Goldenrod);
			// Button Examples
			constexpr FStringView ClickText = TEXT("Click Me!");
			if (SlateIM::Button(ClickText))
			{
				UE_LOG(LogSlateIM, Log, TEXT("Button was clicked"));
			}

			SlateIM::HAlign(HAlign_Fill);
			if (SlateIM::Button(TEXT("Filled Button")))
			{
				UE_LOG(LogSlateIM, Log, TEXT("Button was clicked"));
			}

			{
				SlateIM::BeginVerticalStack();
				// EditableText Example
				{
					SlateIM::BeginHorizontalStack();
					SlateIM::EditableText(ComboItemToAdd, TEXT("Add Combo Item"));
					if (SlateIM::IsFocused(SlateIM::EFocusDepth::IncludingDescendants))
					{
						SlateIM::BeginPopUp();
						SlateIM::Text(TEXT("Enter the value of a new item to add to the combo box"));
						SlateIM::EndPopUp();
					}

					bool bDisableAddButton = ComboItemToAdd.IsEmpty();
					if (bDisableAddButton)
					{
						SlateIM::BeginDisabledState();
					}

					if (SlateIM::Button(TEXT("Add Combo Item")))
					{
						bRefreshComboItems = true;
						ComboBoxItems.Add(MoveTemp(ComboItemToAdd));
						ComboItemToAdd.Reset();
					}

					if (bDisableAddButton)
					{
						SlateIM::EndDisabledState();
					}
					SlateIM::EndHorizontalStack();
				}
				SlateIM::EndVerticalStack();
			}

			// CheckBox examples
			SlateIM::CheckBox(TEXT("Check Box"), CheckState);
			if (CheckState)
			{
				SlateIM::Text(TEXT("Basic Text"));
				SlateIM::Text(TEXT("Text With Color"), FLinearColor::Green);
				SlateIM::Text(TEXT("Text With style color"), FStyleColors::Primary);
			}
		
			if (SlateIM::CheckBox(TEXT("Undetermined Check Box"), CheckStateEnum))
			{
				UE_LOG(LogSlateIM, Log, TEXT("Check Box State Changed"));
			}

			// ComboBox example
			if (SlateIM::ComboBox(ComboBoxItems, SelectedItemIndex, bRefreshComboItems))
			{
				UE_LOG(LogSlateIM, Log, TEXT("Combo Box Item %s chosen"), *ComboBoxItems[SelectedItemIndex]);
			}
		}

		{
			SCOPED_NAMED_EVENT_TEXT("Center Texture and Buttons", FColorList::Goldenrod);
			// Centered alignment example
			SlateIM::HAlign(HAlign_Center);
			SlateIM::BeginHorizontalStack();

#if WITH_ENGINE
			// Texture Example
			const int32 CurrentSeconds = FApp::GetCurrentTime();
			SlateIM::VAlign(VAlign_Center);
			UTexture2D* Texture = ((CurrentSeconds % 2) == 0)
				? GreenIcon.LoadSynchronous()
				: RedIcon.LoadSynchronous();
			SlateIM::Image(Texture);
#endif
		
			SlateIM::BeginVerticalStack();
			SlateIM::Button(TEXT("Button 1"));
			SlateIM::Button(TEXT("Button 2"));
			SlateIM::Button(TEXT("Button 3"));
			SlateIM::EndVerticalStack();

			SlateIM::BeginVerticalStack();
			SlateIM::Button(TEXT("Button 4"));
			SlateIM::Button(TEXT("Button 5"));
			SlateIM::Button(TEXT("Button 6"));
			SlateIM::EndVerticalStack();

			// SelectionList example
			SlateIM::SelectionList(ComboBoxItems, SelectedItem, bRefreshComboItems);

			SlateIM::EndHorizontalStack();

			{
				SCOPED_NAMED_EVENT_TEXT("Style image examples", FColorList::Goldenrod);
				// Style image examples
				SlateIM::BeginHorizontalStack();
				SlateIM::Image("AppIcon");
				SlateIM::Padding(FMargin(20, 10, 0, 0));
				SlateIM::Image("Icons.ErrorWithColor");
				SlateIM::Padding(FMargin(SliderVal, 10, 0, 0));
				SlateIM::Image("Icons.WarningWithColor");
				SlateIM::Padding(FMargin(SliderVal, 10, 0, 0));
				SlateIM::Image("Icons.InfoWithColor");
				SlateIM::Padding(FMargin(SliderVal, 10, 0, 0));
				SlateIM::Image("Icons.SuccessWithColor");
				SlateIM::EndHorizontalStack();
			}
		}

		// Slider, ProgressBar, SpinBox
		SlateIM::HAlign(HAlign_Fill);
		SlateIM::BeginHorizontalStack();
		{
			SCOPED_NAMED_EVENT_TEXT("Slider, ProgressBar, SpinBox", FColorList::Goldenrod);
			SlateIM::BeginVerticalStack();
			{
				// Slider example
				if (SlateIM::Slider(SliderVal, 0, SliderMax, 1))
				{
					UE_LOG(LogSlateIM, Log, TEXT("Slider Value Changed [%f]"), SliderVal);
				}

				// ProgressBar example
				SlateIM::ProgressBar(IntValue / static_cast<float>(IntMax));

				// SpinBox Examples
				{
					SlateIM::Padding(FMargin(0, 10, 0, 5));
					SlateIM::SpinBox(SliderVal, 0.0f, SliderMax);

					SlateIM::Padding(FMargin(0, 10, 0, 5));
					SlateIM::SpinBox(IntValue, 0, IntMax);
				}
			}
			SlateIM::EndVerticalStack();

			SlateIM::Fill();
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::VAlign(VAlign_Fill);
			if (SlateIM::Button(TEXT("Reset Values")))
			{
				IntValue = 50;
				SliderVal = 5.f;
			}
		}
		SlateIM::EndHorizontalStack();

		{
			SCOPED_NAMED_EVENT_TEXT("ToolTip example", FColorList::Goldenrod);
			// ToolTip example
			SlateIM::SetToolTip(TEXT("This Is a Tool Tip"));
			SlateIM::BeginHorizontalStack();
			SlateIM::Text(TEXT("Tool Tip Testing:"));
			SlateIM::Image("AppIcon");
			SlateIM::EndHorizontalStack();
		}
		
		{
			SCOPED_NAMED_EVENT_TEXT("PopUp example", FColorList::Goldenrod);
			// PopUp example
			SlateIM::Padding(0);
			SlateIM::BeginHorizontalStack();
			SlateIM::Text(TEXT("Hover here to Show a floating popup"));
			if (SlateIM::IsHovered())
			{
				SlateIM::BeginPopUp();
				SlateIM::Text(TEXT("Pop Up Test:"));
				SlateIM::Image("AppIcon");
				SlateIM::EndPopUp();
			}
			SlateIM::EndHorizontalStack();
		}
		
		{
			SCOPED_NAMED_EVENT_TEXT("DisabledState example", FColorList::Goldenrod);
			// DisabledState example
			SlateIM::CheckBox(TEXT("Disable Everything Below Me"), bShouldBeDisabled);
			if (bShouldBeDisabled)
			{
				SlateIM::BeginDisabledState();
			}
		}

		// ContextMenu examples
		{
			SCOPED_NAMED_EVENT_TEXT("ContextMenu examples", FColorList::Goldenrod);
			SlateIM::Text(TEXT("Context Menu Test"));
			SlateIM::BeginContextMenuAnchor();
			SlateIM::Text(TEXT("Right Click here to show a menu"));

			// This part is only shown if the menu is open
			SlateIM::AddMenuSection(TEXT("Menu Section 1"));
			if (SlateIM::AddMenuButton(TEXT("Menu Item 1"), TEXT("Menu Item Tool Tip 1")))
			{
				UE_LOG(LogSlateIM, Log, TEXT("Menu Item One menu option clicked"));
			}

			SlateIM::AddMenuButton(TEXT("Menu Item 2"), TEXT("Menu Item Tool Tip 2"));
			SlateIM::AddMenuButton(TEXT("Menu Item 3"), TEXT("Menu Item Tool Tip 3"));
			SlateIM::AddMenuButton(TEXT("Menu Item 4"), TEXT("Menu Item Tool Tip 4"));

			SlateIM::AddMenuCheckButton(TEXT("Menu Item With Check"), MenuCheckState, TEXT("Click to toggle check"));

			if (SlateIM::AddMenuToggleButton(TEXT("Menu Item With Toggle"), MenuToggleState, TEXT("Toggle this box")))
			{
				UE_LOG(LogSlateIM, Log, TEXT("Menu Item With Toggle clicked"));
			}

			SlateIM::AddMenuSeparator();
			SlateIM::BeginSubMenu(TEXT("Sub Menu"));
			SlateIM::AddMenuButton(TEXT("SubMenu Item 1"), TEXT("Menu Item Tool Tip 1"));
			SlateIM::AddMenuButton(TEXT("SubMenu Item 2"), TEXT("Menu Item Tool Tip 2"));
			SlateIM::AddMenuButton(TEXT("SubMenu Item 3"), TEXT("Menu Item Tool Tip 3"));
			SlateIM::AddMenuButton(TEXT("SubMenu Item 4"), TEXT("Menu Item Tool Tip 4"));
			SlateIM::EndSubMenu();
			SlateIM::EndContextMenuAnchor();
		}
	

		// Modal Examples
		{
			SCOPED_NAMED_EVENT_TEXT("Modal Examples", FColorList::Goldenrod);
			SlateIM::Text(TEXT("Open Modal Dialog of Type:"));

			// Wrap example
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::BeginHorizontalWrap();
			if (SlateIM::Button(TEXT("Ok")))
			{
				DialogResult = SlateIM::ModalDialog(EAppMsgType::Ok, TEXT("Ok?"));
			}
			if (SlateIM::Button(TEXT("YesNo")))
			{
				DialogResult = SlateIM::ModalDialog(EAppMsgType::YesNo, TEXT("YesNo?"));
			}
			if (SlateIM::Button(TEXT("OkCancel")))
			{
				DialogResult = SlateIM::ModalDialog(EAppMsgType::OkCancel, TEXT("OkCancel?"));
			}
			if (SlateIM::Button(TEXT("YesNoCancel")))
			{
				DialogResult = SlateIM::ModalDialog(EAppMsgType::YesNoCancel, TEXT("YesNoCancel?"));
			}
			if (SlateIM::Button(TEXT("CancelRetryContinue")))
			{
				DialogResult = SlateIM::ModalDialog(EAppMsgType::CancelRetryContinue, TEXT("CancelRetryContinue?"));
			}
			if (SlateIM::Button(TEXT("YesNoYesAllNoAll")))
			{
				DialogResult = SlateIM::ModalDialog(EAppMsgType::YesNoYesAllNoAll, TEXT("YesNoYesAllNoAll?"));
			}
			if (SlateIM::Button(TEXT("YesNoYesAllNoAllCancel")))
			{
				DialogResult = SlateIM::ModalDialog(EAppMsgType::YesNoYesAllNoAllCancel, TEXT("YesNoYesAllNoAllCancel?"));
			}
			if (SlateIM::Button(TEXT("YesNoYesAll")))
			{
				DialogResult = SlateIM::ModalDialog(EAppMsgType::YesNoYesAll, TEXT("YesNoYesAll?"));
			}
			SlateIM::EndHorizontalWrap();

			if (DialogResult.IsSet())
			{
				SlateIM::Text(TEXT("Dialog Result:"));
				switch (DialogResult.GetValue())
				{
				case EAppReturnType::No:
					SlateIM::Text(TEXT("No"));
					break;
				case EAppReturnType::Yes:
					SlateIM::Text(TEXT("Yes"));
					break;
				case EAppReturnType::YesAll:
					SlateIM::Text(TEXT("Yes to All"));
					break;
				case EAppReturnType::NoAll:
					SlateIM::Text(TEXT("No to All"));
					break;
				case EAppReturnType::Cancel:
					SlateIM::Text(TEXT("Cancel"));
					break;
				case EAppReturnType::Ok:
					SlateIM::Text(TEXT("Ok"));
					break;
				case EAppReturnType::Retry:
					SlateIM::Text(TEXT("Retry"));
					break;
				case EAppReturnType::Continue:
					SlateIM::Text(TEXT("Continue"));
					break;
				default:
					SlateIM::Text(TEXT("UNHANDLED RESULT"));
					break;
				}
			
				if (SlateIM::Button(TEXT("Reset")))
				{
					DialogResult.Reset();
				}
			}
			else
			{
				SlateIM::Text(TEXT("No Dialog Result"));
			}
		}
		
		SlateIM::Text(TEXT("Scroll Bar Test"));
		SlateIM::BeginHorizontalStack();

		SlateIM::Text(TEXT("Num Items "));
		SlateIM::MinWidth(50.f);
		if (SlateIM::EditableText(NumItemsText))
		{
			LiveNumItems = FCString::Atoi(*NumItemsText);
		}

		if (SlateIM::Button(TEXT("Regenerate")))
		{
			NumItems = FCString::Atoi(*NumItemsText);
		}

		int32 NumTableItems = NumItems;
		SlateIM::CheckBox(TEXT("Live Update Table?"), bShouldLiveUpdateTable);
		if (bShouldLiveUpdateTable)
		{
			NumTableItems = LiveNumItems;
		}
		SlateIM::EndHorizontalStack();

		SlateIM::VAlign(VAlign_Fill);
		SlateIM::HAlign(HAlign_Fill);
		SlateIM::BeginHorizontalStack();

		// ScrollBox example
		{
			SCOPED_NAMED_EVENT_TEXT("ScrollBox example", FColorList::Goldenrod);
			SlateIM::AutoSize();
			SlateIM::MaxHeight(200.f);
			SlateIM::BeginScrollBox();
			for (int32 i = 0; i < NumItems; ++i)
			{
				// New row
				SlateIM::BeginHorizontalStack();
				{
					SlateIM::Padding(FMargin(5, 0));
					SlateIM::VAlign(VAlign_Center);// Centers the button in the row
					SlateIM::Text(FString::Printf(TEXT("Item %d/%d"), i + 1, NumItems), FColor::MakeRedToGreenColorFromScalar(static_cast<float>(i) / NumItems));
					SlateIM::Padding(FMargin(0));

					if (SlateIM::Button(TEXT("Click")))
					{
						UE_LOG(LogSlateIM, Log, TEXT("Button %d clicked"), i + 1);
					}
				}
				SlateIM::EndHorizontalStack();
			}
			SlateIM::EndScrollBox();
		}

		// Spacer Example
		SlateIM::Spacer({ 20, 1 });

		// Table Example
		{
			SCOPED_NAMED_EVENT_TEXT("Table Example", FColorList::Goldenrod);
			SlateIM::Fill();
			SlateIM::HAlign(HAlign_Fill);
			SlateIM::MaxHeight(200.f);
			SlateIM::BeginTable();
			SlateIM::AddTableColumn(TEXT("Item"));
			SlateIM::HAlign(HAlign_Center);
			SlateIM::InitialTableColumnWidth(80.f);
			SlateIM::AddTableColumn(TEXT("Button"));
			for (int32 i = 0; i < NumTableItems; ++i)
			{
				if (SlateIM::NextTableCell())
				{
					SlateIM::Padding(FMargin(5, 0));
					SlateIM::Fill();
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(FString::Printf(TEXT("Item %d/%d"), i + 1, NumTableItems), FColor::MakeRedToGreenColorFromScalar(static_cast<float>(i) / NumTableItems));
				}

				if (SlateIM::NextTableCell())
				{
					SlateIM::Padding(FMargin(0));
					SlateIM::HAlign(HAlign_Center);
					if (SlateIM::Button(TEXT("Click")))
					{
						UE_LOG(LogSlateIM, Log, TEXT("Table Button %d clicked"), i + 1);
					}
				}
			}
			SlateIM::EndTable();
		}
		SlateIM::EndHorizontalStack();

		// Graph Examples
		{
			SCOPED_NAMED_EVENT_TEXT("Graph Example", FColorList::Goldenrod);
			SlateIM::BeginHorizontalStack();
			{
				if (SquareGraphValues.Num() >= 100)
				{
					SquareGraphValues.PopFront();
				}
				const double SquareValue = (GFrameCounter / 10) % 2;
				SquareGraphValues.Emplace(SquareValue);
				SlateIM::Fill();
				SlateIM::MinHeight(200.f);
				SlateIM::BeginGraph();
				SlateIM::GraphLine(SquareGraphValues.Compact(), FLinearColor::White, 3.f, FDoubleRange(0, 1.0));
				SlateIM::EndGraph();

				SlateIM::VAlign(VAlign_Center);
				SlateIM::Text(FString::Printf(TEXT("%0.3lf"), SquareValue));

				const double NextSinX = SinGraphValues.Last().X + 1.0;
				const double NextCosX = CosGraphValues.Last().X + 1.0;
				const double NextTanX = TanGraphValues.Last().X + 1.0;
				if (GFrameCounter % 4 == 0)
				{
					if (SinGraphValues.Num() >= 100)
					{
						SinGraphValues.PopFront();
					}
					SinGraphValues.Emplace(NextSinX, FMath::Sin(NextSinX/4.0));
					
					if (CosGraphValues.Num() >= 100)
					{
						CosGraphValues.PopFront();
					}
					CosGraphValues.Emplace(NextCosX, FMath::Cos(NextCosX/4.0));
					
					if (TanGraphValues.Num() >= 100)
					{
						TanGraphValues.PopFront();
					}
					TanGraphValues.Emplace(NextTanX, FMath::Tan(NextTanX/4.0));
				}
				SlateIM::Fill();
				SlateIM::MinHeight(200.f);
				SlateIM::BeginGraph();
				SlateIM::GraphLine(SinGraphValues.Compact(), FColor::Orange, 1.f, FDoubleRange(NextSinX - 100.0, NextSinX), FDoubleRange(-1.5, 1.5));
				SlateIM::GraphLine(CosGraphValues.Compact(), FLinearColor::Green, 1.f, FDoubleRange(NextCosX - 100.0, NextCosX), FDoubleRange(-1.5, 1.5));
				SlateIM::GraphLine(TanGraphValues.Compact(), FColor::Magenta, 1.f, FDoubleRange(NextTanX - 100.0, NextTanX), FDoubleRange(-1.5, 1.5));
				SlateIM::EndGraph();

				SlateIM::MinWidth(50.f);
				SlateIM::MaxWidth(50.f);
				SlateIM::BeginVerticalStack();
				SlateIM::Fill();
				SlateIM::VAlign(VAlign_Center);
				SlateIM::Text(FString::Printf(TEXT("%0.3lf"), SinGraphValues.Last().Y), FColor::Orange);
				SlateIM::Fill();
				SlateIM::VAlign(VAlign_Center);
				SlateIM::Text(FString::Printf(TEXT("%0.3lf"), CosGraphValues.Last().Y), FLinearColor::Green);
				SlateIM::Fill();
				SlateIM::VAlign(VAlign_Center);
				SlateIM::Text(FString::Printf(TEXT("%0.3lf"), TanGraphValues.Last().Y), FColor::Magenta);
				SlateIM::EndVerticalStack();
			}
			SlateIM::EndHorizontalStack();
		}

		// Tree Example
		{
			SCOPED_NAMED_EVENT_TEXT("Tree Example", FColorList::Goldenrod);
			const FTableRowStyle* TableRowStyle = &FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.AlternatingRow");
			SlateIM::MinWidth(500.f);
			SlateIM::MinHeight(200.f);
			SlateIM::MaxHeight(200.f);
			SlateIM::VAlign(VAlign_Fill);
			SlateIM::BeginTable(nullptr, TableRowStyle);
			SlateIM::AddTableColumn(TEXT("Name"));
			SlateIM::AddTableColumn(TEXT("Type"));
			{
				if (SlateIM::NextTableCell())
				{
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(TEXT("Antarctica"));
				}
				if (SlateIM::NextTableCell())
				{
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(TEXT("Continent"));
				}
				
				if (SlateIM::NextTableCell())
				{
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(TEXT("North America"));
				}
				if (SlateIM::NextTableCell())
				{
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(TEXT("Continent"));
				}
				if (SlateIM::BeginTableRowChildren())
				{
					if (SlateIM::NextTableCell())
					{
						SlateIM::VAlign(VAlign_Center);
						SlateIM::Text(TEXT("Canada"));
					}
					if (SlateIM::NextTableCell())
					{
						SlateIM::VAlign(VAlign_Center);
						SlateIM::Text(TEXT("Country"));
					}
					if (SlateIM::BeginTableRowChildren())
					{
						if (SlateIM::NextTableCell())
						{
							SlateIM::VAlign(VAlign_Center);
							SlateIM::Text(TEXT("British Columbia"));
						}
						if (SlateIM::NextTableCell())
						{
							SlateIM::VAlign(VAlign_Center);
							SlateIM::Text(TEXT("Province"));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							if (SlateIM::NextTableCell())
							{
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT("Vancouver"));
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT("City"));
							}
						}
						SlateIM::EndTableRowChildren();
					
						if (SlateIM::NextTableCell())
						{
							SlateIM::VAlign(VAlign_Center);
							SlateIM::Text(TEXT("Quebec"));
						}
						if (SlateIM::NextTableCell())
						{
							SlateIM::VAlign(VAlign_Center);
							SlateIM::Text(TEXT("Province"));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							if (SlateIM::NextTableCell())
							{
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT("Montreal"));
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT("City"));
							}
						}
						SlateIM::EndTableRowChildren();
					}
					SlateIM::EndTableRowChildren();

					if (SlateIM::NextTableCell())
					{
						SlateIM::VAlign(VAlign_Center);
						SlateIM::Text(TEXT("United States"));
					}
					if (SlateIM::NextTableCell())
					{
						SlateIM::VAlign(VAlign_Center);
						SlateIM::Text(TEXT("Country"));
					}
					if (SlateIM::BeginTableRowChildren())
					{
						if (SlateIM::NextTableCell())
						{
							SlateIM::VAlign(VAlign_Center);
							SlateIM::Text(TEXT("North Carolina"));
						}
						if (SlateIM::NextTableCell())
						{
							SlateIM::VAlign(VAlign_Center);
							SlateIM::Text(TEXT("State"));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							if (SlateIM::NextTableCell())
							{
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT("Cary"));
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT("City"));
							}
						}
						SlateIM::EndTableRowChildren();
						
						if (SlateIM::NextTableCell())
						{
							SlateIM::VAlign(VAlign_Center);
							SlateIM::Text(TEXT("Washington"));
						}
						if (SlateIM::NextTableCell())
						{
							SlateIM::VAlign(VAlign_Center);
							SlateIM::Text(TEXT("State"));
						}
						if (SlateIM::BeginTableRowChildren())
						{
							if (SlateIM::NextTableCell())
							{
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT("Bellevue"));
							}
							if (SlateIM::NextTableCell())
							{
								SlateIM::VAlign(VAlign_Center);
								SlateIM::Text(TEXT("City"));
							}
						}
						SlateIM::EndTableRowChildren();
					}
					SlateIM::EndTableRowChildren();
				}
				SlateIM::EndTableRowChildren();
			}
			
			SlateIM::EndTable();
		}

		// Inputs example
		{
			SlateIM::BeginHorizontalStack();
			{
				SlateIM::BeginVerticalStack();
				SlateIM::BeginHorizontalStack();
				SlateIM::Spacer(FVector2D(50, 50));
				{
					WBrush = FSlateColorBrush(SlateIM::Private::GetKeyStateColor(EKeys::W));
					SlateIM::MinWidth(50.f);
					SlateIM::MinHeight(50.f);
					SlateIM::MaxWidth(50.f);
					SlateIM::MaxHeight(50.f);
					SlateIM::BeginBorder(&WBrush);
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(TEXT("W"), FLinearColor::White);
					SlateIM::EndBorder();
				}
				SlateIM::Spacer(FVector2D(50, 50));
				SlateIM::EndHorizontalStack();
				SlateIM::BeginHorizontalStack();
				{
					ABrush = FSlateColorBrush(SlateIM::Private::GetKeyStateColor(EKeys::A));
					SlateIM::MinWidth(50.f);
					SlateIM::MinHeight(50.f);
					SlateIM::MaxWidth(50.f);
					SlateIM::MaxHeight(50.f);
					SlateIM::BeginBorder(&ABrush);
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(TEXT("A"), FLinearColor::White);
					SlateIM::EndBorder();
				}
				{
					SBrush = FSlateColorBrush(SlateIM::Private::GetKeyStateColor(EKeys::S));
					SlateIM::MinWidth(50.f);
					SlateIM::MinHeight(50.f);
					SlateIM::MaxWidth(50.f);
					SlateIM::MaxHeight(50.f);
					SlateIM::BeginBorder(&SBrush);
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(TEXT("S"), FLinearColor::White);
					SlateIM::EndBorder();
				}
				{
					DBrush = FSlateColorBrush(SlateIM::Private::GetKeyStateColor(EKeys::D));
					SlateIM::MinWidth(50.f);
					SlateIM::MinHeight(50.f);
					SlateIM::MaxWidth(50.f);
					SlateIM::MaxHeight(50.f);
					SlateIM::BeginBorder(&DBrush);
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Text(TEXT("D"), FLinearColor::White);
					SlateIM::EndBorder();
				}
				SlateIM::EndHorizontalStack();
				SlateIM::EndVerticalStack();
			}

			{
				SlateIM::BeginVerticalStack();
				SlateIM::Text(TEXT("Right Stick X-value"));
				const float NormalizedAnalogXValue = (1.f + SlateIM::GetKeyAnalogValue(EKeys::Gamepad_RightX)) * 0.5f;
				SlateIM::VAlign(VAlign_Center);
				SlateIM::ProgressBar(NormalizedAnalogXValue);
				
				SlateIM::Fill();
				SlateIM::Spacer(FVector2D(1.f, 1.f));
				
				SlateIM::Text(TEXT("Right Stick Y-value"));
				const float NormalizedAnalogYValue = (1.f + SlateIM::GetKeyAnalogValue(EKeys::Gamepad_RightY)) * 0.5f;
				SlateIM::VAlign(VAlign_Center);
				SlateIM::ProgressBar(NormalizedAnalogYValue);
				SlateIM::EndVerticalStack();
			}
			SlateIM::EndHorizontalStack();
		}

		if (bShouldBeDisabled)
		{
			SlateIM::EndDisabledState();
		}
	}
	SlateIM::EndScrollBox();
	SlateIM::EndBorder();

	bRefreshComboItems = false;
}

void FSlateStyleBrowser::DrawWindow(float DeltaTime)
{
	static const TArray<FString> Options = { TEXT("Option 1"), TEXT("Option 2"), TEXT("Option 3") };
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::Fill();
	SlateIM::BeginVerticalStack();
	SlateIM::EditableText(SearchString, TEXT("Search Styles..."));
	
	SlateIM::HAlign(HAlign_Fill);
	SlateIM::VAlign(VAlign_Fill);
	SlateIM::Fill();
	SlateIM::BeginTable();
	SlateIM::AddTableColumn(TEXT("Name"));
	SlateIM::AddTableColumn(TEXT("Preview"));
	SlateIM::InitialTableColumnWidth(80.f);
	SlateIM::AddTableColumn();

	const auto& Style = static_cast<const SlateIM::Private::FExposedSlateStyle&>(FAppStyle::Get());
	const FString LowerCaseSearchString = SearchString.ToLower();

	auto DrawNameCell = [](const FName& Key)
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::VAlign(VAlign_Center);
			SlateIM::Fill();
			SlateIM::Text(Key.ToString());
		}
	};

	auto DrawCopyCell = [](const FName& Key)
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::SetToolTip(TEXT("Click to copy the style name to your clipboard"));
			SlateIM::HAlign(HAlign_Center);
			SlateIM::VAlign(VAlign_Center);
			SlateIM::Fill();
			if (SlateIM::Button(TEXT("Copy")))
			{
				FPlatformApplicationMisc::ClipboardCopy(*Key.ToString());
			}
		}
	};

	// Brushes
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Brushes"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column

		if (SlateIM::BeginTableRowChildren())
		{
			for (const FName& BrushStyleKey : Style.GetBrushStyleKeys(LowerCaseSearchString))
			{
				if (const FSlateBrush* Brush = Style.GetBrush(BrushStyleKey))
				{
					DrawNameCell(BrushStyleKey);

					if (SlateIM::NextTableCell())
					{
						SlateIM::HAlign(HAlign_Center);
						SlateIM::VAlign(VAlign_Center);
						SlateIM::Fill();
						SlateIM::Image(Brush);
					}
					
					DrawCopyCell(BrushStyleKey);
				}
			}
		}
		SlateIM::EndTableRowChildren();
	}

	// Text Block Styles
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Text Block Styles"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column
		if (SlateIM::BeginTableRowChildren())
		{
			for (const TPair<FName, const FTextBlockStyle*>& WidgetStyle : Style.GetWidgetStyles<FTextBlockStyle>(LowerCaseSearchString))
			{
				DrawNameCell(WidgetStyle.Key);

				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Fill();
					SlateIM::Text(TEXT("The quick brown fox jumps over the lazy dog."), WidgetStyle.Value);
				}
			
				DrawCopyCell(WidgetStyle.Key);
			}
		}
		SlateIM::EndTableRowChildren();
	}

	// Editable Text Box Styles
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Editable Text Box Styles"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column
		if (SlateIM::BeginTableRowChildren())
		{
			for (const TPair<FName, const FEditableTextBoxStyle*>& WidgetStyle : Style.GetWidgetStyles<FEditableTextBoxStyle>(LowerCaseSearchString))
			{
				DrawNameCell(WidgetStyle.Key);

				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Fill();
					SlateIM::EditableText(PreviewText, TEXT("Hint text..."), WidgetStyle.Value);
				}
				
				DrawCopyCell(WidgetStyle.Key);
			}
		}
		SlateIM::EndTableRowChildren();
	}

	// Button Styles
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Button Styles"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column
		if (SlateIM::BeginTableRowChildren())
		{
			for (const TPair<FName, const FButtonStyle*>& WidgetStyle : Style.GetWidgetStyles<FButtonStyle>(LowerCaseSearchString))
			{
				DrawNameCell(WidgetStyle.Key);

				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Fill();
					SlateIM::Button(TEXT("Click Me"), WidgetStyle.Value);
				}
				
				DrawCopyCell(WidgetStyle.Key);
			}
		}
		SlateIM::EndTableRowChildren();
	}

	// SpinBox Styles
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("SpinBox Styles"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column
		if (SlateIM::BeginTableRowChildren())
		{
			for (const TPair<FName, const FSpinBoxStyle*>& WidgetStyle : Style.GetWidgetStyles<FSpinBoxStyle>(LowerCaseSearchString))
			{
				DrawNameCell(WidgetStyle.Key);

				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Fill();
					SlateIM::SpinBox(SpinBoxValue, -100.f, 100.f, WidgetStyle.Value);
				}
				
				DrawCopyCell(WidgetStyle.Key);
			}
		}
		SlateIM::EndTableRowChildren();
	}

	// Slider Styles
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Slider Styles"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column
		if (SlateIM::BeginTableRowChildren())
		{
			for (const TPair<FName, const FSliderStyle*>& WidgetStyle : Style.GetWidgetStyles<FSliderStyle>(LowerCaseSearchString))
			{
				DrawNameCell(WidgetStyle.Key);

				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Fill();
					SlateIM::Slider(SliderValue, 0.f, 100.f, 0.1f, WidgetStyle.Value);
				}
				
				DrawCopyCell(WidgetStyle.Key);
			}
		}
		SlateIM::EndTableRowChildren();
	}

	// ProgressBar Styles
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Progress Bar Styles"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column
		if (SlateIM::BeginTableRowChildren())
		{
			for (const TPair<FName, const FProgressBarStyle*>& WidgetStyle : Style.GetWidgetStyles<FProgressBarStyle>(LowerCaseSearchString))
			{
				DrawNameCell(WidgetStyle.Key);

				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Fill();
					SlateIM::ProgressBar(SliderValue / 100.f, WidgetStyle.Value);
				}
				
				DrawCopyCell(WidgetStyle.Key);
			}
		}
		SlateIM::EndTableRowChildren();
	}

	// ComboBox Styles
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Combo Box Styles"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column
		if (SlateIM::BeginTableRowChildren())
		{
			for (const TPair<FName, const FComboBoxStyle*>& WidgetStyle : Style.GetWidgetStyles<FComboBoxStyle>(LowerCaseSearchString))
			{
				DrawNameCell(WidgetStyle.Key);

				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Fill();
					constexpr bool bForceRefresh = false;
					SlateIM::ComboBox(Options, SelectedComboIndex, bForceRefresh, WidgetStyle.Value);
				}
				
				DrawCopyCell(WidgetStyle.Key);
			}
		}
		SlateIM::EndTableRowChildren();
	}

	// Table View Styles
	{
		if (SlateIM::NextTableCell())
		{
			SlateIM::Text(TEXT("Table View Styles"));
		}
		SlateIM::NextTableCell(); // Skip Preview Column
		SlateIM::NextTableCell(); // Skip Button Column
		if (SlateIM::BeginTableRowChildren())
		{
			for (const TPair<FName, const FTableViewStyle*>& WidgetStyle : Style.GetWidgetStyles<FTableViewStyle>(LowerCaseSearchString))
			{
				DrawNameCell(WidgetStyle.Key);

				if (SlateIM::NextTableCell())
				{
					SlateIM::HAlign(HAlign_Center);
					SlateIM::VAlign(VAlign_Center);
					SlateIM::Fill();
					SlateIM::BeginHorizontalStack();
					{
						SlateIM::HAlign(HAlign_Center);
						SlateIM::VAlign(VAlign_Top);
						SlateIM::Fill();
						SlateIM::SelectionList(Options, SelectedListIndex, false, WidgetStyle.Value);

						SlateIM::HAlign(HAlign_Center);
						SlateIM::VAlign(VAlign_Top);
						SlateIM::Fill();
						SlateIM::BeginTable(WidgetStyle.Value);
						SlateIM::AddTableColumn(TEXT("Column 1"));
						SlateIM::AddTableColumn(TEXT("Column 2"));
						SlateIM::NextTableCell();
						SlateIM::Text(TEXT("Cell 1"));
						SlateIM::NextTableCell();
						SlateIM::Text(TEXT("Cell 2"));
						SlateIM::NextTableCell();
						SlateIM::Text(TEXT("Cell 3"));
						SlateIM::NextTableCell();
						SlateIM::Text(TEXT("Cell 4"));
						if (SlateIM::BeginTableRowChildren())
						{
							SlateIM::NextTableCell();
							SlateIM::Text(TEXT("Cell 5"));
							SlateIM::NextTableCell();
							SlateIM::Text(TEXT("Cell 6"));
						}
						SlateIM::EndTableRowChildren();
						SlateIM::EndTable();
					}
					SlateIM::EndHorizontalStack();
				}
				
				DrawCopyCell(WidgetStyle.Key);
			}
		}
		SlateIM::EndTableRowChildren();
	}
	
	SlateIM::EndTable();
	SlateIM::EndVerticalStack();
}

void FSlateIMTestWindowWidget::DrawWindow(float DeltaTime)
{
	TestWidget.Draw();
}

#if WITH_ENGINE
void FSlateIMTestViewportWidget::DrawWidget(float DeltaTime)
{
	if (GEngine && GEngine->GameViewport)
	{
		if (SlateIM::BeginViewportRoot("SlateIMTestSuiteViewport", GEngine->GameViewport, Layout))
		{
			TestWidget.Draw();
		}
		SlateIM::EndRoot();
	}
#if WITH_EDITOR
	else if (GCurrentLevelEditingViewportClient)
	{
		TSharedPtr<SLevelViewport> LevelViewport = StaticCastSharedPtr<SLevelViewport>(GCurrentLevelEditingViewportClient->GetEditorViewportWidget());
		if (LevelViewport.IsValid())
		{
			if (SlateIM::BeginViewportRoot("SlateIMTestSuiteViewport", LevelViewport, Layout))
			{
				TestWidget.Draw();
			}
			SlateIM::EndRoot();
		}
	}
#endif
}
#endif // WITH_ENGINE
#endif // WITH_SLATEIM_EXAMPLES
