// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ControlRigViewportToolbarExtensions.h"

#include "ControlRigEditorCommands.h"
#include "DetailCategoryBuilder.h"
#include "EditorViewportCommands.h"
#include "EditMode/ControlRigEditModeSettings.h"
#include "ToolMenus.h"
#include "Tools/MotionTrailOptions.h"
#include "Tools/SMotionTrailOptions.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "MovieSceneCommonHelpers.h"
#include "Widgets/Input/SComboBox.h"
#include "LevelEditor.h"
#include "SSocketChooser.h"
#include "LevelEditorActions.h"
#include "ActorPickerMode.h"
#include "EditMode/SComponentPickerPopup.h"
#include "Components/SkeletalMeshComponent.h"
#include "ILevelEditor.h"
#include "LevelEditor.h"
#include "InteractiveToolManager.h"
#include "EditorModeManager.h"
#include "SEditorViewport.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"

#define LOCTEXT_NAMESPACE "ControlRigViewportToolbar"

namespace UE::ControlRig::Private
{

const FName ControlRigOwnerName = "ControlRigViewportToolbar";

} // namespace UE::ControlRig::Private


void OffsetActionExecuteAction(const FToolMenuContext& InContext, UMotionTrailToolOptions* Settings, int32 Index)
{
	if (UMotionTrailToolOptions::FPinnedTrail* Trail = Settings->GetPinnedTrail(Index))
	{
		Settings->SetHasOffset(Index, !Trail->bHasOffset);
	}
}

ECheckBoxState OffsetActionGetActionCheckState(const FToolMenuContext& Context, UMotionTrailToolOptions* Settings, int32 Index)
{
	if (UMotionTrailToolOptions::FPinnedTrail* Trail = Settings->GetPinnedTrail(Index))
	{
		return (Trail->bHasOffset) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void SpaceActionExecuteAction(const FToolMenuContext& InContext, UMotionTrailToolOptions* Settings, int32 Index)
{
	if (UMotionTrailToolOptions::FPinnedTrail* Trail = Settings->GetPinnedTrail(Index))
	{
		if (Trail->SpaceName.IsSet() == false)
		{
			// FIXME temp approach for selecting the parent
			FSlateApplication::Get().DismissAllMenus();

			static const FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

			ActorPickerMode.BeginActorPickingMode(
				FOnGetAllowedClasses(),
				FOnShouldFilterActor::CreateLambda([](const AActor* InActor)
					{
						return true; //todo make sure in sequencer
					}),
				FOnActorSelected::CreateLambda([Settings, Index](AActor* InActor)
					{
						FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
						TSharedPtr< ILevelEditor > LevelEditor = LevelEditorModule.GetFirstLevelEditor();

						TSharedPtr<SWidget> MenuWidget = SNew(SComponentPickerPopup)
							.Actor(InActor)
							.bCheckForSockets(false)
							.OnComponentChosen_Lambda([Settings, Index, InActor](FName InComponentName) mutable
								{
									Settings->PutPinnnedInSpace(Index, InActor, InComponentName);
								}
							);
						// Create as context menu
						FSlateApplication::Get().PushMenu(
							LevelEditor.ToSharedRef(),
							FWidgetPath(),
							MenuWidget.ToSharedRef(),
							FSlateApplication::Get().GetCursorPos(),
							FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
						);

					})

			);
		}
		else
		{
			Settings->PutPinnnedInSpace(Index, nullptr, NAME_None);
		}
	}
}

ECheckBoxState SpaceActionGetActionCheckState(const FToolMenuContext& Context, UMotionTrailToolOptions* Settings, int32 Index)
{
	if (UMotionTrailToolOptions::FPinnedTrail* Trail = Settings->GetPinnedTrail(Index))
	{
		return (Trail->SpaceName.IsSet()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void CreatePinnedMenuDelegate(UToolMenu* SubMenu, UMotionTrailToolOptions* Settings, int32 Index)
{
	FToolMenuSection& Section = SubMenu->AddSection(NAME_None);

	FToolUIAction OffsetAction;
	OffsetAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&OffsetActionExecuteAction, Settings, Index);
	OffsetAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&OffsetActionGetActionCheckState, Settings, Index);

	FToolMenuEntry OffsetEntry = FToolMenuEntry::InitMenuEntry(
		"Offset",
		LOCTEXT("OffsetLabel", "Offset"),
		LOCTEXT("OffsetLabelTooltip", "Toggle offset on selects the curve in the viewport, and allows you to move it like shift select does. Toggling it off will remove any offset."),
		FSlateIcon(),
		OffsetAction,
		EUserInterfaceActionType::ToggleButton
	);
	Section.AddEntry(OffsetEntry);

	FToolUIAction SpaceAction;
	SpaceAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&SpaceActionExecuteAction, Settings, Index);
	SpaceAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&SpaceActionGetActionCheckState, Settings, Index);

	FToolMenuEntry SpaceEntry = FToolMenuEntry::InitMenuEntry(
		"Space",
		LOCTEXT("SpaceLabel", "Space"),
		LOCTEXT("SpaceLabelTooltip", "Toggling on space will put you into eye drop selection mode to pick the scene compponent/socket that you want to have this trail in. Toggling it off puts it back in world space."),
		FSlateIcon(),
		SpaceAction,
		EUserInterfaceActionType::ToggleButton
	);
	Section.AddEntry(SpaceEntry);
}

void CreatePinnedItems(UMotionTrailToolOptions* Settings, FToolMenuSection& PinnedTrails)
{
	const int32  NumPinned = Settings->GetNumPinned();
	if (NumPinned > 0)
	{
		for (int32 Index = 0; Index < NumPinned; ++Index)
		{
			if (UMotionTrailToolOptions::FPinnedTrail* Trail = Settings->GetPinnedTrail(Index))
			{
				FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateStatic(&CreatePinnedMenuDelegate, Settings, Index);
				FUIAction TogglePinnedAction(
					FExecuteAction::CreateLambda([Settings, Index]()
						{
							if (UMotionTrailToolOptions::FPinnedTrail* Trail = Settings->GetPinnedTrail(Index))
							{
								Settings->DeletePinned(Index);
							}
						}
					),
					FCanExecuteAction()
				);

				FText Label = Trail->TrailName;
				FName Name = FName(*Label.ToString());
				FToolMenuEntry Entry = FToolMenuEntry::InitSubMenu(
					Name,
					Label,
					// TODO: Update this and other labels/tooltips in this file.
					LOCTEXT("PinnenTrailtip", "Modify Pinned States"),
					MakeMenuDelegate,
					TogglePinnedAction,
					EUserInterfaceActionType::Button,
					false,
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Pinned")
				);
				PinnedTrails.AddEntry(Entry);

			}
		}
	}
}

TSharedRef<SWidget> CreateFramesBeforeWidget(UMotionTrailToolOptions* Settings)
{
	// clang-format off
	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
						.Padding(FMargin(1.0f))
						[
							SNew(SSpinBox<int32>)
								.IsEnabled_Lambda([Settings]() { return !Settings->bShowFullTrail; })
								.MinValue(0)
								.MaxValue(100)
								.MinDesiredWidth(50)
								.ToolTipText_Lambda(
									[Settings]() -> FText
									{
										return FText::AsNumber(

											Settings->FramesBefore
										);
									}
								)
								.Value_Lambda(
									[Settings]() -> int32
									{
										return Settings->FramesBefore;
									}
								)
								.OnValueChanged_Lambda(
									[Settings](int32 InValue)
									{
										Settings->FramesBefore = InValue;
									}
								)
						]
				]
		];

	// clang-format on
}


FToolMenuEntry CreateFramesBefore(UMotionTrailToolOptions* Settings)
{
	FToolMenuEntry Entry = FToolMenuEntry::InitWidget("FramesBefore", CreateFramesBeforeWidget(Settings), LOCTEXT("FramesBefore", "Frames Before"));
	return Entry;
}

TSharedRef<SWidget> CreateFramesAfterWidget(UMotionTrailToolOptions* Settings)
{
	// clang-format off
	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
						.Padding(FMargin(1.0f))
						[
							SNew(SSpinBox<int32>)
								.IsEnabled_Lambda([Settings]() { return !Settings->bShowFullTrail; })
								.MinValue(0)
								.MaxValue(100)
								.MinDesiredWidth(50)
								.ToolTipText_Lambda(
									[Settings]() -> FText
									{
										return FText::AsNumber(

											Settings->FramesAfter
										);
									}
								)
								.Value_Lambda(
									[Settings]() -> int32
									{
										return Settings->FramesAfter;
									}
								)
								.OnValueChanged_Lambda(
									[Settings](int32 InValue)
									{
										Settings->FramesAfter = InValue;
									}
								)
						]
				]
		];

	// clang-format on
}

FToolMenuEntry CreateFramesAfter(UMotionTrailToolOptions* Settings)
{
	FToolMenuEntry Entry = FToolMenuEntry::InitWidget("FramesAfter", CreateFramesAfterWidget(Settings), LOCTEXT("FramesAfter", "Frames After"));

	return Entry;
}

TSharedRef<SWidget> CreateTrailStyleWidget(UMotionTrailToolOptions* Settings)
{
	// clang-format off
	return

		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
						.Padding(FMargin(1.0f))
						[
							SNew(SComboButton)
								.OnGetMenuContent_Lambda([Settings]()
									{

										FMenuBuilder MenuBuilder(true, NULL); //maybe todo look at settting these up with commands

										MenuBuilder.BeginSection("TrailStyles");

										TArray <TPair<FText, FText>>& TrailStyles = Settings->GetTrailStyles();
										for (int32 Index = 0; Index < TrailStyles.Num(); ++Index)
										{
											FUIAction ItemAction(FExecuteAction::CreateUObject(Settings, &UMotionTrailToolOptions::SetTrailStyle, Index));
											MenuBuilder.AddMenuEntry(TrailStyles[Index].Key, TAttribute<FText>(), FSlateIcon(), ItemAction);
										}

										MenuBuilder.EndSection();

										return MenuBuilder.MakeWidget();
									})
								.ButtonContent()
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										[
											SNew(STextBlock)
												.Text_Lambda([Settings]()
													{
														const int32 Index = Settings->GetTrailStyleIndex();
														const TArray <TPair<FText, FText>>& TrailStyles = Settings->GetTrailStyles();
														return TrailStyles[Index].Key;
													})
												.ToolTipText_Lambda([Settings]()
													{

														const int32 Index = Settings->GetTrailStyleIndex();
														const TArray <TPair<FText, FText>>& TrailStyles = Settings->GetTrailStyles();
														return TrailStyles[Index].Value;
													})
										]

								]
						]
				]
		];

	// clang-format on
}


TSharedRef<SWidget> CreateMaxNumberPinnedWidget(UMotionTrailToolOptions* Settings)
{
	// clang-format off
	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
						.Padding(FMargin(1.0f))
						[
							SNew(SSpinBox<int32>)
								.MinValue(5)
								.MaxValue(100)
								.MinDesiredWidth(50)
								.ToolTipText_Lambda(
									[Settings]() -> FText
									{
										return FText::AsNumber(
											Settings->MaxNumberPinned
										);
									}
								)
								.Value_Lambda(
									[Settings]() -> int32
									{
										return Settings->MaxNumberPinned;
									}
								)
								.OnValueChanged_Lambda(
									[Settings](int32 InValue)
									{
										Settings->MaxNumberPinned = InValue;
									}
								)
						]
				]
		];

	// clang-format on
}

FToolMenuEntry CreateMaxNumberPinned(UMotionTrailToolOptions* Settings)
{
	FToolMenuEntry Entry = FToolMenuEntry::InitWidget("MaxNumberPinned", CreateMaxNumberPinnedWidget(Settings), LOCTEXT("MaxNumberPinned", "Max Number Pinned"));
	return Entry;
}


FToolMenuEntry CreateTrailStyle(UMotionTrailToolOptions* Settings)
{
	FToolMenuEntry Entry = FToolMenuEntry::InitWidget("TrailStyle", CreateTrailStyleWidget(Settings), LOCTEXT("TrailStyle", "Trail Style"));

	return Entry;
}


void CreatePinnedSubMenu(UToolMenu* InSubMenu, UMotionTrailToolOptions* Settings)
{
	FToolMenuSection& PinnedSection = InSubMenu->AddSection("PinnedSection", LOCTEXT("PinnedSection", "Pinned"));

	FUIAction PinSelectedAction(
		FExecuteAction::CreateLambda([Settings]()
			{
				if (Settings->bShowTrails == false)
				{
					Settings->bShowTrails = true;
					FPropertyChangedEvent ShowTrailEvent(UMotionTrailToolOptions::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, bShowTrails)));
					Settings->PostEditChangeProperty(ShowTrailEvent);
				}
				Settings->PinSelection();
			}
		),
		FCanExecuteAction()
	);

	FToolMenuEntry PinSelected = FToolMenuEntry::InitMenuEntry(
		"PinSelected",
		LOCTEXT("PinSelected", "Pin Selected"),
		LOCTEXT("PinSelectedTrails", "Pin Selected Trails"),
		FSlateIcon(),
		PinSelectedAction,
		EUserInterfaceActionType::Button
	);
	PinSelected.InsertPosition.Name = PinSelected.Name;
	PinSelected.InsertPosition.Position = EToolMenuInsertType::First;
	PinnedSection.AddEntry(PinSelected);

	FUIAction SelectSocketAction(
		FExecuteAction::CreateLambda([Settings]()
			{
				// FIXME temp approach for selecting the parent
				FSlateApplication::Get().DismissAllMenus();

				static const FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

				ActorPickerMode.BeginActorPickingMode(
					FOnGetAllowedClasses(),
					FOnShouldFilterActor::CreateLambda([](const AActor* InActor)
						{
							UActorComponent* Component = InActor->GetComponentByClass(USkeletalMeshComponent::StaticClass());
							return Component != nullptr;
						}),
					FOnActorSelected::CreateLambda([Settings](AActor* InActor)
						{
							FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
							TSharedPtr< ILevelEditor > LevelEditor = LevelEditorModule.GetFirstLevelEditor();

							if (USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(InActor->GetComponentByClass(USkeletalMeshComponent::StaticClass())))
							{
								TSharedPtr<SWidget> MenuWidget =
									SNew(SSocketChooserPopup)
									.SceneComponent(Component)
									.OnSocketChosen_Lambda([Settings, Component](FName InSocketName) mutable
										{
											if (Settings->bShowTrails == false)
											{
												Settings->bShowTrails = true;
												FPropertyChangedEvent ShowTrailEvent(UMotionTrailToolOptions::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, bShowTrails)));
												Settings->PostEditChangeProperty(ShowTrailEvent);
											}
											Settings->PinComponent(Component, InSocketName);
										}
									);
								// Create as context menu
								FSlateApplication::Get().PushMenu(
									LevelEditor.ToSharedRef(),
									FWidgetPath(),
									MenuWidget.ToSharedRef(),
									FSlateApplication::Get().GetCursorPos(),
									FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
								);
							}
						})
				);
			}
		),
		FCanExecuteAction()
	);

	FToolMenuEntry SelectSocket = FToolMenuEntry::InitMenuEntry(
		"SelectSocket",
		LOCTEXT("SelectSocket", "Pin Socket"),
		LOCTEXT("SelectSocketTrails", "Pin a Skeletal Mesh Socket by selecting it"),
		FSlateIcon(),
		SelectSocketAction,
		EUserInterfaceActionType::Button
	);
	SelectSocket.InsertPosition.Name = SelectSocket.Name;


	PinnedSection.AddEntry(SelectSocket);

	FUIAction UnpinAllAction(
		FExecuteAction::CreateLambda([Settings]()
			{
				int32 NumPinned = Settings->GetNumPinned();
				for (int32 Index = NumPinned - 1; Index >= 0; --Index)
				{
					Settings->DeletePinned(Index);
				}
			}
		),
		FCanExecuteAction()
	);

	FToolMenuEntry UnpinAll = FToolMenuEntry::InitMenuEntry(
		"UnpinAll",
		LOCTEXT("UnpinAll", "Unpin All"),
		LOCTEXT("UnpinAllTrails", "Unpin All Trails"),
		FSlateIcon(),
		UnpinAllAction,
		EUserInterfaceActionType::Button
	);
	UnpinAll.InsertPosition.Name = UnpinAll.Name;



	PinnedSection.AddEntry(UnpinAll);

	//add pinned items
	if (Settings->GetNumPinned() > 0)
	{
		FToolMenuSection& PinnedTrails = InSubMenu->AddSection("PinnedTrails", LOCTEXT("PinnedTrails", "Pinned Trails"));
		CreatePinnedItems(Settings, PinnedTrails);
	}

}

TSharedRef<SWidget> CreateTrailColorWidget(UMotionTrailToolOptions* Settings, FName PropertyName)
{
	// clang-format off
	return

		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
						.Padding(FMargin(1.0f))
						[
							SNew(SColorBlock).IsEnabled(true)
								//.Size(FVector2D(6.0, 38.0))
								.Color_Lambda([Settings, PropertyName]()
									{
										FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
										FLinearColor Color = Binding.GetCurrentValue<FLinearColor>(*Settings);
										return Color;
									}
								)

								.OnMouseButtonDown_Lambda([Settings, PropertyName](const FGeometry&, const FPointerEvent&)
									{

										FColorPickerArgs PickerArgs;
										PickerArgs.bUseAlpha = false;
										FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
										FLinearColor Color = Binding.GetCurrentValue<FLinearColor>(*Settings);
										PickerArgs.InitialColor = Color;
										PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([Settings, PropertyName](FLinearColor Color)
											{
												FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
												Binding.CallFunction<FLinearColor>(*Settings, Color);
												FPropertyChangedEvent Event(Binding.GetProperty(*Settings));
												Settings->PostEditChangeProperty(Event);
											});
										OpenColorPicker(PickerArgs);
										return FReply::Handled();
									})
						]
				]
		];

	// clang-format on
}

FToolMenuEntry CreateTrailColor(UMotionTrailToolOptions* Settings, FName PropertyName)
{
	FText Text = FText::FromString(PropertyName.ToString());
	FToolMenuEntry Entry = FToolMenuEntry::InitWidget(PropertyName, CreateTrailColorWidget(Settings, PropertyName), Text);

	return Entry;
}
template<typename NumericType>
TSharedRef<SWidget> CreatePropertyWidget(UObject* Settings, FName PropertyName)
{
	// clang-format off
	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
						.Padding(FMargin(1.0f))
						[
							SNew(SSpinBox<NumericType>)
								.MinValue(0)
								.MaxValue(100)
								.MinDesiredWidth(50)
								.ToolTipText_Lambda(
									[Settings, PropertyName]() -> FText
									{
										FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
										NumericType Value = Binding.GetCurrentValue<NumericType>(*Settings);
										return FText::AsNumber(
											Value
										);
									}
								)
								.Value_Lambda(
									[Settings, PropertyName]() -> NumericType
									{
										FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
										NumericType Value = Binding.GetCurrentValue<NumericType>(*Settings);
										return Value;
									}
								)
								.OnValueChanged_Lambda(
									[Settings, PropertyName](NumericType InValue)
									{
										FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
										Binding.CallFunction<NumericType>(*Settings, InValue);
										FPropertyChangedEvent Event(Binding.GetProperty(*Settings));
										Settings->PostEditChangeProperty(Event);
									}
								)
						]
				]
		];

	// clang-format on
}

template<typename NumericType>
FToolMenuEntry CreateProperty(UObject* Settings, FName PropertyName)
{
	FText Text = FText::FromString(PropertyName.ToString());
	FToolMenuEntry Entry = FToolMenuEntry::InitWidget(PropertyName, CreatePropertyWidget<NumericType>(Settings, PropertyName), Text);

	return Entry;
}


void CreateAdvancedSubMenu(UToolMenu* InSubMenu, UMotionTrailToolOptions* Settings)
{
	FToolMenuSection& TrailSettings = InSubMenu->AddSection("TrailSettings", LOCTEXT("TrailSettings", "Trail Settings"));

	{
		FUIAction Action;

		Action.ExecuteAction = FExecuteAction::CreateLambda(
			[Settings]() -> void
			{
				Settings->bShowKeys = !Settings->bShowKeys;
				FPropertyChangedEvent Event(UMotionTrailToolOptions::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, bShowKeys)));
				Settings->PostEditChangeProperty(Event);
			}
		);
		Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
			[Settings]() -> ECheckBoxState
			{
				return Settings->bShowKeys ? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			}
		);

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
			"ShowKeys",
			LOCTEXT("ShowKeys", "Show Keys"),
			LOCTEXT("ShowKeysTooltip", "Show keys"),
			FSlateIcon(),
			Action,
			EUserInterfaceActionType::Check
		);
		TrailSettings.AddEntry(Entry);
	}

	{
		FUIAction Action;

		Action.ExecuteAction = FExecuteAction::CreateLambda(
			[Settings]() -> void
			{
				Settings->bShowMarks = !Settings->bShowMarks;
				FPropertyChangedEvent Event(UMotionTrailToolOptions::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, bShowMarks)));
				Settings->PostEditChangeProperty(Event);
			}
		);
		Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
			[Settings]() -> ECheckBoxState
			{
				return Settings->bShowMarks ? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			}
		);

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
			"ShowMarks",
			LOCTEXT("ShowMarks", "Show Marks"),
			LOCTEXT("ShowMarksTooltip", "Show Marks"),
			FSlateIcon(),
			Action,
			EUserInterfaceActionType::Check
		);
		TrailSettings.AddEntry(Entry);
	}
	{
		const FName DoubleProperty("KeySize");
		TrailSettings.AddEntry(CreateProperty<double>(Settings, DoubleProperty));
	}

	{
		const FName DoubleProperty("MarkSize");
		TrailSettings.AddEntry(CreateProperty<double>(Settings, DoubleProperty));
	}
	{
		const FName DoubleProperty("TrailThickness");
		TrailSettings.AddEntry(CreateProperty<double>(Settings, DoubleProperty));
	}

	FToolMenuSection& ColorSettings = InSubMenu->AddSection("ColorSettings", LOCTEXT("ColorSettings", "Color Settings"));
	{
		const FName ColorProperty("DefaultColor");
		ColorSettings.AddEntry(CreateTrailColor(Settings, ColorProperty));
	}
	{
		const FName ColorProperty("TimePreColor");
		ColorSettings.AddEntry(CreateTrailColor(Settings, ColorProperty));
	}
	{
		const FName ColorProperty("TimePostColor");
		ColorSettings.AddEntry(CreateTrailColor(Settings, ColorProperty));
	}
	{
		const FName ColorProperty("DashPreColor");
		ColorSettings.AddEntry(CreateTrailColor(Settings, ColorProperty));
	}
	{
		const FName ColorProperty("DashPostColor");
		ColorSettings.AddEntry(CreateTrailColor(Settings, ColorProperty));
	}
	{
		const FName ColorProperty("KeyColor");
		ColorSettings.AddEntry(CreateTrailColor(Settings, ColorProperty));
	}
	{
		const FName ColorProperty("SelectedKeyColor");
		ColorSettings.AddEntry(CreateTrailColor(Settings, ColorProperty));
	}

	FToolMenuSection& PinSettings = InSubMenu->AddSection("PinSettings", LOCTEXT("PinSettings", "Pin Settings"));
	{
		PinSettings.AddEntry(CreateMaxNumberPinned(Settings));
	}

}

void CreateMotionTrailMenu(UToolMenu* InMenu)
{
	if (!InMenu)
	{
		return;
	}

	FToolMenuSection& PathModeSection = InMenu->AddSection("PathModeSection", LOCTEXT("PathMode", "Path Mode"));
	UMotionTrailToolOptions* Settings = GetMutableDefault<UMotionTrailToolOptions>();


	//show full trail
	{
		FUIAction Action;

		Action.ExecuteAction = FExecuteAction::CreateLambda(
			[Settings]() -> void
			{
				Settings->bShowFullTrail = !Settings->bShowFullTrail;
				FPropertyChangedEvent Event(UMotionTrailToolOptions::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, bShowFullTrail)));
				Settings->PostEditChangeProperty(Event);
			}
		);
		Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
			[Settings]() -> ECheckBoxState
			{
				return Settings->bShowFullTrail ? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			}
		);

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
			"FullTrail",
			LOCTEXT("FullTrailLabel", "Full trail"),
			LOCTEXT("FullTrailTooltip", "Show full trail"),
			FSlateIcon(),
			Action,
			EUserInterfaceActionType::RadioButton
		);
		PathModeSection.AddEntry(Entry);
	}
	//set frames (oppposite of full trail basically)
	{
		FUIAction Action;

		Action.ExecuteAction = FExecuteAction::CreateLambda(
			[Settings]() -> void
			{
				Settings->bShowFullTrail = !Settings->bShowFullTrail;
				FPropertyChangedEvent Event(UMotionTrailToolOptions::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, bShowFullTrail)));
				Settings->PostEditChangeProperty(Event);
			}
		);
		Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
			[Settings]() -> ECheckBoxState
			{
				return Settings->bShowFullTrail == false ? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			}
		);

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
			"Set Frames",
			LOCTEXT("SetFramesLabel", "Set Frames"),
			LOCTEXT("SetframesTooltip", "Specify frame range"),
			FSlateIcon(),
			Action,
			EUserInterfaceActionType::RadioButton
		);
		PathModeSection.AddEntry(Entry);
	}
	PathModeSection.AddEntry(CreateFramesBefore(Settings));
	PathModeSection.AddEntry(CreateFramesAfter(Settings));
	
	CreatePinnedSubMenu(InMenu, Settings);
	/* in case design wants to go back to a subment
	FToolMenuSection& PinnedSection = InMenu->AddSection(TEXT("Pinned"), LOCTEXT("Pinned", "Pinned"));
	PinnedSection.AddSubMenu(TEXT("PinnedItems"), LOCTEXT("PinnedItems", "Pinned Items"), LOCTEXT("PinnedItems_tooltip", "Managed pinned motion trails"),
		FNewToolMenuDelegate::CreateLambda([Settings](UToolMenu* InSubMenu)
			{
				CreatePinnedSubMenu(InSubMenu, Settings);

			}));

			*/
	FToolMenuSection& PathOptionsMenu = InMenu->AddSection(TEXT("PathOptions"), LOCTEXT("PathOptions", "Path Options"));
	//show selected trails
	{
		FUIAction Action;

		Action.ExecuteAction = FExecuteAction::CreateLambda(
			[Settings]() -> void
			{
				Settings->bShowSelectedTrails = !Settings->bShowSelectedTrails;
				FPropertyChangedEvent Event(UMotionTrailToolOptions::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, bShowSelectedTrails)));
				Settings->PostEditChangeProperty(Event);
			}
		);
		Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
			[Settings]() -> ECheckBoxState
			{
				return Settings->bShowSelectedTrails ? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			}
		);

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
			"ShowSelectedtrails",
			LOCTEXT("ShowSelectedtrailsLabel", "Show Trails On Selection"),
			LOCTEXT("ShowSelectedtrailsLabelTooltip", "Show trails on selected sequencer items."),
			FSlateIcon(),
			Action,
			EUserInterfaceActionType::ToggleButton
		);
		PathOptionsMenu.AddEntry(Entry);

	}
	PathOptionsMenu.AddEntry(CreateTrailStyle(Settings));

	PathOptionsMenu.AddSubMenu(TEXT("Advanced"), LOCTEXT("Advanced", "Advanced"), LOCTEXT("Advanced_tooltip", "Advanced options"),
		FNewToolMenuDelegate::CreateLambda([Settings](UToolMenu* InSubMenu)
			{
				CreateAdvancedSubMenu(InSubMenu, Settings);

			}));

}
void UE::ControlRig::PopulateControlRigViewportToolbarTransformSubmenu(const FName InMenuName)
{
	FToolMenuOwnerScoped ScopeOwner(UE::ControlRig::Private::ControlRigOwnerName);

	UToolMenu* const Menu = UToolMenus::Get()->ExtendMenu(InMenuName);

	{
		FToolMenuSection& GizmoSection = Menu->FindOrAddSection("Gizmo");

		UControlRigEditModeSettings* const ViewportSettings = GetMutableDefault<UControlRigEditModeSettings>();

		// Add "Local Transforms in Each Local Space" checkbox.
		{
			FUIAction Action;

			Action.ExecuteAction = FExecuteAction::CreateLambda(
				[ViewportSettings]() -> void
				{
					ViewportSettings->bLocalTransformsInEachLocalSpace = !ViewportSettings->bLocalTransformsInEachLocalSpace;
					ViewportSettings->PostEditChange();
				}
			);
			Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
				[ViewportSettings]() -> ECheckBoxState
				{
					return ViewportSettings->bLocalTransformsInEachLocalSpace ? ECheckBoxState::Checked
																			  : ECheckBoxState::Unchecked;
				}
			);

			FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
				"LocalTransformsInEachLocalSpace",
				LOCTEXT("LocalTransformsInEachLocalSpaceLabel", "Local Transforms in Each Local Space"),
				LOCTEXT(
					"LocalTransformsInEachLocalSpaceTooltip", "When multiple objects are selected, whether or not to transform each invidual object along its own local transform space."
				),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LocalTransformsInEachLocalSpace"),
				Action,
				EUserInterfaceActionType::ToggleButton
			);
			// We want to appear early in the section.
			Entry.InsertPosition.Position = EToolMenuInsertType::First;
			GizmoSection.AddEntry(Entry);
		}

		// Add "Restore Coordinate Space on Switch" checkbox.
		{
			FUIAction Action;

			Action.ExecuteAction = FExecuteAction::CreateLambda(
				[ViewportSettings]() -> void
				{
					ViewportSettings->bCoordSystemPerWidgetMode = !ViewportSettings->bCoordSystemPerWidgetMode;
					ViewportSettings->PostEditChange();
				}
			);
			Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
				[ViewportSettings]() -> ECheckBoxState
				{
					return ViewportSettings->bCoordSystemPerWidgetMode ? ECheckBoxState::Checked
																	   : ECheckBoxState::Unchecked;
				}
			);

			FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
				"RestoreCoordinateSpaceOnSwitch",
				LOCTEXT("RestoreCoordinateSpaceOnSwitchLabel", "Restore Coordinate Space on Switch"),

				LOCTEXT(
					"RestoreCoordinateSpaceOnSwitchTooltip",
					"Whether to restore the coordinate space when changing Widget Modes in the Viewport."
				),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.RestoreCoordinateSpaceOnSwitch"),
				Action,
				EUserInterfaceActionType::ToggleButton
			);
			// We want to appear early in the section.
			Entry.InsertPosition.Position = EToolMenuInsertType::First;
			GizmoSection.AddEntry(Entry);
		}
	}
	{

		FToolMenuSection& PreviewToolsSection  = Menu->FindOrAddSection("PreviewTools", LOCTEXT("PreviewToolsLabel", "Preview Tools"));
		{
			// Add "Temporary Pivot" checkbox.
			{
				FUIAction Action;

				Action.ExecuteAction = FExecuteAction::CreateLambda(
					[]() -> void
					{
						if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
						{
							TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin();
							if (LevelEditorPtr.IsValid())
							{
								FString ActiveToolName = LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->GetActiveToolName(EToolSide::Left);							
								if (ActiveToolName == TEXT("SequencerPivotTool"))
								{
									LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->DeactivateTool(EToolSide::Left, EToolShutdownType::Completed);
								}
								else
								{
									LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->SelectActiveToolType(EToolSide::Left, TEXT("SequencerPivotTool"));
									LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->ActivateTool(EToolSide::Left);
								}
							}
						}
					}
				);
				Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
					[]() -> ECheckBoxState
					{
						if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
						{
							TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule->GetLevelEditorInstance().Pin();
							if (LevelEditorPtr.IsValid())
							{
								FString ActiveToolName = LevelEditorPtr->GetEditorModeManager().GetInteractiveToolsContext()->ToolManager->GetActiveToolName(EToolSide::Left);
								if (ActiveToolName == TEXT("SequencerPivotTool"))
								{
									return ECheckBoxState::Checked;
								}
							}
						}
						return  ECheckBoxState::Unchecked;
					}
				);

				FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
					"TemporaryPivot",
					LOCTEXT("TemporaryPivotLabel", "Temporary Pivot"),
					LOCTEXT(
						"TemporaryPivotTooltip",
						"Toggle Temporary Pivot Tool"
					),
					FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.TemporaryPivot")),
					Action,
					EUserInterfaceActionType::ToggleButton
				);
				Entry.SetShowInToolbarTopLevel(true);
				PreviewToolsSection.AddEntry(Entry);
			}
		}
		
		FNewToolMenuDelegate MakeMenuDelegate = FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* Submenu)
			{
				UMotionTrailToolOptions* Settings = GetMutableDefault<UMotionTrailToolOptions>();
				{
					CreateMotionTrailMenu(Submenu);
				}

			}
		);

		UMotionTrailToolOptions* Settings = GetMutableDefault<UMotionTrailToolOptions>();

		// Create the checkbox actions for the MotionPaths submenu itself.
		FToolUIAction CheckboxMenuAction;
		{
			CheckboxMenuAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda(
				[Settings](const FToolMenuContext& Context) -> void
				{
					//if trails are already shown and we do a modifier we leave them on but do a specific action
					const bool bAltDown = FSlateApplication::Get().GetModifierKeys().IsAltDown();
					const bool bControlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
					const bool bShiftDown = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
					const bool bModifierDown = bAltDown || bControlDown || bShiftDown;
					auto HandleModifier = [Settings, bAltDown, bControlDown, bShiftDown]() {

						if (bAltDown)
						{
							Settings->UnPinSelection();
							return;
						}
						if (bControlDown)
						{
							Settings->DeleteAllPinned();
							Settings->PinSelection();
							return;
						}
						if (bShiftDown)
						{
							Settings->PinSelection();
							return;
						}
					};
					//if we are on and we have a modifier we do the operations then bail out leaving them on
					if (Settings->bShowTrails && bModifierDown)
					{
						HandleModifier();
						return;
					}
					Settings->bShowTrails = !Settings->bShowTrails;
					FPropertyChangedEvent ShowTrailEvent(UMotionTrailToolOptions::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, bShowTrails)));
					Settings->PostEditChangeProperty(ShowTrailEvent);
					//if we are now on and we 1) Pin if no pinned trails  or 2) handle the modifier if there are pinned
					if (Settings->bShowTrails)
					{
						if (Settings->GetNumPinned() == 0)
						{
							Settings->PinSelection();
						}
						else if (bModifierDown)
						{
							HandleModifier();
						}
					}
				}
			);
			CheckboxMenuAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda(
				[Settings](const FToolMenuContext& Context) -> ECheckBoxState
				{
					if (Settings->bShowTrails)
					{
						return ECheckBoxState::Checked;
					}
					else
					{
						return ECheckBoxState::Unchecked;
					}
				}
			);
		}

		FToolMenuEntry MotionPathsSubmenu = FToolMenuEntry::InitSubMenu(
			"MotionPaths",
			LOCTEXT("MotionPathsLabel", "Motion Paths"),
			// TODO: Update this and other labels/tooltips in this file.
			LOCTEXT("MotionPathsTooltip", "Check to enable motion paths. Submenu contains settings for motion paths.\nHotkeys:\nUse SHIFT to add selected items to pin list\nUse CTRL to reset pin list to just the selected item\nUse ALT to remove selected item from pin list"),
			MakeMenuDelegate,
			CheckboxMenuAction,
			EUserInterfaceActionType::ToggleButton,
			false,
			FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.EditableMotionTrails"))
		);
		MotionPathsSubmenu.SetShowInToolbarTopLevel(true);

		PreviewToolsSection.AddEntry(MotionPathsSubmenu);
	}
	{
		FToolMenuSection& SelectionSection = Menu->FindOrAddSection("Selection");

		// Add "Select Only Control Rig Controls" entry.
		{
			UControlRigEditModeSettings* const Settings = GetMutableDefault<UControlRigEditModeSettings>();

			FUIAction Action;
			Action.ExecuteAction = FExecuteAction::CreateLambda(
				[Settings]() -> void
				{
					Settings->bOnlySelectRigControls = !Settings->bOnlySelectRigControls;
					Settings->PostEditChange();
				}
			);
			Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
				[Settings]() -> ECheckBoxState
				{
					return Settings->bOnlySelectRigControls ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}
			);

			FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
				"OnlySelectRigControls",
				LOCTEXT("OnlySelectRigControlsLabel", "Select Only Control Rig Controls"),
				LOCTEXT("OnlySelectRigControlsTooltip", "Whether or not only Rig Controls can be selected."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.OnlySelectRigControls"),
				Action,
				EUserInterfaceActionType::ToggleButton
			);
			// We want to appear late in the section.
			Entry.InsertPosition.Position = EToolMenuInsertType::Last;
			Entry.SetShowInToolbarTopLevel(true);
			SelectionSection.AddEntry(Entry);
		}
	}
}

void CreateAxisOnSelectionMenu(UToolMenu* AnimationShowFlagsSubmenu, FToolMenuSection& UnnamedSection, UControlRigEditModeSettings* Settings)
{
	UUnrealEdViewportToolbarContext* Context = AnimationShowFlagsSubmenu->FindContext<UUnrealEdViewportToolbarContext>();

	FNewToolMenuDelegate AxisOnSelectionMenuDelegate = FNewToolMenuDelegate::CreateLambda(
		[Settings](UToolMenu* Submenu)
		{
			FToolMenuSection& AxisOnSelection =
				Submenu->FindOrAddSection("AxisOnSelection", LOCTEXT("AxisOnSelectionLabel", "Axis On Selection"));

			const FName DoubleProperty("AxisScale");
			FToolMenuEntry NumericEntry = CreateProperty<float>(Settings, DoubleProperty);

			AxisOnSelection.AddEntry(NumericEntry);
		}
	);

	FToolMenuEntry Entry = FToolMenuEntry::InitSubMenu(
		"AxisOnSelection",
		LOCTEXT("AxisOnSelectionLabel", "Axis On Selection"),
		LOCTEXT("AxisOnSelectionTooltip", "Should we show axes for the selected elements"),
		AxisOnSelectionMenuDelegate,
		FToolUIAction(
			FToolMenuExecuteAction::CreateLambda([Settings,Context](const FToolMenuContext& InContext)
			{
				Settings->bDisplayAxesOnSelection = !Settings->bDisplayAxesOnSelection;
				UControlRigEditModeSettings::OnSettingsChange.Broadcast(Settings);
				if (Context)
				{
					Context->RefreshViewport();
				}
			}),
			FToolMenuGetActionCheckState::CreateLambda([Settings](const FToolMenuContext& InContext)
			{
				return Settings->bDisplayAxesOnSelection ? ECheckBoxState::Checked
								: ECheckBoxState::Unchecked;
			})
		),
		EUserInterfaceActionType::ToggleButton
	);

	UnnamedSection.AddEntry(Entry);
	
}

void UE::ControlRig::PopulateControlRigViewportToolbarShowSubmenu(const FName InMenuName)
{
	FToolMenuOwnerScoped ScopeOwner(UE::ControlRig::Private::ControlRigOwnerName);

	UToolMenu* const Menu = UToolMenus::Get()->ExtendMenu(InMenuName);
	FToolMenuSection& AllShowFlagsSection = Menu->FindOrAddSection("AllShowFlags");

	FToolMenuEntry AnimationSubmenu = FToolMenuEntry::InitSubMenu(
		"Animation",
		LOCTEXT("AnimationLabel", "Animation"),
		LOCTEXT("AnimationTooltip", "Animation-related show flags"),
		FNewToolMenuDelegate::CreateLambda(
			[](UToolMenu* AnimationShowFlagsSubmenu)
			{
				FToolMenuSection& UnnamedSection = AnimationShowFlagsSubmenu->FindOrAddSection(NAME_None);
				
				UUnrealEdViewportToolbarContext* Context = AnimationShowFlagsSubmenu->FindContext<UUnrealEdViewportToolbarContext>();
				if (UControlRigEditModeSettings* const Settings = GetMutableDefault<UControlRigEditModeSettings>())
				{
					{
						CreateAxisOnSelectionMenu(AnimationShowFlagsSubmenu, UnnamedSection, Settings);
					}
					{
						FUIAction Action;
						Action.ExecuteAction = FExecuteAction::CreateLambda(
							[Settings, Context]() -> void
							{
								Settings->bDisplayHierarchy = !Settings->bDisplayHierarchy;
								UControlRigEditModeSettings::OnSettingsChange.Broadcast(Settings);
								if (Context)
								{
									Context->RefreshViewport();
								}
							}
						);
						Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
							[Settings]() -> ECheckBoxState
							{
								return Settings->bDisplayHierarchy ? ECheckBoxState::Checked
									: ECheckBoxState::Unchecked;
							}
						);

						FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
							"Hierarchy",
							LOCTEXT("HierarchyLabel", "Hierarchy"),
							LOCTEXT("HierarchyTooltip", "Whether to show all bones in the hierarchy"),
							FSlateIcon(),
							Action,
							EUserInterfaceActionType::ToggleButton
						);
						UnnamedSection.AddEntry(Entry);
					}
					{
						FUIAction Action;
						Action.ExecuteAction = FExecuteAction::CreateLambda(
							[Settings, Context]() -> void
							{
								Settings->bShowControlsAsOverlay = !Settings->bShowControlsAsOverlay;
								UControlRigEditModeSettings::OnSettingsChange.Broadcast(Settings);
								if (Context)
								{
									Context->RefreshViewport();
								}
							}
						);
						Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
							[Settings]() -> ECheckBoxState
							{
								return Settings->bShowControlsAsOverlay ? ECheckBoxState::Checked
									: ECheckBoxState::Unchecked;
							}
						);

						FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
							"ControlsAsOverlay",
							LOCTEXT("ControlsAsOverlayLabel", "Controls As Overlay"),
							LOCTEXT("ControlsAsOverlaylTooltip", "Whether to show controls as overlay"),
							FSlateIcon(),
							Action,
							EUserInterfaceActionType::ToggleButton
						);
						UnnamedSection.AddEntry(Entry);
					}
					{
						FUIAction Action;
						Action.ExecuteAction = FExecuteAction::CreateLambda(
							[Settings, Context]() -> void
							{
								Settings->bHideControlShapes = !Settings->bHideControlShapes;
								UControlRigEditModeSettings::OnSettingsChange.Broadcast(Settings);
								if (Context)
								{
									Context->RefreshViewport();
								}
							}
						);
						Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
							[Settings]() -> ECheckBoxState
							{
								return Settings->bHideControlShapes == false ? ECheckBoxState::Checked
									: ECheckBoxState::Unchecked;
							}
						);

						FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
							"ControlShapes",
							LOCTEXT("ControlShapesLabel", "Control Shapes"),
							LOCTEXT("ControlShapesTooltip", "Should we always hide control shapes in viewport"),
							FSlateIcon(),
							Action,
							EUserInterfaceActionType::ToggleButton
						);
						UnnamedSection.AddEntry(Entry);
					}
					{
						FUIAction Action;
						Action.ExecuteAction = FExecuteAction::CreateLambda(
							[Settings, Context]() -> void
							{
								Settings->bDisplayNulls = !Settings->bDisplayNulls;
								UControlRigEditModeSettings::OnSettingsChange.Broadcast(Settings);
								if (Context)
								{
									Context->RefreshViewport();
								}
							}
						);
						Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
							[Settings]() -> ECheckBoxState
							{
								return Settings->bDisplayNulls ? ECheckBoxState::Checked
									: ECheckBoxState::Unchecked;
							}
						);

						FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
							"Nulls",
							LOCTEXT("NullsLabel", "Nulls"),
							LOCTEXT("NullTooltip", "Whether to show all nulls in the hierarchy"),
							FSlateIcon(),
							Action,
							EUserInterfaceActionType::ToggleButton
						);
						UnnamedSection.AddEntry(Entry);
					}
					{
						FUIAction Action;
						Action.ExecuteAction = FExecuteAction::CreateLambda(
							[Settings, Context]() -> void
							{
								Settings->bShowAllProxyControls = !Settings->bShowAllProxyControls;
								UControlRigEditModeSettings::OnSettingsChange.Broadcast(Settings);
								if (Context)
								{
									Context->RefreshViewport();
								}
							}
						);
						Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
							[Settings]() -> ECheckBoxState
							{
								return Settings->bShowAllProxyControls ? ECheckBoxState::Checked
									: ECheckBoxState::Unchecked;
							}
						);

						FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
							"ProxyControls",
							LOCTEXT("ProxyControlsLabel", "Proxy Controls"),
							LOCTEXT("ProxyControlsTooltip", "Whether to show Proxy Controls"),
							FSlateIcon(),
							Action,
							EUserInterfaceActionType::ToggleButton
						);
						UnnamedSection.AddEntry(Entry);
					}
					{
						FUIAction Action;
						Action.ExecuteAction = FExecuteAction::CreateLambda(
							[Settings, Context]() -> void
							{
								Settings->bDisplaySockets = !Settings->bDisplaySockets;
								UControlRigEditModeSettings::OnSettingsChange.Broadcast(Settings);
								if (Context)
								{
									Context->RefreshViewport();
								}
							}
						);
						Action.GetActionCheckState = FGetActionCheckState::CreateLambda(
							[Settings]() -> ECheckBoxState
							{
								return Settings->bDisplaySockets ? ECheckBoxState::Checked
									: ECheckBoxState::Unchecked;
							}
						);

						FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
							"Sockets",
							LOCTEXT("SocketsLabel", "Sockets"),
							LOCTEXT("SocketsTooltip", "Whether to show Sockets"),
							FSlateIcon(),
							Action,
							EUserInterfaceActionType::ToggleButton
						);
						UnnamedSection.AddEntry(Entry);
					}
				}
			}
		),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Animation_16x")
	);
	// Show this in the top-level to highlight it for Animation Mode users.
	AnimationSubmenu.SetShowInToolbarTopLevel(true);
	AnimationSubmenu.InsertPosition.Position = EToolMenuInsertType::First;
	AllShowFlagsSection.AddEntry(AnimationSubmenu);
}

void UE::ControlRig::RemoveControlRigViewportToolbarExtensions()
{
	UToolMenus::Get()->UnregisterOwnerByName(UE::ControlRig::Private::ControlRigOwnerName);
}

#undef LOCTEXT_NAMESPACE
