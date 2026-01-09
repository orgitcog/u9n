// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateIM.h"

#include "Containers/SImContextMenuAnchor.h"
#include "Misc/SlateIMManager.h"
#include "Misc/SlateIMWidgetScope.h"

namespace SlateIM
{
	bool BeginContextMenuAnchor()
	{
		TSharedPtr<SImContextMenuAnchor> ContainerWidget;

		bool bMenuOpened = false;
		{
			FWidgetScope<SImContextMenuAnchor> Scope;
			ContainerWidget = Scope.GetWidget();

			if (!ContainerWidget)
			{
				ContainerWidget = SNew(SImContextMenuAnchor);

				Scope.UpdateWidget(ContainerWidget);
			}
			else
			{
				bMenuOpened = ContainerWidget->IsMenuOpen();
			}
		}


		FSlateIMManager::Get().PushContainer(FContainerNode(ContainerWidget));
		FSlateIMManager::Get().PushMenuRoot(ContainerWidget);

		return bMenuOpened;
	}

	void EndContextMenuAnchor()
	{
		FSlateIMManager::Get().PopMenuRoot();
		FSlateIMManager::Get().PopContainer<SImContextMenuAnchor>();
	}

	void AddMenuSeparator()
	{
		TSharedPtr<SImContextMenuAnchor> Anchor = FSlateIMManager::Get().GetCurrentMenuRoot();
		checkf(Anchor, TEXT("Cannot add menu items without a current active menu anchor"));

		return Anchor->AddMenuSeparator();
	}

	void AddMenuSection(const FStringView& SectionText)
	{
		TSharedPtr<SImContextMenuAnchor> Anchor = FSlateIMManager::Get().GetCurrentMenuRoot();
		checkf(Anchor, TEXT("Cannot add menu items without a current active menu anchor"));

		return Anchor->AddMenuSection(SectionText);
	}

	bool AddMenuButton(const FStringView& RowText, const FStringView& ToolTipText)
	{
		TSharedPtr<SImContextMenuAnchor> Anchor = FSlateIMManager::Get().GetCurrentMenuRoot();
		checkf(Anchor, TEXT("Cannot add menu items without a current active menu anchor"));

		return Anchor->AddMenuButton(RowText, ToolTipText);
	}

	bool AddMenuToggleButton(const FStringView& RowText, bool& InOutCurrentState, const FStringView& ToolTipText)
	{
		TSharedPtr<SImContextMenuAnchor> Anchor = FSlateIMManager::Get().GetCurrentMenuRoot();
		checkf(Anchor, TEXT("Cannot add menu items without a current active menu anchor"));

		return Anchor->AddMenuToggleButton(RowText, InOutCurrentState, ToolTipText);
	}

	bool AddMenuCheckButton(const FStringView& RowText, bool& InOutCurrentState, const FStringView& ToolTipText)
	{
		TSharedPtr<SImContextMenuAnchor> Anchor = FSlateIMManager::Get().GetCurrentMenuRoot();
		checkf(Anchor, TEXT("Cannot add menu items without a current active menu anchor"));

		return Anchor->AddMenuCheckButton(RowText, InOutCurrentState, ToolTipText);
	}

	void BeginSubMenu(const FStringView& SubMenuText)
	{
		TSharedPtr<SImContextMenuAnchor> Anchor = FSlateIMManager::Get().GetCurrentMenuRoot();
		checkf(Anchor, TEXT("Cannot add menu items without a current active menu anchor"));

		Anchor->BeginSubMenu(SubMenuText);
	}

	void EndSubMenu()
	{

	}
}
