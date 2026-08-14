// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Output/CustomLaunchOutputLogMarshaller.h"
#include "Model/ProjectLauncherModel.h"
#include "Styling/AppStyle.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/ISlateLineHighlighter.h"


namespace ProjectLauncher
{
	FLaunchLogTextLayoutMarshaller::FLaunchLogTextLayoutMarshaller(const TSharedRef<ProjectLauncher::FModel>& InModel) 
		: Model(InModel)
		, TextLayout(nullptr)
	{
		MessageStyle = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("MonospacedText");

		DisplayStyle = MessageStyle;
		DisplayStyle.ColorAndOpacity = FSlateColor(FColor::Green);

		WarningStyle = MessageStyle;
		WarningStyle.ColorAndOpacity = FSlateColor(FColor::Yellow);

		ErrorStyle = MessageStyle;
		ErrorStyle.ColorAndOpacity = FSlateColor(FColor::Red);
	}


	void FLaunchLogTextLayoutMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
	{
		TextLayout = &TargetTextLayout;
		FlushPendingLogMessages();
	}


	void FLaunchLogTextLayoutMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
	{
		SourceTextLayout.GetAsText(TargetString);
	}


	void FLaunchLogTextLayoutMarshaller::MakeDirty()
	{
		FBaseTextLayoutMarshaller::MakeDirty();
		NumFilteredMessages = 0;
	}


	void FLaunchLogTextLayoutMarshaller::AddPendingLogMessage( TSharedPtr<FLaunchLogMessage> Message )
	{
		PendingMessages.Enqueue(Message);
	}


	void FLaunchLogTextLayoutMarshaller::RefreshAllLogMessages()
	{
		MakeDirty();

		for (TSharedPtr<FLaunchLogMessage> Message : Model->LaunchLogMessages)
		{
			PendingMessages.Enqueue(Message);
		}
	}


	void FLaunchLogTextLayoutMarshaller::SetFilter( ELogFilter InLogFilter )
	{
		LogFilter = InLogFilter;
		RefreshAllLogMessages();
	}


	void FLaunchLogTextLayoutMarshaller::SetFilterString( const FString& InLogFilterString )
	{
		LogFilterString = InLogFilterString;
		RefreshAllLogMessages();
	}


	bool FLaunchLogTextLayoutMarshaller::FlushPendingLogMessages()
	{
		bool bRefreshLog = false;

		TArray<FTextLayout::FNewLineData> LinesToAdd;

		TSharedPtr<FLaunchLogMessage> Message;
		while (PendingMessages.Dequeue(Message))
		{
			if (LogFilter == ELogFilter::Errors && Message->Verbosity != ELogVerbosity::Error && Message->Verbosity != ELogVerbosity::Fatal)
			{
				continue;
			}
			else if (LogFilter == ELogFilter::WarningsAndErrors && Message->Verbosity != ELogVerbosity::Warning && Message->Verbosity != ELogVerbosity::Error && Message->Verbosity != ELogVerbosity::Fatal)
			{
				continue;
			}
			else if (!LogFilterString.IsEmpty() && !Message->Message->Contains(LogFilterString))
			{
				continue;
			}

			NumFilteredMessages++;
			LinesToAdd.Emplace(MoveTemp(Message->Message), GetRun(Message));

			bRefreshLog = true;
		}

		if (bRefreshLog)
		{
			TextLayout->AddLines(LinesToAdd);
		}

		return bRefreshLog;
	}


	TArray<TSharedRef<IRun>> FLaunchLogTextLayoutMarshaller::GetRun(TSharedPtr<FLaunchLogMessage> Message) const
	{
		const FTextBlockStyle* TextStyle = &MessageStyle;
		if (Message->Verbosity == ELogVerbosity::Warning)
		{
			TextStyle = &WarningStyle;
		}
		else if (Message->Verbosity == ELogVerbosity::Error || Message->Verbosity == ELogVerbosity::Fatal)
		{
			TextStyle = &ErrorStyle;
		}
		else if (Message->Verbosity == ELogVerbosity::Display)
		{
			TextStyle = &DisplayStyle;
		}

		TArray<TSharedRef<IRun>> Runs;
		Runs.Add(FSlateTextRun::Create(FRunInfo(), Message->Message, *TextStyle));

		return MoveTemp(Runs);
	}
}
