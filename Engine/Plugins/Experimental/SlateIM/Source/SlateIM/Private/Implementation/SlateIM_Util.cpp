// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/MessageDialog.h"
#include "SlateIM.h"

#include "Misc/SlateIMManager.h"

namespace SlateIM
{
	bool CanUpdateSlateIM()
	{
		return FSlateIMManager::Get().CanUpdateSlateIM();
	}
	
	void BeginDisabledState()
	{
		FSlateIMManager::Get().GetMutableCurrentRoot().SetDisabledState();
	}

	void EndDisabledState()
	{
		FSlateIMManager::Get().GetMutableCurrentRoot().SetEnabledState();
	}

	void SetToolTip(const FStringView& NextToolTip)
	{
		FSlateIMManager::Get().GetMutableCurrentRoot().SetNextToolTip(NextToolTip);
	}

	EAppReturnType::Type ModalDialog(EAppMsgType::Type MessageType, const FStringView& DialogText, EAppMsgCategory Category, const FStringView& DialogTitle)
	{
		FSlateIMManager::Get().OnSlateIMModalOpened();

		ON_SCOPE_EXIT
		{
			FSlateIMManager::Get().OnSlateIMModalClosed();
		};
		return FMessageDialog::Open(Category, MessageType, FText::FromStringView(DialogText), FText::FromStringView(DialogTitle));
	}
}
