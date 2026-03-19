// Copyright Epic Games, Inc. All Rights Reserved.
#include "SoundDashboardCommands.h"

#include "AudioInsightsStyle.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	FSoundDashboardCommands::FSoundDashboardCommands()
		: TCommands<FSoundDashboardCommands>("SoundDashboardCommands", LOCTEXT("SoundDashboardCommands_ContextDescText", "Sound Dashboard Commands"), NAME_None, FSlateStyle::GetStyleName())
	{

	}
	
	void FSoundDashboardCommands::RegisterCommands()
	{
		UI_COMMAND(Pin, "Pin", "Pins the selected sound in the Pinned category.", EUserInterfaceActionType::Button, FInputChord(EKeys::P, EModifierKey::Control));
		UI_COMMAND(Unpin, "Unpin", "Removes the selected sound from the Pinned category.", EUserInterfaceActionType::Button, FInputChord(EKeys::U, EModifierKey::Control));
		UI_COMMAND(Browse, "Browse To Asset", "Browses to the selected sound asset in the content browser.", EUserInterfaceActionType::Button, FInputChord(EKeys::B, EModifierKey::Control));
		UI_COMMAND(Edit, "Edit", "Opens the selected sound for edit.", EUserInterfaceActionType::Button, FInputChord(EKeys::E, EModifierKey::Control));
		// @TODO UE-250399: Hide category pending to implement
		//UI_COMMAND(Hide, "Hide", "Hides the selected sound.", EUserInterfaceActionType::Button, FInputChord(EKeys::H, EModifierKey::Control));
	}
} // namespace UE::Audio::Insights

#undef LOCTEXT_NAMESPACE
