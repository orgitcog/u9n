// Copyright Epic Games, Inc. All Rights Reserved.


#include "SlateIMWidgetBase.h"

#include "Framework/Application/SlateApplication.h"
#include "SlateIM.h"

FSlateIMWidgetBase::FSlateIMWidgetBase(const FStringView& Name)
	: WidgetName(Name)
{
}

FSlateIMWidgetBase::~FSlateIMWidgetBase()
{
	if (FSlateApplication::IsInitialized())
	{
		DisableWidget();
	}
}

void FSlateIMWidgetBase::EnableWidget()
{
	if (!TickHandle.IsValid())
	{
		TickHandle = FSlateApplication::Get().OnPreTick().AddRaw(this, &FSlateIMWidgetBase::DrawWidget);
	}
}

void FSlateIMWidgetBase::DisableWidget()
{
	if (TickHandle.IsValid())
	{
		FSlateApplication::Get().OnPreTick().Remove(TickHandle);
		TickHandle.Reset();
	}
}

FSlateIMWidgetWithCommandBase::FSlateIMWidgetWithCommandBase(const TCHAR* Command, const TCHAR* CommandHelp)
	: FSlateIMWidgetBase(Command)
	, WidgetCommand(Command, CommandHelp, FConsoleCommandDelegate::CreateRaw(this, &FSlateIMWidgetWithCommandBase::ToggleWidget))
{}

void FSlateIMWindowBase::DrawWidget(float DeltaTime)
{
	if (!SlateIM::CanUpdateSlateIM())
	{
		return;
	}
	
	const bool bIsDrawingWindow = SlateIM::BeginWindowRoot(GetWidgetName(), WindowTitle, WindowSize);
	if (bIsDrawingWindow)
	{
		DrawWindow(DeltaTime);
	}
	SlateIM::EndRoot();

	if (!bIsDrawingWindow)
	{
		DisableWidget();
	}
}

TSharedRef<SWidget> FSlateIMExposedBase::GetExposedWidget() const
{
	return ExposedWidget.IsValid() ? ExposedWidget.ToSharedRef() : SNullWidget::NullWidget;
}

void FSlateIMExposedBase::DrawWidget(float DeltaTime)
{
	TSharedPtr<SWidget> NewExposedWidget;
	if (SlateIM::BeginExposedRoot(GetWidgetName(), NewExposedWidget))
	{
		DrawContent(DeltaTime);
	}
	SlateIM::EndRoot();

	if (NewExposedWidget != ExposedWidget)
	{
		ExposedWidget = NewExposedWidget;
		OnExposedWidgetChanged.Broadcast(GetExposedWidget());
	}
}
