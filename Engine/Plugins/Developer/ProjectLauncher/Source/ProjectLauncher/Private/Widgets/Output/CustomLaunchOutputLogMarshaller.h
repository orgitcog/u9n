// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/SpscQueue.h"
#include "Templates/SharedPointer.h"
#include "Styling/SlateTypes.h"
#include "Framework/Text/TextLayout.h"
#include "Framework/Text/BaseTextLayoutMarshaller.h"


namespace ProjectLauncher
{
	class FModel;
	struct FLaunchLogMessage;

	enum class ELogFilter : uint8
	{
		All,
		WarningsAndErrors,
		Errors,
	};


	// this class is responsible for marshalling the output log from FModel into an associated multiline text box
	class FLaunchLogTextLayoutMarshaller : public FBaseTextLayoutMarshaller
	{
	public:
		FLaunchLogTextLayoutMarshaller(const TSharedRef<ProjectLauncher::FModel>& InModel);
		virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
		virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;
		virtual void MakeDirty() override;

		ELogFilter GetFilter() const { return LogFilter; }
		void SetFilter( ELogFilter InLogFilter );

		FString GetFilterString() const { return LogFilterString; }
		void SetFilterString( const FString& InLogFilterString );

		void AddPendingLogMessage( TSharedPtr<FLaunchLogMessage> Message );
		bool FlushPendingLogMessages();

		void RefreshAllLogMessages();
		int32 GetNumFilteredMessages() const { return NumFilteredMessages; }
	protected:
		TArray<TSharedRef<class IRun>> GetRun(TSharedPtr<FLaunchLogMessage> Message) const;

		FTextBlockStyle MessageStyle;
		FTextBlockStyle DisplayStyle;
		FTextBlockStyle WarningStyle;
		FTextBlockStyle ErrorStyle;

		const TSharedRef<ProjectLauncher::FModel> Model;
		TSpscQueue<TSharedPtr<FLaunchLogMessage>> PendingMessages;
		ELogFilter LogFilter = ELogFilter::All;
		FString LogFilterString;
		FTextLayout* TextLayout;
		int32 NumFilteredMessages = 0;

	};
}
