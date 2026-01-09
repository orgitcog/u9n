// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FUICommandInfo;

namespace UE::Audio::Insights
{
	class FSoundDashboardCommands : public TCommands<FSoundDashboardCommands>
	{
	public:
		FSoundDashboardCommands();

		virtual void RegisterCommands() override;

		TSharedPtr<const FUICommandInfo> GetPinCommand() const    { return Pin; }
		TSharedPtr<const FUICommandInfo> GetUnpinCommand() const  { return Unpin; }
		TSharedPtr<const FUICommandInfo> GetBrowseCommand() const { return Browse; }
		TSharedPtr<const FUICommandInfo> GetEditCommand() const   { return Edit; }
		// @TODO UE-250399: Hide category pending to implement
		//TSharedPtr<const FUICommandInfo> GetHideCommand() const   { return Hide; }
		
	private:
		TSharedPtr<FUICommandInfo> Pin;
		TSharedPtr<FUICommandInfo> Unpin;
		TSharedPtr<FUICommandInfo> Browse;
		TSharedPtr<FUICommandInfo> Edit;
		// @TODO UE-250399: Hide category pending to implement
		//TSharedPtr<FUICommandInfo> Hide;
	};
} // namespace UE::Audio::Insights
